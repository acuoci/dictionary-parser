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

#ifndef OpenSMOKEpp_DictionaryFormats_H
#define OpenSMOKEpp_DictionaryFormats_H

#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "DictionaryUnits.h"

namespace OpenSMOKEpp {
/**
 * \brief Parsed value of a keyword, independent of any C++ type.
 *
 * Scalars carry their value; records, lists, tables and lists of measures
 * are `Sequence`s of items. Quantities are stored in SI units (amount in
 * kmol by default, see `units::UnitSystem`).
 */
struct Value {
  /** \brief Kind of value. */
  enum class Kind {
    None,     ///< No value (keyword of type `none`).
    Integer,  ///< `integer`; also the code of an enumeration value.
    Real,     ///< `real`; quantities (in SI units).
    Boolean,  ///< `boolean`.
    Text,     ///< `word`, `text`, enumeration value (canonical spelling).
    Sequence  ///< record, list, table, list of measures.
  };

  /** \brief Kind of value. */
  Kind kind = Kind::None;
  /** \brief Integer value, or enumeration code. */
  long long integer = 0;
  /** \brief Real value (SI units for quantities). */
  double real = 0.0;
  /** \brief Boolean value. */
  bool boolean = false;
  /** \brief Text value, or canonical spelling of an enumeration value. */
  std::string text;
  /** \brief Items of a sequence. */
  std::vector<Value> items;
  /** \brief For an enumeration value: `true` (it also has `integer` and
   * `text`). */
  bool enumeration = false;
  /** \brief For the value of a `Choice`: index of the matched alternative. */
  std::size_t alternative = 0;
};

struct ValueFormatField;

/**
 * \brief Description of the token layout of a keyword value.
 *
 * Formats are immutable and cheap to copy. They are built with the factory
 * functions in `OpenSMOKEpp::formats` and refined with the modifiers below,
 * which return a modified copy:
 *
 * \code
 * using namespace OpenSMOKEpp::formats;
 * // @InternalConnections 3 7 250.3 kg/s  4 8 1.2 kg/s;
 * auto connections = List(Record({{"from", Integer().Minimum(1)},
 *                                 {"to", Integer().Minimum(1)},
 *                                 {"flow", Quantity(units::dim::MassFlowRate)}}));
 * // @Profile s K   0 300   1 500   2 800;
 * auto profile = Table({{"time", units::dim::Time},
 *                       {"temperature", units::dim::Temperature}});
 * // @OutputSpecies ALL;  or  @OutputSpecies CH4 O2 CO2;
 * auto species = Choice({Enumeration({"ALL", "NONE"}), List(Word())});
 * \endcode
 */
class ValueFormat {
public:
  /** \brief Kind of format. */
  enum class Kind {
    Integer,      ///< one integer token
    Real,         ///< one finite number
    Boolean,      ///< true/false/on/off (case-insensitive)
    Word,         ///< one token (string, name, dictionary reference)
    Text,         ///< all remaining tokens, joined by blanks
    Enumeration,  ///< one token among a fixed set (with aliases)
    Quantity,     ///< number followed by units of a given dimension
    Record,       ///< fixed sequence of named fields
    List,         ///< repetition of a fixed-width element
    QuantityList, ///< measures: pairs or numbers followed by one unit
    Table,        ///< units header (if any column has a dimension) + rows
    Choice,       ///< the first alternative that matches
    Unit          ///< one units token (of a dimension, if given)
  };

  /** \brief One named field of a record (see `ValueFormatField`). */
  using Field = ValueFormatField;

  /** \brief One column of a table. */
  struct Column {
    /** \brief Column name, used in messages and documentation. */
    std::string name;
    /** \brief Dimension of the column; none for a dimensionless number. */
    std::optional<units::Dimension> dimension;
  };

  /** \brief One value of an enumeration. */
  struct EnumValue {
    /** \brief Accepted spelling. */
    std::string name;
    /** \brief Code returned for this value (index of the canonical value). */
    long long code = 0;
    /** \brief Canonical spelling (differs from `name` for aliases). */
    std::string canonical;
    /** \brief If not empty, the value is rejected with this reason. */
    std::string unavailable;
  };

  /** \brief Numeric bound. */
  struct Bound {
    /** \brief Bound value (SI units for quantities). */
    double value = 0.0;
    /** \brief Whether the bound itself is allowed. */
    bool inclusive = true;
  };

  /** \brief Kind of format. */
  [[nodiscard]] Kind kind() const noexcept;
  /** \brief Fields of a record. */
  [[nodiscard]] const std::vector<Field> &fields() const noexcept;
  /** \brief Element of a list. */
  [[nodiscard]] const ValueFormat &element() const;
  /** \brief Columns of a table. */
  [[nodiscard]] const std::vector<Column> &columns() const noexcept;
  /** \brief Alternatives of a choice. */
  [[nodiscard]] const std::vector<ValueFormat> &alternatives() const noexcept;
  /** \brief Values (and aliases) of an enumeration. */
  [[nodiscard]] const std::vector<EnumValue> &enum_values() const noexcept;
  /** \brief Dimension of a quantity or list of quantities. */
  [[nodiscard]] const std::optional<units::Dimension> &dimension() const noexcept;
  /** \brief Lower bound of a number or quantity. */
  [[nodiscard]] const std::optional<Bound> &minimum() const noexcept;
  /** \brief Upper bound of a number or quantity. */
  [[nodiscard]] const std::optional<Bound> &maximum() const noexcept;
  /** \brief Minimum number of elements (lists, tables, lists of measures). */
  [[nodiscard]] std::size_t min_size() const noexcept;
  /** \brief Maximum number of elements (lists, tables, lists of measures). */
  [[nodiscard]] std::size_t max_size() const noexcept;
  /**
   * \brief Whether a quantity or list of measures takes its dimension when
   * the value is read (see `WithDimension`).
   */
  [[nodiscard]] bool runtime_dimension() const noexcept;
  /** \brief Whether a list ends with one units token for its numbers
   * (`Composition(dimension)`). */
  [[nodiscard]] bool trailing_unit() const noexcept;
  /** \brief Whether this format, or a nested one, takes its dimension when
   * the value is read. */
  [[nodiscard]] bool needs_dimension() const;
  /**
   * \brief Copy in which the quantities and lists of measures without a
   * dimension (`Quantity()`, `QuantityList()`) take `dimension`.
   */
  [[nodiscard]] ValueFormat WithDimension(const units::Dimension &dimension) const;

  /** \brief Copy with a lower bound (numbers and quantities, SI units). */
  [[nodiscard]] ValueFormat Minimum(double value, bool inclusive = true) const;
  /** \brief Copy with an upper bound (numbers and quantities, SI units). */
  [[nodiscard]] ValueFormat Maximum(double value, bool inclusive = true) const;
  /** \brief Copy with size limits (lists, tables, lists of measures). */
  [[nodiscard]] ValueFormat Size(std::size_t minimum, std::size_t maximum) const;
  /** \brief Copy with an exact size (lists, tables, lists of measures). */
  [[nodiscard]] ValueFormat Size(std::size_t n) const { return Size(n, n); }
  /** \brief Copy of an enumeration with an extra spelling for a value. */
  [[nodiscard]] ValueFormat Alias(std::string alias, std::string value) const;
  /**
   * \brief Copy of an enumeration in which a value is known but rejected,
   * e.g. a solver not available in this build.
   */
  [[nodiscard]] ValueFormat Disable(std::string value, std::string reason) const;

  /**
   * \brief Number of tokens consumed, if fixed (e.g. 2 for a quantity).
   */
  [[nodiscard]] std::optional<std::size_t> width() const;

  /** \brief Human-readable description, e.g. `list of (from: integer, ...)`. */
  [[nodiscard]] std::string describe(const units::UnitSystem &system =
                                         units::si_kmol) const;

  /**
   * \brief Problems in the format itself (e.g. a list of variable-width
   * elements, a bound on a word, an empty enumeration).
   */
  [[nodiscard]] std::vector<std::string> problems() const;

  struct Node;
  /** \brief Internal constructor used by the factory functions. */
  explicit ValueFormat(std::shared_ptr<const Node> node);

private:
  std::shared_ptr<const Node> node_;
};

/**
 * \brief One named field of a record.
 */
struct ValueFormatField {
  /** \brief Field name, used in messages and documentation. */
  std::string name;
  /** \brief Format of the field. */
  ValueFormat format;
};

/**
 * \brief Factory functions for value formats.
 */
namespace formats {
/** \brief One integer. */
[[nodiscard]] auto Integer() -> ValueFormat;
/** \brief One finite number. */
[[nodiscard]] auto Real() -> ValueFormat;
/** \brief One boolean (true/false/on/off). */
[[nodiscard]] auto Boolean() -> ValueFormat;
/** \brief One token (possibly quoted). */
[[nodiscard]] auto Word() -> ValueFormat;
/** \brief All remaining tokens joined by blanks (free text, paths). */
[[nodiscard]] auto Text() -> ValueFormat;
/** \brief One token among `values`; the code of a value is its index. */
[[nodiscard]] auto Enumeration(std::vector<std::string> values) -> ValueFormat;
/** \brief A number followed by units of the given dimension (SI value). */
[[nodiscard]] auto Quantity(const units::Dimension &dimension) -> ValueFormat;
/**
 * \brief A number followed by units whose dimension is chosen when the value
 * is read (`ValueFormat::WithDimension`). Before that, the units must only be
 * valid units of some dimension.
 */
[[nodiscard]] auto Quantity() -> ValueFormat;
/** \brief A fixed sequence of named fields; only the last field may have a
 * variable width. */
[[nodiscard]] auto Record(std::vector<ValueFormat::Field> fields) -> ValueFormat;
/** \brief Repetition of a fixed-width element (at least `minimum`). */
[[nodiscard]] auto List(ValueFormat element, std::size_t minimum = 1,
                        std::size_t maximum = std::numeric_limits<std::size_t>::max())
    -> ValueFormat;
/** \brief Measures of one dimension: `1 m 20 cm` or `1 2 3 m` (SI values). */
[[nodiscard]] auto QuantityList(const units::Dimension &dimension)
    -> ValueFormat;
/** \brief Measures whose dimension is chosen when the value is read (see
 * `Quantity()`). */
[[nodiscard]] auto QuantityList() -> ValueFormat;
/** \brief One units token, e.g. `cm` (of the given dimension, if any). */
[[nodiscard]] auto Unit() -> ValueFormat;
/** \brief One units token of the given dimension. */
[[nodiscard]] auto Unit(const units::Dimension &dimension) -> ValueFormat;
/**
 * \brief Rows of numbers. If any column has a dimension, the value starts
 * with one unit per column (`-` for dimensionless columns), e.g.
 * `@Profile s K  0 300  1 500;`. Values are converted to SI.
 */
[[nodiscard]] auto Table(std::vector<ValueFormat::Column> columns)
    -> ValueFormat;
/** \brief The first alternative that matches the whole value. */
[[nodiscard]] auto Choice(std::vector<ValueFormat> alternatives) -> ValueFormat;
/** \brief Pairs of a name and a number, e.g. `CH4 0.5 O2 0.5`. */
[[nodiscard]] auto Composition() -> ValueFormat;
/**
 * \brief Pairs of a name and a number followed by the units of the numbers,
 * e.g. `OH 1e-5 HO2 2e-5 mol/cm3`; the numbers are converted to SI.
 */
[[nodiscard]] auto Composition(const units::Dimension &dimension) -> ValueFormat;
} // namespace formats

// ---------------------------------------------------------------------------
// Conversion of values to C++ types (used by Dictionary::Read<T>)
// ---------------------------------------------------------------------------
namespace detail {
/** \brief A value does not have the shape requested by a C++ type. */
class ValueShapeError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

template <typename T, typename = void> struct ValueConverter {
  static_assert(sizeof(T) == 0,
                "Dictionary::Read<T>: unsupported type. Supported: bool, "
                "integers, floating-point types, std::string, enums, "
                "std::vector, std::array, std::pair, std::tuple, std::variant");
};

[[noreturn]] inline void shape_error(const char *expected, const Value &value) {
  static constexpr const char *names[] = {"no value", "an integer", "a number",
                                          "a boolean", "a text", "a sequence"};
  throw ValueShapeError(std::string("expected ") + expected + ", but the value is " +
                        names[static_cast<int>(value.kind)]);
}

template <> struct ValueConverter<bool> {
  static auto from(const Value &v) -> bool {
    if (v.kind != Value::Kind::Boolean)
      shape_error("a boolean", v);
    return v.boolean;
  }
};

template <typename T>
struct ValueConverter<T, std::enable_if_t<std::is_integral_v<T> &&
                                          !std::is_same_v<T, bool>>> {
  static auto from(const Value &v) -> T {
    if (v.kind != Value::Kind::Integer && !v.enumeration)
      shape_error("an integer", v);
    if (v.integer < static_cast<long long>(std::numeric_limits<T>::min()) ||
        (v.integer > 0 &&
         static_cast<unsigned long long>(v.integer) >
             static_cast<unsigned long long>(std::numeric_limits<T>::max())))
      throw ValueShapeError("the integer " + std::to_string(v.integer) +
                            " does not fit the requested type");
    return static_cast<T>(v.integer);
  }
};

template <typename T>
struct ValueConverter<T, std::enable_if_t<std::is_floating_point_v<T>>> {
  static auto from(const Value &v) -> T {
    if (v.kind == Value::Kind::Real)
      return static_cast<T>(v.real);
    if (v.kind == Value::Kind::Integer && !v.enumeration)
      return static_cast<T>(v.integer);
    shape_error("a number", v);
  }
};

template <typename T>
struct ValueConverter<T, std::enable_if_t<std::is_enum_v<T>>> {
  static auto from(const Value &v) -> T {
    if (!v.enumeration)
      shape_error("an enumeration value", v);
    return static_cast<T>(v.integer);
  }
};

template <> struct ValueConverter<std::string> {
  static auto from(const Value &v) -> std::string {
    if (v.kind != Value::Kind::Text)
      shape_error("a text", v);
    return v.text;
  }
};

template <typename T, typename A> struct ValueConverter<std::vector<T, A>> {
  static auto from(const Value &v) -> std::vector<T, A> {
    if (v.kind != Value::Kind::Sequence)
      shape_error("a sequence", v);
    std::vector<T, A> result;
    result.reserve(v.items.size());
    for (const auto &item : v.items)
      result.push_back(ValueConverter<T>::from(item));
    return result;
  }
};

template <typename T, std::size_t N> struct ValueConverter<std::array<T, N>> {
  static auto from(const Value &v) -> std::array<T, N> {
    if (v.kind != Value::Kind::Sequence || v.items.size() != N)
      throw ValueShapeError("expected a sequence of " + std::to_string(N) +
                            " items");
    std::array<T, N> result{};
    for (std::size_t i = 0; i < N; ++i)
      result[i] = ValueConverter<T>::from(v.items[i]);
    return result;
  }
};

template <typename... Ts> struct ValueConverter<std::tuple<Ts...>> {
  static auto from(const Value &v) -> std::tuple<Ts...> {
    if (v.kind != Value::Kind::Sequence || v.items.size() != sizeof...(Ts))
      throw ValueShapeError("expected a sequence of " +
                            std::to_string(sizeof...(Ts)) + " items");
    return build(v, std::index_sequence_for<Ts...>{});
  }
  template <std::size_t... I>
  static auto build(const Value &v, std::index_sequence<I...>)
      -> std::tuple<Ts...> {
    return std::tuple<Ts...>(ValueConverter<Ts>::from(v.items[I])...);
  }
};

template <typename A, typename B> struct ValueConverter<std::pair<A, B>> {
  static auto from(const Value &v) -> std::pair<A, B> {
    if (v.kind != Value::Kind::Sequence || v.items.size() != 2)
      throw ValueShapeError("expected a sequence of 2 items");
    return {ValueConverter<A>::from(v.items[0]),
            ValueConverter<B>::from(v.items[1])};
  }
};

template <typename... Ts> struct ValueConverter<std::variant<Ts...>> {
  static auto from(const Value &v) -> std::variant<Ts...> {
    if (v.alternative >= sizeof...(Ts))
      throw ValueShapeError("the matched alternative " +
                            std::to_string(v.alternative) +
                            " has no corresponding variant type");
    return pick<0>(v);
  }
  template <std::size_t I> static auto pick(const Value &v) -> std::variant<Ts...> {
    if constexpr (I < sizeof...(Ts)) {
      if (v.alternative == I) {
        using U = std::variant_alternative_t<I, std::variant<Ts...>>;
        return std::variant<Ts...>(std::in_place_index<I>,
                                   ValueConverter<U>::from(v));
      }
      return pick<I + 1>(v);
    } else {
      throw ValueShapeError("invalid alternative");
    }
  }
};
} // namespace detail
} // namespace OpenSMOKEpp

#endif // OpenSMOKEpp_DictionaryFormats_H
