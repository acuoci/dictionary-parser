/*-----------------------------------------------------------------------*\
|    ___                   ____  __  __  ___  _  _______                  |
|   / _ \ _ __   ___ _ __ / ___||  \/  |/ _ \| |/ / ____| _     _         |
|  | | | | '_ \ / _ \ '_ \\___ \| |\/| | | | | ' /|  _| _| |_ _| |_       |
|  | |_| | |_) |  __/ | | |___) | |  | | |_| | . \| |__|_   _|_   _|      |
|   \___/| .__/ \___|_| |_|____/|_|  |_|\___/|_|\_\_____||_|   |_|        |
|        |_|                                                              |
|                                                                         |
|   Author: Alberto Cuoci <alberto.cuoci@polimi.it>                       |
|   CRECK Modeling Group <https://www.creckmodeling.polimi.it>            |
|   Department of Chemistry, Materials, and Chemical Engineering          |
|   Politecnico di Milano                                                 |
|   P.zza Leonardo da Vinci 32, 20133 Milano                              |
|                                                                         |
|-------------------------------------------------------------------------|
|                                                                         |
|   This file is part of the OpenSMOKE++ library.                         |
|                                                                         |
|   Copyright (C) 2026 Alberto Cuoci                                      |
|                                                                         |
|   OpenSMOKE++ is free software: you can redistribute it and/or modify   |
|   it under the terms of the GNU General Public License as published by  |
|   the Free Software Foundation, either version 3 of the License, or     |
|   (at your option) any later version.                                   |
|                                                                         |
|   OpenSMOKE++ is distributed in the hope that it will be useful,        |
|   but WITHOUT ANY WARRANTY; without even the implied warranty of        |
|   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         |
|   GNU General Public License for more details.                          |
|                                                                         |
|   You should have received a copy of the GNU General Public License     |
|   along with OpenSMOKE++. If not, see <https://www.gnu.org/licenses/>.  |
|                                                                         |
\*-----------------------------------------------------------------------*/

#include "DictionaryUnits.h"
#include "DictionaryUtilities.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace OpenSMOKEpp::units {

auto Rational::to_string() const -> std::string {
  return den == 1 ? std::to_string(num)
                  : std::to_string(num) + "/" + std::to_string(den);
}

namespace {
constexpr double pi = 3.14159265358979323846;

// Exact scale factor `num / den * 10^e10`. `num` and `den` are integers
// whenever the definition is an exact decimal number, and powers of ten are
// kept apart in `e10`, so that products and quotients of metric units stay
// exact (`g/cm3` is 10^3, `ms` is 10^-3). `e10` becomes fractional only
// through fractional exponents (`s^0.5`).
struct Scale {
  double num = 1.0;
  double den = 1.0;
  Rational e10{0, 1};
};

// Largest double below which every integer is exactly representable.
constexpr double exact_integer_limit = 9007199254740992.0; // 2^53

[[nodiscard]] auto is_exact_integer(const double x) -> bool {
  return x >= 1.0 && x < exact_integer_limit && x == std::trunc(x);
}

// Moves the trailing zeros of integer numerators and denominators into the
// power of ten and cancels common integer factors.
void normalize(Scale &s) {
  if (is_exact_integer(s.num) && is_exact_integer(s.den)) {
    const auto g = std::gcd(static_cast<long long>(s.num),
                            static_cast<long long>(s.den));
    if (g > 1) {
      s.num /= static_cast<double>(g);
      s.den /= static_cast<double>(g);
    }
  }
  const auto pull_tens = [&s](double &x, const int sign) {
    while (is_exact_integer(x) && x >= 10.0 && std::fmod(x, 10.0) == 0.0) {
      x /= 10.0;
      s.e10 = s.e10 + Rational{sign, 1};
    }
  };
  pull_tens(s.num, 1);
  pull_tens(s.den, -1);
}

[[nodiscard]] auto multiply(const Scale &a, const Scale &b) -> Scale {
  Scale r{a.num * b.num, a.den * b.den, a.e10 + b.e10};
  normalize(r);
  return r;
}

[[nodiscard]] auto inverse(const Scale &a) -> Scale {
  return Scale{a.den, a.num, Rational{0, 1} - a.e10};
}

[[nodiscard]] auto raise(const Scale &a, const Rational &p) -> Scale {
  const Scale base = p.num < 0 ? inverse(a) : a;
  const int n = p.num < 0 ? -p.num : p.num;
  Scale r;
  if (p.den == 1) {
    // Integer power by repeated products: exact for integer factors.
    for (int i = 0; i < n; ++i)
      r = multiply(r, base);
    return r;
  }
  const double q = static_cast<double>(n) / static_cast<double>(p.den);
  r.num = std::pow(base.num, q);
  r.den = std::pow(base.den, q);
  r.e10 = base.e10 * Rational::make(n, p.den);
  normalize(r);
  return r;
}

// Exact powers of ten representable as double.
constexpr double exact_powers_of_ten[] = {
    1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
constexpr int max_exact_power_of_ten = 22;

// Unevaluated sum hi + lo (double-double), used to round a product by an
// exact factor only once.
struct TwoDouble {
  double hi;
  double lo;
};

[[nodiscard]] auto fast_two_sum(const double a, const double b) -> TwoDouble {
  const double s = a + b;
  return {s, b - (s - a)};
}

[[nodiscard]] auto times(const TwoDouble &x, const double b) -> TwoDouble {
  const double p = x.hi * b;
  const double e = std::fma(x.hi, b, -p) + x.lo * b;
  return fast_two_sum(p, e);
}

[[nodiscard]] auto over(const TwoDouble &x, const double b) -> TwoDouble {
  const double q = x.hi / b;
  const double r = std::fma(-q, b, x.hi) + x.lo;
  return fast_two_sum(q, r / b);
}

// value * num / den * 10^e10, computed in double-double and rounded once.
[[nodiscard]] auto apply_scale(const double value, const double num,
                               const double den, const int e10) -> double {
  if (value == 0.0 || !std::isfinite(value))
    return value * (num / den) * std::pow(10.0, e10);
  TwoDouble x{value, 0.0};
  if (num != 1.0)
    x = times(x, num);
  for (int e = e10; e > 0; e -= max_exact_power_of_ten)
    x = times(x, exact_powers_of_ten[std::min(e, max_exact_power_of_ten)]);
  for (int e = -e10; e > 0; e -= max_exact_power_of_ten)
    x = over(x, exact_powers_of_ten[std::min(e, max_exact_power_of_ten)]);
  if (den != 1.0)
    x = over(x, den);
  return x.hi + x.lo;
}

// Fills the exact and rounded factors of a conversion; a fractional power of
// ten is folded into the numerator.
void set_factor(UnitConversion &conversion, Scale scale) {
  const int whole = scale.e10.num >= 0 ? scale.e10.num / scale.e10.den
                                       : -((-scale.e10.num + scale.e10.den - 1) /
                                           scale.e10.den);
  const Rational fraction = scale.e10 - Rational{whole, 1};
  if (!fraction.is_zero())
    scale.num *= std::pow(10.0, static_cast<double>(fraction.num) /
                                    static_cast<double>(fraction.den));
  conversion.numerator = scale.num;
  conversion.denominator = scale.den;
  conversion.exponent10 = whole;
  conversion.factor = apply_scale(1.0, scale.num, scale.den, whole);
}

struct UnitDefinition {
  std::string_view symbol;
  Scale scale;
  Dimension dimension;
  bool prefixable;
  double offset = 0.0; // affine temperature scales only
};

// Helpers for the exact definitions below: value * 10^e, or num/den.
[[nodiscard]] auto dec(const double value, const int e = 0) -> Scale {
  return Scale{value, 1.0, Rational{e, 1}};
}
[[nodiscard]] auto ratio(const double num, const double den) -> Scale {
  return Scale{num, den, Rational{0, 1}};
}

// Symbols are case-sensitive. Exact symbols take precedence over prefixed
// readings (min = minute, Pa = pascal, cd = candela, h = hour).
// Every factor is written exactly, as an integer (ratio) times a power of
// ten, except the angles (multiples of pi).
const std::vector<UnitDefinition> &registry() {
  static const std::vector<UnitDefinition> table = {
      // SI base units (g instead of kg, so that prefixes apply)
      {"m", dec(1), dim::Length, true},
      {"g", dec(1, -3), dim::Mass, true},
      {"s", dec(1), dim::Time, true},
      {"K", dec(1), dim::Temperature, true},
      {"mol", dec(1), dim::Amount, true},
      {"A", dec(1), dim::Current, true},
      {"cd", dec(1), dim::LuminousIntensity, true},
      // SI derived units
      {"Hz", dec(1), dim::Frequency, true},
      {"N", dec(1), dim::Force, true},
      {"Pa", dec(1), dim::Pressure, true},
      {"J", dec(1), dim::Energy, true},
      {"W", dec(1), dim::Power, true},
      {"Wh", dec(3600), dim::Energy, true},
      {"L", dec(1, -3), dim::Volume, true},
      {"l", dec(1, -3), dim::Volume, true},
      {"bar", dec(1, 5), dim::Pressure, true},
      {"eV", dec(1602176634, -28), dim::Energy, true},
      {"cal", dec(4184, -3), dim::Energy, true}, // thermochemical calorie
      // Time
      {"min", dec(60), dim::Time, false},
      {"h", dec(3600), dim::Time, false},
      {"hr", dec(3600), dim::Time, false},
      // Length
      {"micron", dec(1, -6), dim::Length, false},
      {"angstrom", dec(1, -10), dim::Length, false},
      {"in", dec(254, -4), dim::Length, false},
      {"ft", dec(3048, -4), dim::Length, false},
      // Mass
      {"lb", dec(45359237, -8), dim::Mass, false},
      {"oz", dec(28349523125, -12), dim::Mass, false},
      // Pressure
      {"atm", dec(101325), dim::Pressure, false},
      {"torr", ratio(101325, 760), dim::Pressure, false},
      {"mmHg", dec(133322387415, -9), dim::Pressure, false},
      // lbf / in^2 = 4.4482216152605 N / (0.0254 m)^2
      {"psi", Scale{44482216152605, 64516, Rational{-5, 1}}, dim::Pressure,
       false},
      // Energy and force (CGS and others)
      {"erg", dec(1, -7), dim::Energy, false},
      {"dyn", dec(1, -5), dim::Force, false},
      {"BTU", dec(105505585262, -8), dim::Energy, false},
      // Temperature scales
      {"C", dec(1), dim::Temperature, false, 273.15},
      {"\xC2\xB0" "C", dec(1), dim::Temperature, false, 273.15},
      {"degC", dec(1), dim::Temperature, false, 273.15},
      {"F", ratio(5, 9), dim::Temperature, false, 459.67 * 5.0 / 9.0},
      {"\xC2\xB0" "F", ratio(5, 9), dim::Temperature, false,
       459.67 * 5.0 / 9.0},
      {"degF", ratio(5, 9), dim::Temperature, false, 459.67 * 5.0 / 9.0},
      {"R", ratio(5, 9), dim::Temperature, false},
      // Angles and rotation (radian is dimensionless)
      {"rad", dec(1), dim::Dimensionless, false},
      {"deg", ratio(pi, 180), dim::Dimensionless, false},
      {"\xC2\xB0", ratio(pi, 180), dim::Dimensionless, false},
      {"rpm", ratio(2.0 * pi, 60), dim::Frequency, false},
      // Dimensionless
      {"dimensionless", dec(1), dim::Dimensionless, false},
      {"#", dec(1), dim::Dimensionless, false},
      {"%", dec(1, -2), dim::Dimensionless, false},
      {"ppm", dec(1, -6), dim::Dimensionless, false},
  };
  return table;
}

struct Prefix {
  std::string_view symbol;
  int exponent10;
};

constexpr Prefix prefixes[] = {
    {"da", 1},  {"Y", 24}, {"Z", 21}, {"E", 18},  {"P", 15},
    {"T", 12},  {"G", 9},  {"M", 6},  {"k", 3},   {"h", 2},
    {"d", -1},  {"c", -2}, {"m", -3}, {"u", -6},  {"\xC2\xB5", -6},
    {"\xCE\xBC", -6}, {"n", -9}, {"p", -12}, {"f", -15},
    {"a", -18}};

// Frequently used prefixed symbols, offered as spelling suggestions.
constexpr std::string_view common_prefixed[] = {
    "kg", "mg", "km", "cm", "mm", "um", "nm", "ms", "kPa", "MPa",
    "kmol", "mmol", "kJ", "MJ", "kW", "kcal", "mbar", "kWh", "mL", "dm"};

struct Term {
  Scale scale{};
  Dimension dimension{};
  double offset = 0.0;  // offset of a single affine symbol
  int symbols = 0;      // number of unit symbols in the term
  bool modified = false; // exponent, prefix, number or operator applied
};

[[nodiscard]] auto suggestion_for(std::string_view symbol) -> std::string {
  std::string best;
  std::size_t best_distance = std::numeric_limits<std::size_t>::max();
  const auto consider = [&](std::string_view candidate) {
    const std::size_t d = detail::edit_distance(symbol, candidate);
    if (d < best_distance) {
      best_distance = d;
      best = std::string(candidate);
    }
  };
  for (const auto &unit : registry())
    consider(unit.symbol);
  for (const auto candidate : common_prefixed)
    consider(candidate);
  // Costs are doubled: 1 = case difference, 2 = one edit.
  const std::size_t threshold = symbol.size() <= 3 ? 2 : 4;
  return best_distance <= threshold ? best : std::string();
}

class Parser {
public:
  explicit Parser(std::string_view text) : text_(text) {}

  auto parse() -> Term {
    skip_blanks();
    if (at_end())
      fail("empty unit expression");
    Term term = expression();
    skip_blanks();
    if (!at_end())
      fail(std::string("unexpected '") + text_[pos_] + "'");
    return term;
  }

private:
  std::string_view text_;
  std::size_t pos_ = 0;

  [[nodiscard]] auto at_end() const -> bool { return pos_ >= text_.size(); }
  [[nodiscard]] auto peek(std::size_t ahead = 0) const -> char {
    return pos_ + ahead < text_.size() ? text_[pos_ + ahead] : '\0';
  }
  [[nodiscard]] auto at_middle_dot() const -> bool {
    return peek() == '\xC2' && peek(1) == '\xB7';
  }
  void skip_blanks() {
    while (!at_end() && (peek() == ' ' || peek() == '\t'))
      ++pos_;
  }
  [[noreturn]] void fail(const std::string &message) const {
    throw UnitError("invalid unit '" + std::string(text_) + "': " + message);
  }

  [[nodiscard]] static auto is_digit(char c) -> bool {
    return c >= '0' && c <= '9';
  }
  [[nodiscard]] auto is_symbol_char(std::size_t at) const -> bool {
    if (at >= text_.size())
      return false;
    const char c = text_[at];
    if (c == '.' || c == '*' || c == '/' || c == '(' || c == ')' ||
        c == '^' || c == '-' || c == ' ' || c == '\t' || is_digit(c))
      return false;
    if (c == '\xC2' && at + 1 < text_.size() && text_[at + 1] == '\xB7')
      return false; // middle dot
    return true;
  }

  // expression := factor ( op factor )*
  auto expression() -> Term {
    Term result = factor();
    while (true) {
      skip_blanks();
      bool divide = false;
      if (peek() == '/') {
        divide = true;
        ++pos_;
      } else if (peek() == '.' || peek() == '*') {
        ++pos_;
      } else if (at_middle_dot()) {
        pos_ += 2;
      } else {
        break;
      }
      skip_blanks();
      if (at_end())
        fail("missing unit after an operator");
      const Term next = factor();
      if (divide) {
        result.scale = multiply(result.scale, inverse(next.scale));
        result.dimension = result.dimension / next.dimension;
      } else {
        result.scale = multiply(result.scale, next.scale);
        result.dimension = result.dimension * next.dimension;
      }
      result.symbols += next.symbols;
      result.modified = true;
    }
    return result;
  }

  // factor := primary exponent?
  auto factor() -> Term {
    Term term = primary();
    if (const auto power = exponent()) {
      term.scale = raise(term.scale, *power);
      term.dimension = term.dimension.pow(*power);
      term.modified = true;
    }
    return term;
  }

  auto primary() -> Term {
    skip_blanks();
    if (peek() == '(') {
      ++pos_;
      Term inner = expression();
      skip_blanks();
      if (peek() != ')')
        fail("missing ')'");
      ++pos_;
      inner.modified = true;
      return inner;
    }
    if (text_.substr(pos_, 5) == "sqrt(") {
      pos_ += 5;
      Term inner = expression();
      skip_blanks();
      if (peek() != ')')
        fail("missing ')' after sqrt(");
      ++pos_;
      inner.scale = raise(inner.scale, Rational::make(1, 2));
      inner.dimension = inner.dimension.pow(Rational::make(1, 2));
      inner.modified = true;
      return inner;
    }
    if (is_digit(peek()))
      return number();
    if (is_symbol_char(pos_))
      return symbol();
    if (at_end())
      fail("missing unit at the end");
    fail(std::string("unexpected '") + peek() + "'");
  }

  auto number() -> Term {
    const std::size_t begin = pos_;
    while (is_digit(peek()) || peek() == '.' || peek() == 'e' ||
           peek() == 'E' ||
           ((peek() == '-' || peek() == '+') &&
            (text_[pos_ - 1] == 'e' || text_[pos_ - 1] == 'E')))
      ++pos_;
    const std::string literal(text_.substr(begin, pos_ - begin));
    double value = 0.0;
    try {
      std::size_t used = 0;
      value = std::stod(literal, &used);
      if (used != literal.size())
        throw std::invalid_argument("trailing characters");
    } catch (const std::exception &) {
      fail("invalid number '" + literal + "'");
    }
    if (!(value > 0.0) || !std::isfinite(value))
      fail("numeric factors must be positive");
    Term term;
    term.scale = Scale{value, 1.0, Rational{0, 1}};
    normalize(term.scale);
    term.modified = true;
    return term;
  }

  auto symbol() -> Term {
    const std::size_t begin = pos_;
    while (is_symbol_char(pos_))
      ++pos_;
    const std::string_view name = text_.substr(begin, pos_ - begin);

    Term term;
    term.symbols = 1;
    for (const auto &unit : registry())
      if (unit.symbol == name) {
        term.scale = unit.scale;
        normalize(term.scale);
        term.dimension = unit.dimension;
        term.offset = unit.offset;
        return term;
      }
    for (const auto &prefix : prefixes) {
      if (name.size() <= prefix.symbol.size() ||
          name.substr(0, prefix.symbol.size()) != prefix.symbol)
        continue;
      const std::string_view rest = name.substr(prefix.symbol.size());
      for (const auto &unit : registry())
        if (unit.symbol == rest && unit.prefixable) {
          term.scale = multiply(Scale{1.0, 1.0, Rational{prefix.exponent10, 1}},
                                unit.scale);
          term.dimension = unit.dimension;
          term.modified = true;
          return term;
        }
    }
    const std::string hint = suggestion_for(name);
    fail("unknown unit symbol '" + std::string(name) + "'" +
         (hint.empty() ? std::string() : " (did you mean '" + hint + "'?)"));
  }

  // exponent := '^' signed | glued signed (digits right after a symbol/')')
  auto exponent() -> std::optional<Rational> {
    if (peek() == '^') {
      ++pos_;
      skip_blanks();
      if (peek() == '(') {
        ++pos_;
        const Rational value = signed_rational(true);
        if (peek() != ')')
          fail("missing ')' in exponent");
        ++pos_;
        return value;
      }
      return signed_rational(false);
    }
    if (is_digit(peek()) || (peek() == '-' && is_digit(peek(1))))
      return signed_rational(false);
    return std::nullopt;
  }

  auto signed_rational(const bool allow_fraction) -> Rational {
    int sign = 1;
    if (peek() == '-' || peek() == '+') {
      sign = peek() == '-' ? -1 : 1;
      ++pos_;
    }
    if (!is_digit(peek()))
      fail("invalid exponent");
    long long numerator = 0;
    long long denominator = 1;
    while (is_digit(peek())) {
      numerator = numerator * 10 + (peek() - '0');
      ++pos_;
      if (numerator > 1000)
        fail("exponent too large");
    }
    if (peek() == '.' && is_digit(peek(1))) {
      ++pos_;
      while (is_digit(peek())) {
        numerator = numerator * 10 + (peek() - '0');
        denominator *= 10;
        ++pos_;
        if (denominator > 1000)
          fail("too many decimals in exponent");
      }
    } else if (allow_fraction && peek() == '/') {
      ++pos_;
      if (!is_digit(peek()))
        fail("invalid fractional exponent");
      denominator = 0;
      while (is_digit(peek())) {
        denominator = denominator * 10 + (peek() - '0');
        ++pos_;
        if (denominator > 1000)
          fail("exponent too large");
      }
      if (denominator == 0)
        fail("zero denominator in exponent");
    }
    return Rational::make(sign * static_cast<int>(numerator),
                          static_cast<int>(denominator));
  }
};

struct NamedDimension {
  Dimension dimension;
  std::string_view name;
};

const std::vector<NamedDimension> &named_dimensions() {
  static const std::vector<NamedDimension> table = {
      {dim::Dimensionless, "dimensionless"},
      {dim::Length, "length"},
      {dim::Mass, "mass"},
      {dim::Time, "time"},
      {dim::Temperature, "temperature"},
      {dim::Amount, "amount of substance"},
      {dim::Current, "electric current"},
      {dim::LuminousIntensity, "luminous intensity"},
      {dim::Area, "area"},
      {dim::Volume, "volume"},
      {dim::InverseLength, "inverse length"},
      {dim::Frequency, "frequency"},
      {dim::Velocity, "velocity"},
      {dim::Acceleration, "acceleration"},
      {dim::Force, "force"},
      {dim::Pressure, "pressure"},
      {dim::Energy, "energy"},
      {dim::Power, "power"},
      {dim::Density, "density"},
      {dim::SpecificVolume, "specific volume"},
      {dim::NumberDensity, "number density"},
      {dim::Concentration, "concentration"},
      {dim::MolarMass, "molar mass"},
      {dim::MassFlowRate, "mass flow rate"},
      {dim::MolarFlowRate, "molar flow rate"},
      {dim::VolumetricFlowRate, "volumetric flow rate"},
      {dim::MassFlux, "mass flux"},
      {dim::HeatFlux, "heat flux"},
      {dim::HeatTransferCoefficient, "heat transfer coefficient"},
      {dim::ThermalConductivity, "thermal conductivity"},
      {dim::DynamicViscosity, "dynamic viscosity"},
      {dim::Diffusivity, "diffusivity or kinematic viscosity"},
      {dim::SurfaceTension, "surface tension"},
      {dim::SpecificEnergy, "specific energy"},
      {dim::MolarEnergy, "molar energy"},
      {dim::HeatCapacity, "heat capacity"},
      {dim::SpecificHeatCapacity, "specific heat capacity"},
      {dim::MolarHeatCapacity, "molar heat capacity"},
      {dim::PowerDensity, "power density"},
      {dim::HeatingRate, "heating rate"},
      {dim::SurfaceDensity, "surface density"},
      {dim::SurfaceConcentration, "surface concentration"},
  };
  return table;
}

[[nodiscard]] auto mismatch_message(std::string_view unit,
                                    const Dimension &found,
                                    const Dimension &expected) -> std::string {
  return "units '" + std::string(unit) + "' (" + describe(found) +
         ") are not compatible with the expected " + describe(expected);
}

// Parses a unit expression into its exact scale, dimension and offset.
[[nodiscard]] auto parse_term(std::string_view text) -> Term {
  // Trim blanks.
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
    text.remove_prefix(1);
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
    text.remove_suffix(1);

  if (text == "-")
    return Term{};

  Parser parser(text);
  Term term = parser.parse();
  // The offset survives only for a stand-alone affine symbol (e.g. "C").
  if (term.symbols != 1 || term.modified)
    term.offset = 0.0;
  return term;
}

[[nodiscard]] auto to_conversion(const Term &term) -> UnitConversion {
  UnitConversion conversion;
  set_factor(conversion, term.scale);
  conversion.offset = term.offset;
  conversion.dimension = term.dimension;
  return conversion;
}

[[nodiscard]] auto parse_checked(std::string_view unit,
                                 const Dimension &expected,
                                 const UnitSystem &system) -> UnitConversion {
  Term term = parse_term(unit);
  if (!(term.dimension == expected))
    throw UnitError(mismatch_message(unit, term.dimension, expected));
  if (!expected.is_temperature() || expected.temperature_difference)
    term.offset = 0.0;
  if (system.amount == AmountBase::Kilomole) {
    // 1 kmol = 10^3 mol: divide by 10^(3e) exactly.
    const Rational e =
        expected.exponents[static_cast<std::size_t>(Base::Amount)];
    term.scale.e10 = term.scale.e10 - Rational{3, 1} * e;
  }
  return to_conversion(term);
}
} // namespace

auto parse_unit(std::string_view text) -> UnitConversion {
  return to_conversion(parse_term(text));
}

auto to_si(const double value, std::string_view unit, const Dimension &expected,
           const UnitSystem &system) -> double {
  const UnitConversion c = parse_checked(unit, expected, system);
  return apply_scale(value, c.numerator, c.denominator, c.exponent10) +
         c.offset;
}

auto from_si(const double value_si, std::string_view unit,
             const Dimension &expected, const UnitSystem &system) -> double {
  const UnitConversion c = parse_checked(unit, expected, system);
  return apply_scale(value_si - c.offset, c.denominator, c.numerator,
                     -c.exponent10);
}

auto check_unit(std::string_view unit, const Dimension &expected)
    -> std::optional<std::string> {
  try {
    (void)parse_checked(unit, expected, si);
    return std::nullopt;
  } catch (const UnitError &error) {
    return std::string(error.what());
  }
}

auto si_unit(const Dimension &dimension, const UnitSystem &system)
    -> std::string {
  if (dimension.is_dimensionless())
    return "-";
  // Conventional order: kg, m, s, K, mol, A, cd.
  constexpr std::pair<Base, std::string_view> order[] = {
      {Base::Mass, "kg"},      {Base::Length, "m"},
      {Base::Time, "s"},       {Base::Temperature, "K"},
      {Base::Amount, "mol"},   {Base::Current, "A"},
      {Base::LuminousIntensity, "cd"}};
  const auto power = [](const Rational &r) -> std::string {
    if (r == Rational{1, 1})
      return "";
    if (r.den == 1)
      return std::to_string(r.num);
    return "^(" + r.to_string() + ")";
  };
  std::string numerator;
  std::string denominator;
  for (const auto &[base, base_symbol] : order) {
    const std::string_view symbol =
        (base == Base::Amount && system.amount == AmountBase::Kilomole)
            ? std::string_view("kmol")
            : base_symbol;
    const Rational e = dimension.exponents[static_cast<std::size_t>(base)];
    if (e.num > 0) {
      if (!numerator.empty())
        numerator += '.';
      numerator += std::string(symbol) + power(e);
    } else if (e.num < 0) {
      denominator +=
          "/" + std::string(symbol) + power(Rational::make(-e.num, e.den));
    }
  }
  return (numerator.empty() ? "1" : numerator) + denominator;
}

auto describe(const Dimension &dimension, const UnitSystem &system)
    -> std::string {
  const std::string unit = "[" + si_unit(dimension, system) + "]";
  if (dimension.is_temperature() && dimension.temperature_difference)
    return "temperature difference " + unit;
  for (const auto &named : named_dimensions())
    if (named.dimension == dimension)
      return std::string(named.name) + " " + unit;
  return "dimension " + unit;
}
} // namespace OpenSMOKEpp::units
