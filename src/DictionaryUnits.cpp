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

struct UnitDefinition {
  std::string_view symbol;
  double factor;
  Dimension dimension;
  bool prefixable;
  double offset = 0.0; // affine temperature scales only
};

// Symbols are case-sensitive. Exact symbols take precedence over prefixed
// readings (min = minute, Pa = pascal, cd = candela, h = hour).
const std::vector<UnitDefinition> &registry() {
  static const std::vector<UnitDefinition> table = {
      // SI base units (g instead of kg, so that prefixes apply)
      {"m", 1.0, dim::Length, true},
      {"g", 1.0e-3, dim::Mass, true},
      {"s", 1.0, dim::Time, true},
      {"K", 1.0, dim::Temperature, true},
      {"mol", 1.0, dim::Amount, true},
      {"A", 1.0, dim::Current, true},
      {"cd", 1.0, dim::LuminousIntensity, true},
      // SI derived units
      {"Hz", 1.0, dim::Frequency, true},
      {"N", 1.0, dim::Force, true},
      {"Pa", 1.0, dim::Pressure, true},
      {"J", 1.0, dim::Energy, true},
      {"W", 1.0, dim::Power, true},
      {"Wh", 3600.0, dim::Energy, true},
      {"L", 1.0e-3, dim::Volume, true},
      {"l", 1.0e-3, dim::Volume, true},
      {"bar", 1.0e5, dim::Pressure, true},
      {"eV", 1.602176634e-19, dim::Energy, true},
      {"cal", 4.184, dim::Energy, true}, // thermochemical calorie
      // Time
      {"min", 60.0, dim::Time, false},
      {"h", 3600.0, dim::Time, false},
      {"hr", 3600.0, dim::Time, false},
      // Length
      {"micron", 1.0e-6, dim::Length, false},
      {"angstrom", 1.0e-10, dim::Length, false},
      {"in", 0.0254, dim::Length, false},
      {"ft", 0.3048, dim::Length, false},
      // Mass
      {"lb", 0.45359237, dim::Mass, false},
      {"oz", 0.028349523125, dim::Mass, false},
      // Pressure
      {"atm", 101325.0, dim::Pressure, false},
      {"torr", 101325.0 / 760.0, dim::Pressure, false},
      {"mmHg", 133.322387415, dim::Pressure, false},
      {"psi", 6894.757293168361, dim::Pressure, false},
      // Energy and force (CGS and others)
      {"erg", 1.0e-7, dim::Energy, false},
      {"dyn", 1.0e-5, dim::Force, false},
      {"BTU", 1055.05585262, dim::Energy, false},
      // Temperature scales
      {"C", 1.0, dim::Temperature, false, 273.15},
      {"\xC2\xB0" "C", 1.0, dim::Temperature, false, 273.15},
      {"degC", 1.0, dim::Temperature, false, 273.15},
      {"F", 5.0 / 9.0, dim::Temperature, false, 459.67 * 5.0 / 9.0},
      {"\xC2\xB0" "F", 5.0 / 9.0, dim::Temperature, false, 459.67 * 5.0 / 9.0},
      {"degF", 5.0 / 9.0, dim::Temperature, false, 459.67 * 5.0 / 9.0},
      {"R", 5.0 / 9.0, dim::Temperature, false},
      // Angles and rotation (radian is dimensionless)
      {"rad", 1.0, dim::Dimensionless, false},
      {"deg", pi / 180.0, dim::Dimensionless, false},
      {"\xC2\xB0", pi / 180.0, dim::Dimensionless, false},
      {"rpm", 2.0 * pi / 60.0, dim::Frequency, false},
      // Dimensionless
      {"dimensionless", 1.0, dim::Dimensionless, false},
      {"#", 1.0, dim::Dimensionless, false},
      {"%", 1.0e-2, dim::Dimensionless, false},
      {"ppm", 1.0e-6, dim::Dimensionless, false},
  };
  return table;
}

struct Prefix {
  std::string_view symbol;
  double factor;
};

constexpr Prefix prefixes[] = {
    {"da", 1e1},  {"Y", 1e24}, {"Z", 1e21}, {"E", 1e18},  {"P", 1e15},
    {"T", 1e12},  {"G", 1e9},  {"M", 1e6},  {"k", 1e3},   {"h", 1e2},
    {"d", 1e-1},  {"c", 1e-2}, {"m", 1e-3}, {"u", 1e-6},  {"\xC2\xB5", 1e-6},
    {"\xCE\xBC", 1e-6}, {"n", 1e-9}, {"p", 1e-12}, {"f", 1e-15},
    {"a", 1e-18}};

// Frequently used prefixed symbols, offered as spelling suggestions.
constexpr std::string_view common_prefixed[] = {
    "kg", "mg", "km", "cm", "mm", "um", "nm", "ms", "kPa", "MPa",
    "kmol", "mmol", "kJ", "MJ", "kW", "kcal", "mbar", "kWh", "mL", "dm"};

struct Term {
  double factor = 1.0;
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
        result.factor /= next.factor;
        result.dimension = result.dimension / next.dimension;
      } else {
        result.factor *= next.factor;
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
      term.factor = std::pow(term.factor, static_cast<double>(power->num) /
                                              static_cast<double>(power->den));
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
      inner.factor = std::sqrt(inner.factor);
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
    term.factor = value;
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
        term.factor = unit.factor;
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
          term.factor = prefix.factor * unit.factor;
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

[[nodiscard]] auto parse_checked(std::string_view unit,
                                 const Dimension &expected,
                                 const UnitSystem &system) -> UnitConversion {
  UnitConversion conversion = parse_unit(unit);
  if (!(conversion.dimension == expected))
    throw UnitError(mismatch_message(unit, conversion.dimension, expected));
  if (!expected.is_temperature() || expected.temperature_difference)
    conversion.offset = 0.0;
  if (system.amount == AmountBase::Kilomole) {
    const Rational e =
        expected.exponents[static_cast<std::size_t>(Base::Amount)];
    if (!e.is_zero())
      conversion.factor /= std::pow(1000.0, static_cast<double>(e.num) /
                                                static_cast<double>(e.den));
  }
  return conversion;
}
} // namespace

auto parse_unit(std::string_view text) -> UnitConversion {
  // Trim blanks.
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
    text.remove_prefix(1);
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
    text.remove_suffix(1);

  if (text == "-")
    return UnitConversion{1.0, 0.0, dim::Dimensionless};

  Parser parser(text);
  const Term term = parser.parse();
  UnitConversion result;
  result.factor = term.factor;
  result.dimension = term.dimension;
  // The offset survives only for a stand-alone affine symbol (e.g. "C").
  result.offset = (term.symbols == 1 && !term.modified) ? term.offset : 0.0;
  return result;
}

auto to_si(const double value, std::string_view unit, const Dimension &expected,
           const UnitSystem &system) -> double {
  const UnitConversion conversion = parse_checked(unit, expected, system);
  return value * conversion.factor + conversion.offset;
}

auto from_si(const double value_si, std::string_view unit,
             const Dimension &expected, const UnitSystem &system) -> double {
  const UnitConversion conversion = parse_checked(unit, expected, system);
  return (value_si - conversion.offset) / conversion.factor;
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
