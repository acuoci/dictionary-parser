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


#ifndef OpenSMOKEpp_DictionaryTyped_H
#define OpenSMOKEpp_DictionaryTyped_H

/**
 * \file DictionaryTyped.h
 * \brief Typed, compile-time front end.
 *
 * A keyword is declared once, as a `constexpr` object whose type carries
 * the keyword name and the C++ type of its value. A grammar lists its keys
 * and rules; a `View` of a dictionary reads a key with the type deduced
 * from the declaration:
 *
 * \code
 * namespace gas {
 * using namespace OpenSMOKEpp::typed;
 * namespace dim = OpenSMOKEpp::units::dim;
 * inline constexpr auto T = Key<"@Temperature">(Quantity(dim::Temperature), "Temperature");
 * inline constexpr auto P = Key<"@Pressure">(Quantity(dim::Pressure), "Pressure");
 * inline constexpr auto X = Key<"@MoleFractions">(Composition().Sum(1.0), "Mole fractions");
 *
 * struct Status : Grammar<Status> {
 *   static constexpr auto keys = Keys(T, P, X);
 *   static constexpr auto constraints = Rules(Required(T), Required(X));
 * };
 * } // namespace gas
 *
 * auto mix = manager.view<gas::Status>("initial-mixture");
 * double T = mix.get(gas::T);        // SI kelvin
 * auto   P = mix.get_or(gas::P, 101325.);
 * mix.get(ode::RelTol);              // does not compile: not a key of gas::Status
 * \endcode
 *
 * What is checked at compile time:
 * - keyword names (a leading `@`, no blanks);
 * - reading a key that the grammar does not declare, or declares with a
 *   different value type;
 * - reading into a variable of the wrong type (`read(key, out)`);
 * - the coherence of the grammar: duplicate keys, rules on undeclared
 *   keys, invalid counts, conditions on values a key can never have, keys
 *   that require and conflict with each other, unusable ("dead") keys and
 *   unsatisfiable rules (the same analysis as `DictionaryGrammar::Check`,
 *   restricted to what is known at compile time).
 *
 * What depends on the input file is checked when a grammar is attached, as
 * for every other grammar (presence, values, units, references).
 *
 * The typed layer only describes keywords: a typed grammar is translated
 * into an ordinary `DictionaryGrammar` and reads go through `Dictionary`.
 */

#include <array>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

#include "Dictionary.h"
#include "DictionaryFormats.h"
#include "DictionaryGrammar.h"
#include "DictionaryManager.h"
#include "DictionaryRules.h"
#include "DictionaryUnits.h"

namespace OpenSMOKEpp::typed {

// ---------------------------------------------------------------------------
// Compile-time strings
// ---------------------------------------------------------------------------

/**
 * \brief String literal usable as a template argument (`Key<"@Name">`).
 */
template <std::size_t N> struct fixed_string {
  /** \brief Characters, including the terminating null. */
  char data[N]{};

  constexpr fixed_string() = default;
  // NOLINTNEXTLINE(google-explicit-constructor): used for literals
  constexpr fixed_string(const char (&text)[N]) {
    for (std::size_t i = 0; i < N; ++i)
      data[i] = text[i];
  }
  /** \brief Copy of (at most `N - 1` characters of) `text`. */
  constexpr explicit fixed_string(std::string_view text) {
    for (std::size_t i = 0; i + 1 < N && i < text.size(); ++i)
      data[i] = text[i];
  }
  /** \brief The text, without the terminating null. */
  [[nodiscard]] constexpr auto view() const -> std::string_view {
    std::size_t n = 0;
    while (n + 1 < N && data[n] != '\0')
      ++n;
    return {data, n};
  }
};
template <std::size_t N> fixed_string(const char (&)[N]) -> fixed_string<N>;

namespace detail {
/** \brief A keyword name: `@` followed by at least one character, no blanks
 * or separators. */
[[nodiscard]] constexpr auto valid_keyword_name(std::string_view name) -> bool {
  if (name.size() < 2 || name[0] != '@')
    return false;
  for (const char c : name)
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ';' ||
        c == '{' || c == '}' || c == '"')
      return false;
  return true;
}

[[nodiscard]] constexpr auto lower(char c) -> char {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

[[nodiscard]] constexpr auto iequals(std::string_view a, std::string_view b)
    -> bool {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (lower(a[i]) != lower(b[i]))
      return false;
  return true;
}

[[nodiscard]] constexpr auto is_integer_text(std::string_view text) -> bool {
  std::size_t i = (!text.empty() && (text[0] == '+' || text[0] == '-')) ? 1 : 0;
  if (i == text.size())
    return false;
  for (; i < text.size(); ++i)
    if (text[i] < '0' || text[i] > '9')
      return false;
  return true;
}

/** \brief Numeric bound of a typed value. */
struct Bound {
  double value = 0.0;
  bool inclusive = true;
};

/** \brief Minimum and maximum, shared by the numeric specs. */
template <class Spec> struct Bounded {
  std::optional<Bound> minimum_{};
  std::optional<Bound> maximum_{};

  /** \brief Copy with a lower bound (SI units for quantities). */
  [[nodiscard]] constexpr auto Minimum(double value, bool inclusive = true) const
      -> Spec {
    Spec copy = static_cast<const Spec &>(*this);
    copy.minimum_ = Bound{value, inclusive};
    return copy;
  }
  /** \brief Copy with an upper bound (SI units for quantities). */
  [[nodiscard]] constexpr auto Maximum(double value, bool inclusive = true) const
      -> Spec {
    Spec copy = static_cast<const Spec &>(*this);
    copy.maximum_ = Bound{value, inclusive};
    return copy;
  }

protected:
  [[nodiscard]] auto bound(ValueFormat format) const -> ValueFormat {
    if (minimum_)
      format = format.Minimum(minimum_->value, minimum_->inclusive);
    if (maximum_)
      format = format.Maximum(maximum_->value, maximum_->inclusive);
    return format;
  }
  void bound(DictionaryKeyWord &keyword) const {
    if (minimum_)
      keyword.SetMinimum(minimum_->value, minimum_->inclusive);
    if (maximum_)
      keyword.SetMaximum(maximum_->value, maximum_->inclusive);
  }
};

/** \brief Size limits, shared by the list specs. */
template <class Spec> struct Sized {
  std::size_t size_min_ = 1;
  std::size_t size_max_ = std::numeric_limits<std::size_t>::max();

  /** \brief Copy with an exact number of elements. */
  [[nodiscard]] constexpr auto Size(std::size_t n) const -> Spec {
    return Size(n, n);
  }
  /** \brief Copy with a minimum and maximum number of elements. */
  [[nodiscard]] constexpr auto Size(std::size_t minimum, std::size_t maximum) const
      -> Spec {
    Spec copy = static_cast<const Spec &>(*this);
    copy.size_min_ = minimum;
    copy.size_max_ = maximum;
    return copy;
  }

protected:
  [[nodiscard]] constexpr auto sized() const -> bool {
    return size_min_ != 1 ||
           size_max_ != std::numeric_limits<std::size_t>::max();
  }
};

[[nodiscard]] inline auto structured(const std::string &name, ValueFormat format,
                                     const std::string &comment,
                                     bool compulsory) -> DictionaryKeyWord {
  return {name, std::move(format), comment, compulsory};
}
} // namespace detail

template <class G> class View;

/**
 * \brief Units read from a dictionary (key declared with `Unit()`), e.g. the
 * units of a profile whose variable is known only at run time.
 */
struct Units {
  /** \brief The units as written, e.g. `cm`. */
  std::string symbol;
  /** \brief Unit system of the dictionary (amounts in kmol by default). */
  units::UnitSystem system{};
  /** \brief Location of the keyword, used in error messages. */
  SourceLocation location{};

  /**
   * \brief Converts `value`, written in these units, to SI.
   *
   * \warning Throws `DictionaryError` (code `InvalidUnit`, located at the
   * keyword) if the units do not have `dimension`.
   */
  [[nodiscard]] auto to_si(double value, const units::Dimension &dimension) const
      -> double {
    try {
      return units::to_si(value, symbol, dimension, system);
    } catch (const units::UnitError &error) {
      throw DictionaryError({Diagnostic{Severity::Error, DiagnosticCode::InvalidUnit,
                                        location, error.what(), {}, {}}},
                            "Units " + symbol);
    }
  }
};
} // namespace OpenSMOKEpp::typed

namespace OpenSMOKEpp::detail {
/** \brief Conversion of a `Unit()` value (its system and location are set by
 * the view). */
template <> struct ValueConverter<typed::Units, void> {
  static auto from(const Value &v) -> typed::Units {
    if (v.kind != Value::Kind::Text)
      shape_error("units", v);
    return typed::Units{v.text, {}, {}};
  }
};
} // namespace OpenSMOKEpp::detail

namespace OpenSMOKEpp::typed {

// ---------------------------------------------------------------------------
// Value specs: the C++ type of a value and its format
// ---------------------------------------------------------------------------
//
// Every spec has:
//   using type = ...;                     the C++ type returned by View::get
//   static constexpr bool comparable;     usable in Equals / OneOf conditions
//   constexpr bool may_equal(text) const; whether a condition value is valid
//   ValueFormat format() const;           the runtime format
//   DictionaryKeyWord keyword(name, comment, compulsory) const;

/** \brief One integer (`int`). */
struct IntegerSpec : detail::Bounded<IntegerSpec> {
  using type = int;
  static constexpr bool comparable = true;
  [[nodiscard]] constexpr auto may_equal(std::string_view text) const -> bool {
    return detail::is_integer_text(text);
  }
  [[nodiscard]] auto format() const -> ValueFormat {
    return bound(formats::Integer());
  }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    DictionaryKeyWord k(name, SINGLE_INT, comment, compulsory);
    bound(k);
    return k;
  }
};

/** \brief One finite number (`double`). */
struct RealSpec : detail::Bounded<RealSpec> {
  using type = double;
  static constexpr bool comparable = false;
  [[nodiscard]] constexpr auto may_equal(std::string_view) const -> bool {
    return false;
  }
  [[nodiscard]] auto format() const -> ValueFormat {
    return bound(formats::Real());
  }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    DictionaryKeyWord k(name, SINGLE_DOUBLE, comment, compulsory);
    bound(k);
    return k;
  }
};

/** \brief One boolean: `true`, `false`, `on`, `off` (`bool`). */
struct BooleanSpec {
  using type = bool;
  static constexpr bool comparable = true;
  [[nodiscard]] constexpr auto may_equal(std::string_view text) const -> bool {
    return detail::iequals(text, "true") || detail::iequals(text, "false") ||
           detail::iequals(text, "on") || detail::iequals(text, "off");
  }
  [[nodiscard]] auto format() const -> ValueFormat { return formats::Boolean(); }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    return {name, SINGLE_BOOL, comment, compulsory};
  }
};

/** \brief One token, possibly quoted (`std::string`). */
struct WordSpec {
  using type = std::string;
  static constexpr bool comparable = true;
  [[nodiscard]] constexpr auto may_equal(std::string_view text) const -> bool {
    return !text.empty();
  }
  [[nodiscard]] auto format() const -> ValueFormat { return formats::Word(); }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    return {name, SINGLE_STRING, comment, compulsory};
  }
};

/** \brief All the tokens of the value, joined by blanks (`std::string`). */
struct TextSpec {
  using type = std::string;
  static constexpr bool comparable = false;
  [[nodiscard]] constexpr auto may_equal(std::string_view) const -> bool {
    return false;
  }
  [[nodiscard]] auto format() const -> ValueFormat { return formats::Text(); }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    return detail::structured(name, format(), comment, compulsory);
  }
};

/** \brief A file or directory path (`std::filesystem::path`). */
struct PathSpec {
  using type = std::filesystem::path;
  static constexpr bool comparable = false;
  [[nodiscard]] constexpr auto may_equal(std::string_view) const -> bool {
    return false;
  }
  [[nodiscard]] auto format() const -> ValueFormat { return formats::Text(); }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    return {name, SINGLE_PATH, comment, compulsory};
  }
};

/**
 * \brief The name of another dictionary read with its own, untyped grammar
 * (`std::string`). Use `Reference<G>()` when that dictionary has a typed
 * grammar `G`: it is then validated and returned as a `View<G>`.
 */
struct DictionaryNameSpec {
  using type = std::string;
  static constexpr bool comparable = false;
  [[nodiscard]] constexpr auto may_equal(std::string_view) const -> bool {
    return false;
  }
  [[nodiscard]] auto format() const -> ValueFormat { return formats::Word(); }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    return {name, SINGLE_DICTIONARY, comment, compulsory};
  }
};

/** \brief A number followed by units of a dimension; the value is in SI
 * units (`double`). */
struct QuantitySpec : detail::Bounded<QuantitySpec> {
  using type = double;
  static constexpr bool comparable = false;
  units::Dimension dimension{};
  [[nodiscard]] constexpr auto may_equal(std::string_view) const -> bool {
    return false;
  }
  [[nodiscard]] auto format() const -> ValueFormat {
    return bound(formats::Quantity(dimension));
  }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    DictionaryKeyWord k(name, SINGLE_MEASURE, comment, compulsory);
    k.SetDimension(dimension);
    bound(k);
    return k;
  }
};

/** \brief Measures of one dimension, `1 m 20 cm` or `1 2 3 m`; values in SI
 * units (`std::vector<double>`). */
struct QuantityListSpec : detail::Bounded<QuantityListSpec>,
                          detail::Sized<QuantityListSpec> {
  using type = std::vector<double>;
  static constexpr bool comparable = false;
  units::Dimension dimension{};
  [[nodiscard]] constexpr auto may_equal(std::string_view) const -> bool {
    return false;
  }
  [[nodiscard]] auto format() const -> ValueFormat {
    ValueFormat f = bound(formats::QuantityList(dimension));
    return sized() ? f.Size(size_min_, size_max_) : f;
  }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    DictionaryKeyWord k(name, VECTOR_MEASURE, comment, compulsory);
    k.SetDimension(dimension);
    bound(k);
    if (sized())
      k.SetSize(size_min_, size_max_);
    return k;
  }
};

/**
 * \brief A number followed by units whose dimension is given when the key is
 * read, with `View::get(key, dimension)`; the value is in SI units
 * (`double`). The grammar checks that the units are valid units.
 */
struct RuntimeQuantitySpec : detail::Bounded<RuntimeQuantitySpec> {
  using type = double;
  static constexpr bool comparable = false;
  [[nodiscard]] constexpr auto may_equal(std::string_view) const -> bool {
    return false;
  }
  [[nodiscard]] auto format() const -> ValueFormat {
    return bound(formats::Quantity());
  }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    return detail::structured(name, format(), comment, compulsory);
  }
};

/** \brief Measures whose dimension is given when the key is read, with
 * `View::get(key, dimension)` (`std::vector<double>`, SI units). The units are
 * compulsory: `1 m 20 cm` or `1 2 3 m`. */
struct RuntimeQuantityListSpec : detail::Bounded<RuntimeQuantityListSpec>,
                                 detail::Sized<RuntimeQuantityListSpec> {
  using type = std::vector<double>;
  static constexpr bool comparable = false;
  [[nodiscard]] constexpr auto may_equal(std::string_view) const -> bool {
    return false;
  }
  [[nodiscard]] auto format() const -> ValueFormat {
    ValueFormat f = bound(formats::QuantityList());
    return sized() ? f.Size(size_min_, size_max_) : f;
  }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    return detail::structured(name, format(), comment, compulsory);
  }
};

/** \brief One units token, e.g. `cm` (`Units`, which converts values with
 * `Units::to_si(value, dimension)`). The grammar checks that the units are
 * valid units. */
struct UnitSpec {
  using type = Units;
  static constexpr bool comparable = false;
  [[nodiscard]] constexpr auto may_equal(std::string_view) const -> bool {
    return false;
  }
  [[nodiscard]] auto format() const -> ValueFormat { return formats::Unit(); }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    return detail::structured(name, format(), comment, compulsory);
  }
};

/** \brief Pairs of a name and a number followed by their units,
 * `OH 1e-5 HO2 2e-5 mol/cm3`; the numbers are in SI units
 * (`std::vector<std::pair<std::string, double>>`). The units are compulsory. */
struct QuantityCompositionSpec : detail::Sized<QuantityCompositionSpec> {
  using type = std::vector<std::pair<std::string, double>>;
  static constexpr bool comparable = false;
  units::Dimension dimension{};
  [[nodiscard]] constexpr auto may_equal(std::string_view) const -> bool {
    return false;
  }
  [[nodiscard]] auto format() const -> ValueFormat {
    ValueFormat f = formats::Composition(dimension);
    return sized() ? f.Size(size_min_, size_max_) : f;
  }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    return detail::structured(name, format(), comment, compulsory);
  }
};

/** \brief Pairs of a name and a number, `CH4 0.5 O2 0.5`
 * (`std::vector<std::pair<std::string, double>>`). */
struct CompositionSpec : detail::Bounded<CompositionSpec>,
                         detail::Sized<CompositionSpec> {
  using type = std::vector<std::pair<std::string, double>>;
  static constexpr bool comparable = false;
  std::optional<std::pair<double, double>> sum_{};
  /** \brief Copy requiring the numbers to add up to `target`. */
  [[nodiscard]] constexpr auto Sum(double target, double tolerance = 1e-6) const
      -> CompositionSpec {
    CompositionSpec copy = *this;
    copy.sum_ = std::pair<double, double>{target, tolerance};
    return copy;
  }
  [[nodiscard]] constexpr auto may_equal(std::string_view) const -> bool {
    return false;
  }
  [[nodiscard]] auto format() const -> ValueFormat {
    ValueFormat f = formats::List(
        formats::Record({{"name", formats::Word()}, {"value", bound(formats::Real())}}));
    return sized() ? f.Size(size_min_, size_max_) : f;
  }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    DictionaryKeyWord k(name, VECTOR_STRING_DOUBLE, comment, compulsory);
    bound(k);
    if (sized())
      k.SetSize(size_min_, size_max_);
    if (sum_)
      k.SetSum(sum_->first, sum_->second);
    return k;
  }
};

/**
 * \brief One token among a fixed set of names.
 *
 * With `E` a C++ enumeration, the value is converted to `E` by position:
 * the first name is `E(0)`, the second `E(1)`, and so on. With
 * `E = std::string` the value is the (canonical) name.
 */
template <class E, std::size_t N, std::size_t A = 0, std::size_t D = 0>
struct EnumerationSpec {
  static_assert(std::is_enum_v<E> || std::is_same_v<E, std::string>,
                "Enumeration<E>: E must be an enum type or std::string");
  using type = E;
  static constexpr bool comparable = true;
  std::array<std::string_view, N> values{};
  std::array<std::array<std::string_view, 2>, A> aliases{};
  std::array<std::array<std::string_view, 2>, D> disabled{};

  /** \brief Copy with an extra spelling for `value`. */
  [[nodiscard]] constexpr auto Alias(std::string_view alias,
                                     std::string_view value) const
      -> EnumerationSpec<E, N, A + 1, D> {
    EnumerationSpec<E, N, A + 1, D> copy{};
    copy.values = values;
    for (std::size_t i = 0; i < A; ++i)
      copy.aliases[i] = aliases[i];
    copy.aliases[A] = {alias, value};
    copy.disabled = disabled;
    return copy;
  }
  /** \brief Copy in which `value` is known but rejected with `reason` (e.g.
   * a solver not available in this build). */
  [[nodiscard]] constexpr auto Disable(std::string_view value,
                                       std::string_view reason) const
      -> EnumerationSpec<E, N, A, D + 1> {
    EnumerationSpec<E, N, A, D + 1> copy{};
    copy.values = values;
    copy.aliases = aliases;
    for (std::size_t i = 0; i < D; ++i)
      copy.disabled[i] = disabled[i];
    copy.disabled[D] = {value, reason};
    return copy;
  }
  [[nodiscard]] constexpr auto may_equal(std::string_view text) const -> bool {
    std::string_view canonical = text;
    bool found = false;
    for (const auto &v : values)
      found = found || v == text;
    for (const auto &a : aliases)
      if (a[0] == text) {
        found = true;
        canonical = a[1];
      }
    for (const auto &d : disabled)
      if (d[0] == canonical)
        return false;
    return found;
  }
  [[nodiscard]] auto format() const -> ValueFormat {
    ValueFormat f =
        formats::Enumeration(std::vector<std::string>(values.begin(), values.end()));
    for (const auto &a : aliases)
      f = f.Alias(std::string(a[0]), std::string(a[1]));
    for (const auto &d : disabled)
      f = f.Disable(std::string(d[0]), std::string(d[1]));
    return f;
  }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    return detail::structured(name, format(), comment, compulsory);
  }
};

/** \brief Repetition of a fixed-width element (`std::vector<element type>`). */
template <class S> struct ListSpec : detail::Sized<ListSpec<S>> {
  using type = std::vector<typename S::type>;
  static constexpr bool comparable = false;
  S element{};
  [[nodiscard]] constexpr auto may_equal(std::string_view) const -> bool {
    return false;
  }
  [[nodiscard]] auto format() const -> ValueFormat {
    return formats::List(element.format(), this->size_min_, this->size_max_);
  }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    return detail::structured(name, format(), comment, compulsory);
  }
};

/** \brief A named field of a record. */
template <class S> struct Field {
  std::string_view name;
  S spec;
};
template <class S> Field(std::string_view, S) -> Field<S>;
template <std::size_t M, class S> Field(const char (&)[M], S) -> Field<S>;

/** \brief A fixed sequence of named fields (`std::tuple<field types...>`). */
template <class... S> struct RecordSpec {
  using type = std::tuple<typename S::type...>;
  static constexpr bool comparable = false;
  std::tuple<Field<S>...> fields;
  [[nodiscard]] constexpr auto may_equal(std::string_view) const -> bool {
    return false;
  }
  [[nodiscard]] auto format() const -> ValueFormat {
    return std::apply(
        [](const auto &...f) {
          return formats::Record({ValueFormat::Field{std::string(f.name),
                                                     f.spec.format()}...});
        },
        fields);
  }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    return detail::structured(name, format(), comment, compulsory);
  }
};

/** \brief A column of a table. */
struct Column {
  std::string_view name;
  std::optional<units::Dimension> dimension{};
  constexpr Column() = default;
  constexpr Column(std::string_view column_name) : name(column_name) {}
  constexpr Column(std::string_view column_name, const units::Dimension &d)
      : name(column_name), dimension(d) {}
};

/** \brief Rows of numbers after an optional units header; values in SI units
 * (`std::vector<std::array<double, N>>`). */
template <std::size_t N> struct TableSpec : detail::Sized<TableSpec<N>> {
  using type = std::vector<std::array<double, N>>;
  static constexpr bool comparable = false;
  std::array<Column, N> columns;
  [[nodiscard]] constexpr auto may_equal(std::string_view) const -> bool {
    return false;
  }
  [[nodiscard]] auto format() const -> ValueFormat {
    std::vector<ValueFormat::Column> list;
    for (const auto &c : columns)
      list.push_back({std::string(c.name), c.dimension});
    ValueFormat f = formats::Table(std::move(list));
    return this->sized() ? f.Size(this->size_min_, this->size_max_) : f;
  }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    return detail::structured(name, format(), comment, compulsory);
  }
};

/** \brief The first alternative that matches the whole value
 * (`std::variant<alternative types...>`). */
template <class... S> struct ChoiceSpec {
  using type = std::variant<typename S::type...>;
  static constexpr bool comparable = false;
  std::tuple<S...> alternatives;
  [[nodiscard]] constexpr auto may_equal(std::string_view) const -> bool {
    return false;
  }
  [[nodiscard]] auto format() const -> ValueFormat {
    return std::apply(
        [](const auto &...a) { return formats::Choice({a.format()...}); },
        alternatives);
  }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    return detail::structured(name, format(), comment, compulsory);
  }
};

/**
 * \brief The name of another dictionary, validated with the typed grammar
 * `G`. `View::get` returns a `View<G>` of it.
 */
template <class G> struct ReferenceSpec {
  using type = View<G>;
  using grammar = G;
  static constexpr bool comparable = true;
  [[nodiscard]] constexpr auto may_equal(std::string_view text) const -> bool {
    return !text.empty();
  }
  [[nodiscard]] auto format() const -> ValueFormat { return formats::Word(); }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    return {name, SINGLE_DICTIONARY, comment, compulsory};
  }
};

/** \brief Names of other dictionaries, each validated with the typed grammar
 * `G` (`std::vector<View<G>>`). */
template <class G> struct ReferenceListSpec : detail::Sized<ReferenceListSpec<G>> {
  using type = std::vector<View<G>>;
  using grammar = G;
  static constexpr bool comparable = false;
  [[nodiscard]] constexpr auto may_equal(std::string_view) const -> bool {
    return false;
  }
  [[nodiscard]] auto format() const -> ValueFormat {
    return formats::List(formats::Word(), this->size_min_, this->size_max_);
  }
  [[nodiscard]] auto keyword(const std::string &name, const std::string &comment,
                             bool compulsory) const -> DictionaryKeyWord {
    DictionaryKeyWord k(name, VECTOR_STRING, comment, compulsory);
    if (this->sized())
      k.SetSize(this->size_min_, this->size_max_);
    return k;
  }
};

// Factories ------------------------------------------------------------------

/** \brief One integer (`int`). */
[[nodiscard]] constexpr auto Integer() -> IntegerSpec { return {}; }
/** \brief One finite number (`double`). */
[[nodiscard]] constexpr auto Real() -> RealSpec { return {}; }
/** \brief One boolean (`bool`). */
[[nodiscard]] constexpr auto Boolean() -> BooleanSpec { return {}; }
/** \brief One token (`std::string`). */
[[nodiscard]] constexpr auto Word() -> WordSpec { return {}; }
/** \brief Free text: all the tokens joined by blanks (`std::string`). */
[[nodiscard]] constexpr auto Text() -> TextSpec { return {}; }
/** \brief A path (`std::filesystem::path`). */
[[nodiscard]] constexpr auto Path() -> PathSpec { return {}; }
/** \brief The name of a dictionary with an untyped grammar (`std::string`). */
[[nodiscard]] constexpr auto DictionaryName() -> DictionaryNameSpec { return {}; }
/** \brief A measure of the given dimension, in SI units (`double`). */
[[nodiscard]] constexpr auto Quantity(const units::Dimension &dimension)
    -> QuantitySpec {
  QuantitySpec spec;
  spec.dimension = dimension;
  return spec;
}
/** \brief Measures of the given dimension, in SI units
 * (`std::vector<double>`). */
[[nodiscard]] constexpr auto QuantityList(const units::Dimension &dimension)
    -> QuantityListSpec {
  QuantityListSpec spec;
  spec.dimension = dimension;
  return spec;
}
/** \brief A measure whose dimension is given when the key is read with
 * `View::get(key, dimension)` (`double`, SI units). */
[[nodiscard]] constexpr auto Quantity() -> RuntimeQuantitySpec { return {}; }
/** \brief Measures whose dimension is given when the key is read with
 * `View::get(key, dimension)` (`std::vector<double>`, SI units). */
[[nodiscard]] constexpr auto QuantityList() -> RuntimeQuantityListSpec {
  return {};
}
/** \brief One units token (`Units`). */
[[nodiscard]] constexpr auto Unit() -> UnitSpec { return {}; }
/** \brief Name–number pairs (`std::vector<std::pair<std::string, double>>`). */
[[nodiscard]] constexpr auto Composition() -> CompositionSpec { return {}; }
/** \brief Name–number pairs followed by the units of the numbers, which are
 * converted to SI (`std::vector<std::pair<std::string, double>>`). */
[[nodiscard]] constexpr auto Composition(const units::Dimension &dimension)
    -> QuantityCompositionSpec {
  QuantityCompositionSpec spec;
  spec.dimension = dimension;
  return spec;
}
/** \brief One of `names` (type `E`: an enum, or `std::string`). */
template <class E = std::string, class... Names>
[[nodiscard]] constexpr auto Enumeration(const Names &...names)
    -> EnumerationSpec<E, sizeof...(Names)> {
  static_assert(sizeof...(Names) > 0, "Enumeration: at least one value is needed");
  return EnumerationSpec<E, sizeof...(Names)>{{std::string_view(names)...}, {}, {}};
}
/** \brief A list of elements (`std::vector<element type>`). */
template <class S> [[nodiscard]] constexpr auto List(S element) -> ListSpec<S> {
  return ListSpec<S>{{}, element};
}
/** \brief A record of named fields (`std::tuple<field types...>`). */
template <class... S>
[[nodiscard]] constexpr auto Record(Field<S>... fields) -> RecordSpec<S...> {
  static_assert(sizeof...(S) > 0, "Record: at least one field is needed");
  return RecordSpec<S...>{std::tuple<Field<S>...>(fields...)};
}
/** \brief A table (`std::vector<std::array<double, N>>`). */
template <class... C>
[[nodiscard]] constexpr auto Table(C... columns) -> TableSpec<sizeof...(C)> {
  static_assert(sizeof...(C) > 0, "Table: at least one column is needed");
  return TableSpec<sizeof...(C)>{{}, {Column(columns)...}};
}
/** \brief The first matching alternative (`std::variant<...>`). */
template <class... S>
[[nodiscard]] constexpr auto Choice(S... alternatives) -> ChoiceSpec<S...> {
  static_assert(sizeof...(S) > 1, "Choice: at least two alternatives are needed");
  return ChoiceSpec<S...>{std::tuple<S...>(alternatives...)};
}
/** \brief A reference to a dictionary validated with the typed grammar `G`. */
template <class G> [[nodiscard]] constexpr auto Reference() -> ReferenceSpec<G> {
  return {};
}
/** \brief References to dictionaries validated with the typed grammar `G`. */
template <class G>
[[nodiscard]] constexpr auto ReferenceList() -> ReferenceListSpec<G> {
  return {};
}

// ---------------------------------------------------------------------------
// Keys
// ---------------------------------------------------------------------------

/**
 * \brief A typed keyword: its name and value spec are part of the type.
 *
 * Created with `Key<"@Name">(spec, "description")` and refined with
 * `Compulsory()`, `Default(text)` and `Deprecated(message)`.
 */
template <fixed_string Name, class S> struct KeySpec {
  static_assert(detail::valid_keyword_name(Name.view()),
                "typed key: a keyword name starts with '@' and contains no "
                "blanks, ';', '{', '}' or quotes");

  /** \brief The value spec type. */
  using spec_type = S;
  /** \brief The C++ type returned by `View::get`. */
  using value_type = typename S::type;

  /** \brief Value spec. */
  S spec{};
  /** \brief Description (documentation, `@Help`). */
  std::string_view comment{};
  /** \brief Whether the keyword must be written in the file. */
  bool compulsory = false;
  /** \brief Default value, as written in a file (e.g. `300 K`). */
  std::optional<std::string_view> default_{};
  /** \brief Deprecation advice, if deprecated. */
  std::optional<std::string_view> deprecated_{};

  /** \brief Keyword name, including the `@`. */
  [[nodiscard]] static constexpr auto name() -> std::string_view {
    return Name.view();
  }
  /** \brief Copy that must be written in the file. */
  [[nodiscard]] constexpr auto Compulsory() const -> KeySpec {
    KeySpec copy = *this;
    copy.compulsory = true;
    return copy;
  }
  /** \brief Copy with a default value, written as in a file. */
  [[nodiscard]] constexpr auto Default(std::string_view text) const -> KeySpec {
    KeySpec copy = *this;
    copy.default_ = text;
    return copy;
  }
  /** \brief Copy marked as deprecated. */
  [[nodiscard]] constexpr auto Deprecated(std::string_view message = {}) const
      -> KeySpec {
    KeySpec copy = *this;
    copy.deprecated_ = message;
    return copy;
  }
  /** \brief The equivalent runtime keyword declaration. */
  [[nodiscard]] auto keyword() const -> DictionaryKeyWord {
    DictionaryKeyWord k =
        spec.keyword(std::string(name()), std::string(comment), compulsory);
    if (default_)
      k.SetDefault(std::string(*default_));
    if (deprecated_)
      k.SetDeprecated(std::string(*deprecated_));
    return k;
  }
};

/** \brief Declares a typed key: `Key<"@Temperature">(Quantity(dim::Temperature), "...")`. */
template <fixed_string Name, class S>
[[nodiscard]] constexpr auto Key(S spec, std::string_view comment)
    -> KeySpec<Name, S> {
  return KeySpec<Name, S>{spec, comment};
}

namespace detail {
template <class T> struct is_key : std::false_type {};
template <fixed_string N, class S> struct is_key<KeySpec<N, S>> : std::true_type {};
template <class T> struct is_reference : std::false_type {};
template <class G> struct is_reference<ReferenceSpec<G>> : std::true_type {};
template <class T> struct is_reference_list : std::false_type {};
template <class G> struct is_reference_list<ReferenceListSpec<G>> : std::true_type {};

/** \brief Whether a spec contains measures whose dimension is given when the
 * key is read (`Quantity()`, `QuantityList()`). */
template <class S> struct needs_dimension : std::false_type {};
template <> struct needs_dimension<RuntimeQuantitySpec> : std::true_type {};
template <> struct needs_dimension<RuntimeQuantityListSpec> : std::true_type {};
template <class S> struct needs_dimension<ListSpec<S>> : needs_dimension<S> {};
template <class... S>
struct needs_dimension<RecordSpec<S...>>
    : std::bool_constant<(needs_dimension<S>::value || ...)> {};
template <class... S>
struct needs_dimension<ChoiceSpec<S...>>
    : std::bool_constant<(needs_dimension<S>::value || ...)> {};
} // namespace detail

/** \brief A typed key. */
template <class T>
concept TypedKey = detail::is_key<std::remove_cvref_t<T>>::value;

/** \brief The keys of a grammar: `static constexpr auto keys = Keys(T, P);`. */
template <TypedKey... K>
[[nodiscard]] constexpr auto Keys(const K &...keys) -> std::tuple<K...> {
  return std::tuple<K...>(keys...);
}

// ---------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------

/** \brief Capacity of a typed rule (keywords) and condition (values). */
inline constexpr std::size_t max_rule_keywords = 32;
inline constexpr std::size_t max_condition_values = 16;

namespace detail {
// Called only when a capacity is exceeded: not constexpr, so the compiler
// reports it.
inline void too_many_keywords_in_one_rule() {}
inline void too_many_values_in_one_condition() {}
} // namespace detail

/** \brief A typed condition (see `OpenSMOKEpp::Condition`). */
struct ConditionSpec {
  Condition::Kind kind = Condition::Kind::Always;
  std::string_view keyword{};
  std::array<std::string_view, max_condition_values> values{};
  std::size_t size = 0;

  /** \brief The equivalent runtime condition. */
  [[nodiscard]] auto condition() const -> Condition {
    Condition c;
    c.kind = kind;
    c.keyword = std::string(keyword);
    for (std::size_t i = 0; i < size; ++i)
      c.values.emplace_back(values[i]);
    return c;
  }
};

/** \brief A typed rule (see `OpenSMOKEpp::Rule`). */
struct RuleSpec {
  Rule::Kind kind = Rule::Kind::Required;
  std::array<std::string_view, max_rule_keywords> keywords{};
  std::size_t size = 0;
  std::size_t count = 0;
  ConditionSpec when{};

  /** \brief The equivalent runtime rule. */
  [[nodiscard]] auto rule() const -> Rule {
    Rule r;
    r.kind = kind;
    for (std::size_t i = 0; i < size; ++i)
      r.keywords.emplace_back(keywords[i]);
    r.count = count;
    r.when = when.condition();
    return r;
  }
};

namespace detail {
template <TypedKey... K>
[[nodiscard]] constexpr auto make_rule(Rule::Kind kind, std::size_t count,
                                       const K &...keys) -> RuleSpec {
  RuleSpec rule;
  rule.kind = kind;
  rule.count = count;
  if (sizeof...(K) > max_rule_keywords)
    too_many_keywords_in_one_rule();
  else
    ((rule.keywords[rule.size++] = keys.name()), ...);
  return rule;
}
} // namespace detail

/** \brief The key must be present. */
template <TypedKey K> [[nodiscard]] constexpr auto Required(const K &key) -> RuleSpec {
  return detail::make_rule(Rule::Kind::Required, 0, key);
}
/** \brief The key must be absent (useful with `When`). */
template <TypedKey K> [[nodiscard]] constexpr auto Forbidden(const K &key) -> RuleSpec {
  return detail::make_rule(Rule::Kind::Forbidden, 0, key);
}
/** \brief Exactly `n` of the keys must be present. */
template <TypedKey... K>
[[nodiscard]] constexpr auto Exactly(std::size_t n, const K &...keys) -> RuleSpec {
  return detail::make_rule(Rule::Kind::Exactly, n, keys...);
}
/** \brief At least `n` of the keys must be present. */
template <TypedKey... K>
[[nodiscard]] constexpr auto AtLeast(std::size_t n, const K &...keys) -> RuleSpec {
  return detail::make_rule(Rule::Kind::AtLeast, n, keys...);
}
/** \brief At most `n` of the keys may be present. */
template <TypedKey... K>
[[nodiscard]] constexpr auto AtMost(std::size_t n, const K &...keys) -> RuleSpec {
  return detail::make_rule(Rule::Kind::AtMost, n, keys...);
}
/** \brief Exactly one of the keys must be present. */
template <TypedKey... K>
[[nodiscard]] constexpr auto ExactlyOneOf(const K &...keys) -> RuleSpec {
  return Exactly(1, keys...);
}
/** \brief At least one of the keys must be present. */
template <TypedKey... K>
[[nodiscard]] constexpr auto AtLeastOneOf(const K &...keys) -> RuleSpec {
  return AtLeast(1, keys...);
}
/** \brief At most one of the keys may be present. */
template <TypedKey... K>
[[nodiscard]] constexpr auto AtMostOneOf(const K &...keys) -> RuleSpec {
  return AtMost(1, keys...);
}
/** \brief If `key` is present, every one of `required` must be present. */
template <TypedKey K, TypedKey R, TypedKey... Rs>
[[nodiscard]] constexpr auto Requires(const K &key, const R &required,
                                      const Rs &...more) -> RuleSpec {
  return detail::make_rule(Rule::Kind::Requires, 0, key, required, more...);
}
/** \brief If `key` is present, at least one of the candidates must be. */
template <TypedKey K, TypedKey R, TypedKey... Rs>
[[nodiscard]] constexpr auto RequiresAnyOf(const K &key, const R &candidate,
                                           const Rs &...more) -> RuleSpec {
  return detail::make_rule(Rule::Kind::RequiresAny, 0, key, candidate, more...);
}
/** \brief `key` cannot be used with any of the others (symmetric). */
template <TypedKey K, TypedKey R, TypedKey... Rs>
[[nodiscard]] constexpr auto Conflicts(const K &key, const R &other,
                                       const Rs &...more) -> RuleSpec {
  return detail::make_rule(Rule::Kind::Conflicts, 0, key, other, more...);
}

/** \brief The key is written in the file. */
template <TypedKey K> [[nodiscard]] constexpr auto Present(const K &key) -> ConditionSpec {
  ConditionSpec c;
  c.kind = Condition::Kind::Present;
  c.keyword = key.name();
  return c;
}
/** \brief The key is not written in the file. */
template <TypedKey K> [[nodiscard]] constexpr auto Absent(const K &key) -> ConditionSpec {
  ConditionSpec c;
  c.kind = Condition::Kind::Absent;
  c.keyword = key.name();
  return c;
}
/** \brief The (effective) value of the key equals `value`. */
template <TypedKey K>
[[nodiscard]] constexpr auto Equals(const K &key, std::string_view value)
    -> ConditionSpec {
  ConditionSpec c;
  c.kind = Condition::Kind::Equals;
  c.keyword = key.name();
  c.values[0] = value;
  c.size = 1;
  return c;
}
/** \brief The (effective) value of the key is one of `values`. */
template <TypedKey K, class... V>
[[nodiscard]] constexpr auto OneOf(const K &key, const V &...values)
    -> ConditionSpec {
  static_assert(sizeof...(V) > 0, "OneOf: at least one value is needed");
  ConditionSpec c;
  c.kind = Condition::Kind::OneOf;
  c.keyword = key.name();
  if (sizeof...(V) > max_condition_values)
    detail::too_many_values_in_one_condition();
  else
    ((c.values[c.size++] = std::string_view(values)), ...);
  return c;
}
/** \brief Makes `rule` apply only when `condition` holds. */
[[nodiscard]] constexpr auto When(const ConditionSpec &condition, RuleSpec rule)
    -> RuleSpec {
  rule.when = condition;
  return rule;
}

/** \brief The rules of a grammar: `static constexpr auto constraints = Rules(...);`. */
template <class... R>
[[nodiscard]] constexpr auto Rules(const R &...rules)
    -> std::array<RuleSpec, sizeof...(R)> {
  static_assert((std::is_same_v<R, RuleSpec> && ...),
                "Rules: every argument must be a rule (Required, Exactly, "
                "Requires, Conflicts, When, ...)");
  return {rules...};
}

// ---------------------------------------------------------------------------
// Compile-time coherence of a typed grammar
// ---------------------------------------------------------------------------

/** \brief Kind of coherence problem found at compile time. */
enum class Issue {
  none,
  duplicate_keyword,              ///< two keys have the same name
  compulsory_keyword_with_default,///< a compulsory key has a default
  rule_on_undeclared_keyword,     ///< a rule names a key not in `keys`
  keyword_listed_twice_in_rule,   ///< a rule names a key twice
  rule_needs_two_keywords,        ///< a counting rule names fewer than two keys
  invalid_rule_count,             ///< a count outside 1..size (AtMost: size-1)
  condition_on_undeclared_keyword,///< a condition tests a key not in `keys`
  condition_on_multi_token_value, ///< Equals/OneOf on a non single-token key
  invalid_condition_value,        ///< a value the key can never have
  requires_and_conflicts,         ///< a key requires a key it conflicts with
  compulsory_keywords_conflict,   ///< two always-required keys conflict
  unsatisfiable_keyword,          ///< an always-required key can never be used
  unsatisfiable_rule,             ///< a counting rule can never be satisfied
  dead_keyword                    ///< a key can never be used
};

/** \brief Result of the compile-time analysis (`keyword` and `other` are
 * the keys involved). */
struct Problem {
  Issue issue = Issue::none;
  fixed_string<64> keyword{};
  fixed_string<64> other{};
};

namespace detail {
template <class G>
concept has_constraints = requires { G::constraints; };

template <class G> constexpr auto constraints_of() {
  if constexpr (has_constraints<G>)
    return G::constraints;
  else
    return std::array<RuleSpec, 0>{};
}

template <class G>
inline constexpr std::size_t key_count =
    std::tuple_size_v<std::remove_cvref_t<decltype(G::keys)>>;

template <class Tuple>
[[nodiscard]] constexpr auto may_equal_at(const Tuple &keys, std::size_t index,
                                          std::string_view value) -> bool {
  return std::apply(
      [&](const auto &...k) {
        std::size_t i = 0;
        bool result = false;
        ((i++ == index ? (result = k.spec.may_equal(value), 0) : 0), ...);
        return result;
      },
      keys);
}

[[nodiscard]] constexpr auto problem(Issue issue, std::string_view keyword,
                                     std::string_view other = {}) -> Problem {
  return Problem{issue, fixed_string<64>(keyword), fixed_string<64>(other)};
}

/**
 * \brief Coherence analysis of a typed grammar, evaluated at compile time.
 *
 * Mirrors `DictionaryGrammar::Check` for what is known at compile time and
 * returns the first problem found.
 */
template <class G> [[nodiscard]] constexpr auto check() -> Problem {
  constexpr std::size_t n = key_count<G>;
  std::array<std::string_view, n> names{};
  std::array<bool, n> compulsory{};
  std::array<bool, n> defaulted{};
  std::array<bool, n> comparable{};
  {
    std::size_t i = 0;
    std::apply(
        [&](const auto &...k) {
          ((names[i] = k.name(), compulsory[i] = k.compulsory,
            defaulted[i] = k.default_.has_value(),
            comparable[i] = std::remove_cvref_t<decltype(k.spec)>::comparable,
            ++i),
           ...);
        },
        G::keys);
  }
  const auto index_of = [&](std::string_view name) -> std::size_t {
    for (std::size_t j = 0; j < n; ++j)
      if (names[j] == name)
        return j;
    return n;
  };

  // Keys.
  for (std::size_t a = 0; a < n; ++a)
    for (std::size_t b = a + 1; b < n; ++b)
      if (names[a] == names[b])
        return problem(Issue::duplicate_keyword, names[a]);
  for (std::size_t a = 0; a < n; ++a)
    if (compulsory[a] && defaulted[a])
      return problem(Issue::compulsory_keyword_with_default, names[a]);

  // Rules, one by one.
  const auto rules = constraints_of<G>();
  for (const RuleSpec &r : rules) {
    for (std::size_t k = 0; k < r.size; ++k) {
      if (index_of(r.keywords[k]) == n)
        return problem(Issue::rule_on_undeclared_keyword, r.keywords[k]);
      for (std::size_t j = 0; j < k; ++j)
        if (r.keywords[j] == r.keywords[k])
          return problem(Issue::keyword_listed_twice_in_rule, r.keywords[k]);
    }
    const bool counting = r.kind == Rule::Kind::Exactly ||
                          r.kind == Rule::Kind::AtLeast ||
                          r.kind == Rule::Kind::AtMost;
    if (counting && r.size < 2)
      return problem(Issue::rule_needs_two_keywords,
                     r.size > 0 ? r.keywords[0] : std::string_view{});
    if ((r.kind == Rule::Kind::Exactly || r.kind == Rule::Kind::AtLeast) &&
        (r.count < 1 || r.count > r.size))
      return problem(Issue::invalid_rule_count, r.keywords[0]);
    if (r.kind == Rule::Kind::AtMost && (r.count < 1 || r.count >= r.size))
      return problem(Issue::invalid_rule_count, r.keywords[0]);

    const ConditionSpec &c = r.when;
    if (c.kind == Condition::Kind::Always)
      continue;
    const std::size_t tested = index_of(c.keyword);
    if (tested == n)
      return problem(Issue::condition_on_undeclared_keyword, c.keyword);
    if (c.kind != Condition::Kind::Equals && c.kind != Condition::Kind::OneOf)
      continue;
    if (!comparable[tested])
      return problem(Issue::condition_on_multi_token_value, c.keyword);
    for (std::size_t v = 0; v < c.size; ++v)
      if (!may_equal_at(G::keys, tested, c.values[v]))
        return problem(Issue::invalid_condition_value, c.keyword, c.values[v]);
  }

  // Satisfiability and dead keys (unconditional rules only).
  std::array<std::array<bool, n>, n> conflict{};
  std::array<bool, n> mandatory = compulsory;
  std::array<bool, n> forbidden{};
  constexpr std::size_t rule_count =
      std::tuple_size_v<decltype(constraints_of<G>())>;
  std::array<std::pair<std::size_t, std::size_t>,
             rule_count * max_rule_keywords + 1>
      edges{};
  std::size_t edge_count = 0;
  for (const RuleSpec &r : rules) {
    if (r.when.kind != Condition::Kind::Always)
      continue;
    std::array<std::size_t, max_rule_keywords> ids{};
    for (std::size_t k = 0; k < r.size; ++k)
      ids[k] = index_of(r.keywords[k]);
    switch (r.kind) {
    case Rule::Kind::Required: mandatory[ids[0]] = true; break;
    case Rule::Kind::Forbidden: forbidden[ids[0]] = true; break;
    case Rule::Kind::Requires:
      for (std::size_t k = 1; k < r.size; ++k)
        edges[edge_count++] = {ids[0], ids[k]};
      break;
    case Rule::Kind::Conflicts:
      for (std::size_t k = 1; k < r.size; ++k)
        conflict[ids[0]][ids[k]] = conflict[ids[k]][ids[0]] = true;
      break;
    case Rule::Kind::Exactly:
    case Rule::Kind::AtMost:
      if (r.count == 1)
        for (std::size_t a = 0; a < r.size; ++a)
          for (std::size_t b = a + 1; b < r.size; ++b)
            conflict[ids[a]][ids[b]] = conflict[ids[b]][ids[a]] = true;
      break;
    case Rule::Kind::AtLeast:
    case Rule::Kind::RequiresAny:
    default: // no pairwise constraint (default: clean under -Wswitch-default)
      break;
    }
  }
  for (std::size_t e = 0; e < edge_count; ++e)
    if (conflict[edges[e].first][edges[e].second])
      return problem(Issue::requires_and_conflicts, names[edges[e].first],
                     names[edges[e].second]);
  for (std::size_t a = 0; a < n; ++a)
    for (std::size_t b = a + 1; b < n; ++b)
      if (mandatory[a] && mandatory[b] && conflict[a][b])
        return problem(Issue::compulsory_keywords_conflict, names[a], names[b]);

  // Transitive closure of the requirements (each closure contains the key).
  std::array<std::array<bool, n>, n> forced{};
  for (std::size_t i = 0; i < n; ++i) {
    std::array<std::size_t, n + 1> stack{};
    std::size_t top = 0;
    forced[i][i] = true;
    stack[top++] = i;
    while (top > 0) {
      const std::size_t current = stack[--top];
      for (std::size_t e = 0; e < edge_count; ++e)
        if (edges[e].first == current && !forced[i][edges[e].second]) {
          forced[i][edges[e].second] = true;
          stack[top++] = edges[e].second;
        }
    }
  }
  std::array<bool, n> always{};
  for (std::size_t m = 0; m < n; ++m)
    if (mandatory[m])
      for (std::size_t r = 0; r < n; ++r)
        always[r] = always[r] || forced[m][r];

  // dead[i] = index of the key that makes i unusable (n: usable).
  std::array<std::size_t, n> dead{};
  for (std::size_t i = 0; i < n; ++i) {
    dead[i] = n;
    for (std::size_t a = 0; a < n && dead[i] == n; ++a) {
      if (!forced[i][a])
        continue;
      if (forbidden[a])
        dead[i] = a;
      for (std::size_t b = 0; b < n && dead[i] == n; ++b) {
        if (b == a || !conflict[a][b])
          continue;
        if (forced[i][b] || always[b])
          dead[i] = (a == i) ? b : a;
      }
    }
  }
  for (std::size_t i = 0; i < n; ++i)
    if (always[i] && dead[i] != n)
      return problem(Issue::unsatisfiable_keyword, names[i], names[dead[i]]);

  for (const RuleSpec &r : rules) {
    if (r.when.kind != Condition::Kind::Always ||
        !(r.kind == Rule::Kind::Exactly || r.kind == Rule::Kind::AtLeast ||
          r.kind == Rule::Kind::AtMost))
      continue;
    std::size_t usable = 0;
    std::size_t present = 0;
    for (std::size_t k = 0; k < r.size; ++k) {
      const std::size_t id = index_of(r.keywords[k]);
      usable += dead[id] == n ? std::size_t{1} : std::size_t{0};
      present += always[id] ? std::size_t{1} : std::size_t{0};
    }
    if (r.kind != Rule::Kind::AtMost && usable < r.count)
      return problem(Issue::unsatisfiable_rule, r.keywords[0]);
    if (r.kind != Rule::Kind::AtLeast && present > r.count)
      return problem(Issue::unsatisfiable_rule, r.keywords[0]);
  }
  for (std::size_t i = 0; i < n; ++i)
    if (dead[i] != n)
      return problem(Issue::dead_keyword, names[i], names[dead[i]]);
  return {};
}

/**
 * \brief Turns a problem into a compiler error. The keys involved appear in
 * the template arguments of `Report` in the compiler message, e.g.
 * `Report<Issue::dead_keyword, "@Temperature", "@Pressure">`.
 */
template <Issue I, fixed_string Keyword, fixed_string Other> struct Report {
  static_assert(I != Issue::duplicate_keyword,
                "typed grammar: two keys have the same name (see the template arguments of Report)");
  static_assert(I != Issue::compulsory_keyword_with_default,
                "typed grammar: a compulsory key cannot have a default");
  static_assert(I != Issue::rule_on_undeclared_keyword,
                "typed grammar: a rule names a key that is not in 'keys'");
  static_assert(I != Issue::keyword_listed_twice_in_rule,
                "typed grammar: a rule names the same key twice (or a key "
                "requires or conflicts with itself)");
  static_assert(I != Issue::rule_needs_two_keywords,
                "typed grammar: a counting rule must name at least two keys");
  static_assert(I != Issue::invalid_rule_count,
                "typed grammar: invalid count (Exactly/AtLeast: 1..number of "
                "keys; AtMost: 1..number of keys - 1)");
  static_assert(I != Issue::condition_on_undeclared_keyword,
                "typed grammar: a condition tests a key that is not in 'keys'");
  static_assert(I != Issue::condition_on_multi_token_value,
                "typed grammar: Equals/OneOf need a single-token key "
                "(integer, boolean, word, enumeration, reference)");
  static_assert(I != Issue::invalid_condition_value,
                "typed grammar: a condition compares a key with a value it can "
                "never have (see 'other')");
  static_assert(I != Issue::requires_and_conflicts,
                "typed grammar: a key requires a key it conflicts with");
  static_assert(I != Issue::compulsory_keywords_conflict,
                "typed grammar: two always-required keys conflict");
  static_assert(I != Issue::unsatisfiable_keyword,
                "typed grammar: an always-required key can never be used");
  static_assert(I != Issue::unsatisfiable_rule,
                "typed grammar: a counting rule can never be satisfied");
  static_assert(I != Issue::dead_keyword,
                "typed grammar: a key can never be used (see 'other' for the "
                "key that excludes it)");
  static constexpr bool ok = true;
};

/** \brief Runs `check<G>()` and reports its problem, if any. */
template <class G> struct Coherence {
  static constexpr Problem problem = check<G>();
  static constexpr auto keyword =
      fixed_string<problem.keyword.view().size() + 1>(problem.keyword.view());
  static constexpr auto other =
      fixed_string<problem.other.view().size() + 1>(problem.other.view());
  static constexpr bool ok = Report<problem.issue, keyword, other>::ok;
};

/** \brief Unique identity of a typed grammar. */
template <class G> inline constexpr char grammar_tag = 0;

template <class G>
concept has_display_name = requires {
  { G::display_name } -> std::convertible_to<std::string_view>;
};
} // namespace detail

/**
 * \brief Base of typed grammars (CRTP).
 *
 * The derived class lists `static constexpr auto keys = Keys(...);` and,
 * optionally, `static constexpr auto constraints = Rules(...);` and
 * `static constexpr std::string_view display_name = "...";` (used in
 * messages). The grammar is checked at compile time when it is used, and
 * translated into keyword declarations and rules when it is attached.
 */
template <class Derived> class Grammar : public DictionaryGrammar {
public:
  Grammar() { static_assert(detail::Coherence<Derived>::ok); }

protected:
  void DefineRules() override {
    std::apply([this](const auto &...k) { (AddKeyWord(k.keyword()), ...); },
               Derived::keys);
    for (const RuleSpec &r : detail::constraints_of<Derived>())
      AddRule(r.rule());
  }
};

/** \brief A typed grammar (derived from `Grammar<G>`). */
template <class G>
concept TypedGrammar = std::is_base_of_v<Grammar<G>, G> && requires {
  G::keys;
};

/** \brief Compile-time coherence of a typed grammar (`Issue::none` if
 * coherent); usable in `static_assert`. */
template <TypedGrammar G> [[nodiscard]] constexpr auto check() -> Problem {
  return detail::check<G>();
}

/** \brief Name of a typed grammar used in messages. */
template <class G> [[nodiscard]] auto grammar_name() -> std::string {
  if constexpr (detail::has_display_name<G>)
    return std::string(G::display_name);
  else
    return typeid(G).name();
}

namespace detail {
template <class G, class K>
inline constexpr bool declares = []<class... Ks>(std::type_identity<std::tuple<Ks...>>) {
  return (std::is_same_v<K, Ks> || ...);
}(std::type_identity<std::remove_cvref_t<decltype(G::keys)>>{});

template <class G>
[[nodiscard]] constexpr auto declares_name(std::string_view name) -> bool {
  return std::apply([&](const auto &...k) { return ((k.name() == name) || ...); },
                    G::keys);
}
} // namespace detail

// ---------------------------------------------------------------------------
// Views
// ---------------------------------------------------------------------------

/**
 * \brief Typed access to a dictionary validated with the typed grammar `G`.
 *
 * Obtained from `DictionaryManager::view<G>(name)` (or `attach<G>(dict)`).
 * Only keys of `G` can be read, and the type of a value is the type
 * declared by its key. A view is a cheap handle: copy it freely while the
 * manager (or dictionary) is alive.
 */
template <class G> class View {
  static_assert(TypedGrammar<G>,
                "View<G>: G must be a typed grammar (struct G : "
                "OpenSMOKEpp::typed::Grammar<G> { static constexpr auto keys "
                "= Keys(...); })");
  static_assert(detail::Coherence<G>::ok);

public:
  /** \brief View of a dictionary that already has grammar `G` attached. */
  explicit View(Dictionary &dictionary, DictionaryManager *manager = nullptr)
      : dictionary_(&dictionary), manager_(manager) {}

  /**
   * \brief Reads a key; the type is deduced from its declaration.
   *
   * A quantity is returned in SI units; a reference is returned as a
   * `View` of the referenced dictionary. An absent key returns its
   * default.
   *
   * \warning Throws `DictionaryError` if the key is absent and has no
   * default.
   */
  template <fixed_string N, class S>
  [[nodiscard]] auto get(const KeySpec<N, S> &key) const -> typename S::type {
    assert_declared<N, S>();
    return read_value(key);
  }

  /**
   * \brief Reads a key whose measures take their dimension when read
   * (declared with `Quantity()` or `QuantityList()`, also inside a list,
   * record or choice). The units are checked against `dimension` and the
   * values are converted to SI.
   *
   * \warning Throws `DictionaryError` if the key is absent and has no
   * default, or (located at the units) if the units do not have `dimension`.
   */
  template <fixed_string N, class S>
  [[nodiscard]] auto get(const KeySpec<N, S> &key,
                         const units::Dimension &dimension) const ->
      typename S::type {
    assert_declared<N, S>();
    static_assert(detail::needs_dimension<S>::value,
                  "View::get(key, dimension): the dimension of this key is "
                  "fixed by the grammar; use get(key)");
    return dictionary_->template Read<typename S::type>(key.name(), dimension);
  }

  /** \brief As `get(key, dimension)`, or `std::nullopt` if the key is absent
   * and has no default. */
  template <fixed_string N, class S>
  [[nodiscard]] auto get_if(const KeySpec<N, S> &key,
                            const units::Dimension &dimension) const
      -> std::optional<typename S::type> {
    if (!dictionary_->CheckOption(key.name()) &&
        !std::get<KeySpec<N, S>>(G::keys).default_)
      return std::nullopt;
    return get(key, dimension);
  }

  /** \brief The value of a key, or `std::nullopt` if it is absent and has no
   * default. */
  template <fixed_string N, class S>
  [[nodiscard]] auto get_if(const KeySpec<N, S> &key) const
      -> std::optional<typename S::type> {
    assert_declared<N, S>();
    if (!dictionary_->CheckOption(key.name()) &&
        !std::get<KeySpec<N, S>>(G::keys).default_)
      return std::nullopt;
    return read_value(key);
  }

  /** \brief The value of a key, or `fallback` if it is absent and has no
   * default. */
  template <fixed_string N, class S>
  [[nodiscard]] auto get_or(const KeySpec<N, S> &key,
                            typename S::type fallback) const -> typename S::type {
    auto value = get_if(key);
    return value ? std::move(*value) : std::move(fallback);
  }

  /**
   * \brief Reads a key into `out`. Does not compile if the type of `out` is
   * not exactly the type of the key (no silent narrowing).
   */
  template <fixed_string N, class S, class T>
  void read(const KeySpec<N, S> &key, T &out) const {
    static_assert(std::is_same_v<T, typename S::type>,
                  "View::read: the variable does not have the type declared "
                  "by the key (see KeySpec<...>::value_type)");
    out = get(key);
  }

  /**
   * \brief Reads a key into `out` only if it is present (or has a default);
   * otherwise `out` keeps its value. Returns whether `out` was assigned.
   * Does not compile if the type of `out` is not exactly the type of the key.
   *
   * Replaces the legacy idiom `if (CheckOption(k)) Read(k, out);`, keeping
   * the value already in `out` (e.g. set by a constructor) as the default.
   */
  template <fixed_string N, class S, class T>
  auto read_if(const KeySpec<N, S> &key, T &out) const -> bool {
    static_assert(std::is_same_v<T, typename S::type>,
                  "View::read_if: the variable does not have the type "
                  "declared by the key (see KeySpec<...>::value_type)");
    auto value = get_if(key);
    if (!value)
      return false;
    out = std::move(*value);
    return true;
  }

  /** \brief Whether the key is written in the file. */
  template <fixed_string N, class S>
  [[nodiscard]] auto has(const KeySpec<N, S> &key) const -> bool {
    assert_declared<N, S>();
    return dictionary_->CheckOption(key.name());
  }

  /** \brief The underlying dictionary (locations, warnings). */
  [[nodiscard]] auto dictionary() const -> Dictionary & { return *dictionary_; }

  /** \brief The dictionary name. */
  [[nodiscard]] auto name() const -> std::string { return dictionary_->name(); }

private:
  Dictionary *dictionary_;
  DictionaryManager *manager_;

  template <fixed_string N, class S> static constexpr void assert_declared() {
    static_assert(detail::declares_name<G>(N.view()),
                  "View::get: the key is not declared in this grammar (see the "
                  "keyword name in the template arguments)");
    static_assert(!detail::declares_name<G>(N.view()) ||
                      detail::declares<G, KeySpec<N, S>>,
                  "View::get: the grammar declares this keyword with a "
                  "different value type or properties; read it with the key "
                  "object listed in the grammar");
  }

  template <fixed_string N, class S>
  auto read_value(const KeySpec<N, S> &key) const -> typename S::type {
    static_assert(!detail::needs_dimension<S>::value,
                  "View::get: the measures of this key take their dimension "
                  "when it is read; use get(key, dimension)");
    if constexpr (detail::is_reference<S>::value) {
      const auto target = dictionary_->template Read<std::string>(key.name());
      return manager().template view<typename S::grammar>(
          target, dictionary_->location_of(key.name()));
    } else if constexpr (detail::is_reference_list<S>::value) {
      const auto targets =
          dictionary_->template Read<std::vector<std::string>>(key.name());
      std::vector<View<typename S::grammar>> views;
      views.reserve(targets.size());
      for (const auto &target : targets)
        views.push_back(manager().template view<typename S::grammar>(
            target, dictionary_->location_of(key.name())));
      return views;
    } else if constexpr (std::is_same_v<typename S::type, std::filesystem::path>) {
      return std::filesystem::path(
          dictionary_->template Read<std::string>(key.name()));
    } else if constexpr (std::is_same_v<typename S::type, Units>) {
      Units units = dictionary_->template Read<Units>(key.name());
      units.system = dictionary_->unit_system();
      units.location = dictionary_->location_of(key.name());
      return units;
    } else if constexpr (std::is_same_v<typename S::type, std::vector<Units>>) {
      auto list = dictionary_->template Read<std::vector<Units>>(key.name());
      for (auto &units : list) {
        units.system = dictionary_->unit_system();
        units.location = dictionary_->location_of(key.name());
      }
      return list;
    } else {
      return dictionary_->template Read<typename S::type>(key.name());
    }
  }

  [[nodiscard]] auto manager() const -> DictionaryManager & {
    if (manager_ == nullptr)
      throw DictionaryError(
          {Diagnostic{Severity::Error, DiagnosticCode::NoGrammar,
                      dictionary_->location(),
                      "this view cannot follow references to other "
                      "dictionaries: create it with DictionaryManager::view",
                      {}, {}}},
          "Dictionary " + dictionary_->name());
    return *manager_;
  }
};

/**
 * \brief Attaches the typed grammar `G` to a dictionary and returns a view.
 *
 * Use `DictionaryManager::view<G>` instead when the dictionary references
 * other dictionaries.
 *
 * \warning Throws `DictionaryError` if the dictionary is not valid.
 */
template <TypedGrammar G> [[nodiscard]] auto attach(Dictionary &dictionary) -> View<G> {
  G grammar;
  dictionary.SetGrammar(grammar);
  return View<G>(dictionary);
}

/** \brief The runtime grammar equivalent to the typed grammar `G` (for
 * documentation: `Describe()`, `AvailableKeyWordsTable()`). */
template <TypedGrammar G> [[nodiscard]] auto runtime_grammar() -> DictionaryGrammar {
  struct Builder : G {
    void Build() { this->UserDefined(); }
  };
  Builder builder;
  builder.Build();
  return static_cast<const DictionaryGrammar &>(builder);
}

namespace detail {
/** \brief State of `DictionaryManager::validate`. */
struct Walk {
  DictionaryManager *manager = nullptr;
  std::vector<std::string> stack;
  std::vector<std::string> visited; // validated with a typed grammar
  std::vector<std::string> named;   // reached through DictionaryName keys
  std::vector<Diagnostic> diagnostics;

  template <class G> void visit(const std::string &name, const SourceLocation &from);

  /**
   * A dictionary named by a DictionaryName key: it must exist; it is not
   * validated (its grammar is not typed), and the dictionaries whose names
   * appear among its values are taken as referenced too.
   */
  void follow_name(const std::string &target, const SourceLocation &from) {
    try {
      (void)manager->Require(target, from);
    } catch (const DictionaryError &error) {
      for (const auto &d : error.diagnostics())
        diagnostics.push_back(d);
      return;
    }
    for (const auto &n : named)
      if (n == target)
        return;
    named.push_back(target);
    const Dictionary &dictionary = (*manager)(target);
    for (const auto &entry : dictionary.entries())
      for (const auto &value : entry.values)
        if (value != target && manager->contains(value))
          follow_name(value, dictionary.location_of(entry.keyword));
  }
};

template <class G>
void Walk::visit(const std::string &name, const SourceLocation &from) {
  for (std::size_t i = 0; i < stack.size(); ++i)
    if (stack[i] == name) {
      std::string path;
      for (std::size_t j = i; j < stack.size(); ++j)
        path += stack[j] + " -> ";
      diagnostics.push_back(Diagnostic{
          Severity::Error, DiagnosticCode::ReferenceCycle, from,
          "the reference to the dictionary '" + name + "' forms a cycle: " +
              path + name,
          {}, {}});
      return;
    }
  bool first_visit = true;
  for (const auto &v : visited)
    first_visit = first_visit && v != name;
  if (first_visit)
    visited.push_back(name);
  std::optional<View<G>> view;
  try {
    view.emplace(manager->template view<G>(name, from));
  } catch (const DictionaryError &error) {
    for (const auto &d : error.diagnostics())
      diagnostics.push_back(d);
    return;
  }
  if (!first_visit)
    return; // already walked from another reference (same grammar)
  stack.push_back(name);
  Dictionary &dictionary = view->dictionary();
  std::apply(
      [&](const auto &...key) {
        (
            [&](const auto &k) {
              using S = typename std::remove_cvref_t<decltype(k)>::spec_type;
              if constexpr (std::is_same_v<S, DictionaryNameSpec>) {
                if (!dictionary.CheckOption(k.name()) && !k.default_)
                  return;
                follow_name(dictionary.template Read<std::string>(k.name()),
                            dictionary.location_of(k.name()));
              } else if constexpr (is_reference<S>::value || is_reference_list<S>::value) {
                if (!dictionary.CheckOption(k.name()) && !k.default_)
                  return;
                std::vector<std::string> targets;
                if constexpr (is_reference<S>::value)
                  targets.push_back(dictionary.template Read<std::string>(k.name()));
                else
                  targets = dictionary.template Read<std::vector<std::string>>(k.name());
                for (const auto &target : targets)
                  visit<typename S::grammar>(target, dictionary.location_of(k.name()));
              }
            }(key),
            ...);
      },
      G::keys);
  stack.pop_back();
}
} // namespace detail
} // namespace OpenSMOKEpp::typed

namespace OpenSMOKEpp {
template <class G>
auto DictionaryManager::view(std::string_view name, const SourceLocation &from)
    -> typed::View<G> {
  static_assert(typed::TypedGrammar<G>,
                "DictionaryManager::view<G>: G must be a typed grammar");
  Dictionary &dictionary = AttachGrammar(
      name, &typed::detail::grammar_tag<G>, typed::grammar_name<G>(),
      [](Dictionary &d) {
        G grammar;
        d.SetGrammar(grammar);
      },
      from);
  return typed::View<G>(dictionary, this);
}

template <class G>
auto DictionaryManager::validate(std::string_view root) -> std::vector<Diagnostic> {
  static_assert(typed::TypedGrammar<G>,
                "DictionaryManager::validate<G>: G must be a typed grammar");
  typed::detail::Walk walk;
  walk.manager = this;
  walk.template visit<G>(std::string(root), {});
  std::vector<std::string> referenced = walk.visited;
  referenced.insert(referenced.end(), walk.named.begin(), walk.named.end());
  return FinishValidation(std::move(walk.diagnostics), referenced);
}
} // namespace OpenSMOKEpp

#endif // OpenSMOKEpp_DictionaryTyped_H
