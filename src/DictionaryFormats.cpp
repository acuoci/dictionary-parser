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

#include "DictionaryFormats.h"
#include "DictionaryUtilities.h"
#include "DictionaryValues.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <utility>

namespace OpenSMOKEpp {
namespace {
constexpr std::size_t unlimited = std::numeric_limits<std::size_t>::max();

[[nodiscard]] auto format_number(const double value) -> std::string {
  std::ostringstream out;
  out << value;
  return out.str();
}

[[nodiscard]] auto join(const std::vector<std::string> &items,
                        const char *separator) -> std::string {
  std::string text;
  for (std::size_t i = 0; i < items.size(); ++i)
    text += (i == 0 ? "" : separator) + items[i];
  return text;
}

[[nodiscard]] auto kind_name(const ValueFormat::Kind kind) -> std::string {
  switch (kind) {
  case ValueFormat::Kind::Integer: return "integer";
  case ValueFormat::Kind::Real: return "number";
  case ValueFormat::Kind::Boolean: return "boolean";
  case ValueFormat::Kind::Word: return "word";
  case ValueFormat::Kind::Text: return "text";
  case ValueFormat::Kind::Enumeration: return "enumeration";
  case ValueFormat::Kind::Quantity: return "quantity";
  case ValueFormat::Kind::Record: return "record";
  case ValueFormat::Kind::List: return "list";
  case ValueFormat::Kind::QuantityList: return "list of measures";
  case ValueFormat::Kind::Table: return "table";
  case ValueFormat::Kind::Choice: return "choice";
  }
  return "format";
}
} // namespace

struct ValueFormat::Node {
  Kind kind = Kind::Word;
  std::vector<Field> fields;
  std::optional<ValueFormat> element;
  std::vector<Column> columns;
  std::vector<ValueFormat> alternatives;
  std::vector<EnumValue> enum_values;
  std::optional<units::Dimension> dimension;
  std::optional<Bound> minimum;
  std::optional<Bound> maximum;
  std::size_t min_size = 0;
  std::size_t max_size = unlimited;
  std::vector<std::string> errors; // misuse of modifiers
};

ValueFormat::ValueFormat(std::shared_ptr<const Node> node)
    : node_(std::move(node)) {}

auto ValueFormat::kind() const noexcept -> Kind { return node_->kind; }
auto ValueFormat::fields() const noexcept -> const std::vector<Field> & {
  return node_->fields;
}
auto ValueFormat::element() const -> const ValueFormat & {
  if (!node_->element)
    throw std::logic_error("ValueFormat::element: not a list");
  return *node_->element;
}
auto ValueFormat::columns() const noexcept -> const std::vector<Column> & {
  return node_->columns;
}
auto ValueFormat::alternatives() const noexcept
    -> const std::vector<ValueFormat> & {
  return node_->alternatives;
}
auto ValueFormat::enum_values() const noexcept
    -> const std::vector<EnumValue> & {
  return node_->enum_values;
}
auto ValueFormat::dimension() const noexcept
    -> const std::optional<units::Dimension> & {
  return node_->dimension;
}
auto ValueFormat::minimum() const noexcept -> const std::optional<Bound> & {
  return node_->minimum;
}
auto ValueFormat::maximum() const noexcept -> const std::optional<Bound> & {
  return node_->maximum;
}
std::size_t ValueFormat::min_size() const noexcept { return node_->min_size; }
std::size_t ValueFormat::max_size() const noexcept { return node_->max_size; }

namespace {
[[nodiscard]] auto make(ValueFormat::Kind kind) -> std::shared_ptr<ValueFormat::Node> {
  auto node = std::make_shared<ValueFormat::Node>();
  node->kind = kind;
  return node;
}
} // namespace

ValueFormat ValueFormat::Minimum(const double value, const bool inclusive) const {
  auto node = std::make_shared<Node>(*node_);
  if (node->kind != Kind::Integer && node->kind != Kind::Real &&
      node->kind != Kind::Quantity && node->kind != Kind::QuantityList)
    node->errors.push_back("a minimum cannot be applied to a " +
                           kind_name(node->kind));
  node->minimum = Bound{value, inclusive};
  return ValueFormat(std::move(node));
}

ValueFormat ValueFormat::Maximum(const double value, const bool inclusive) const {
  auto node = std::make_shared<Node>(*node_);
  if (node->kind != Kind::Integer && node->kind != Kind::Real &&
      node->kind != Kind::Quantity && node->kind != Kind::QuantityList)
    node->errors.push_back("a maximum cannot be applied to a " +
                           kind_name(node->kind));
  node->maximum = Bound{value, inclusive};
  return ValueFormat(std::move(node));
}

ValueFormat ValueFormat::Size(const std::size_t minimum,
                              const std::size_t maximum) const {
  auto node = std::make_shared<Node>(*node_);
  if (node->kind != Kind::List && node->kind != Kind::Table &&
      node->kind != Kind::QuantityList)
    node->errors.push_back("a size cannot be applied to a " +
                           kind_name(node->kind));
  node->min_size = minimum;
  node->max_size = maximum;
  return ValueFormat(std::move(node));
}

ValueFormat ValueFormat::Alias(std::string alias, std::string value) const {
  auto node = std::make_shared<Node>(*node_);
  if (node->kind != Kind::Enumeration) {
    node->errors.push_back("an alias cannot be applied to a " +
                           kind_name(node->kind));
    return ValueFormat(std::move(node));
  }
  const auto it = std::find_if(node->enum_values.begin(), node->enum_values.end(),
                               [&](const EnumValue &v) {
                                 return v.name == value && v.name == v.canonical;
                               });
  if (it == node->enum_values.end()) {
    node->errors.push_back("the alias '" + alias + "' refers to the unknown value '" +
                           value + "'");
    return ValueFormat(std::move(node));
  }
  EnumValue entry = *it;
  entry.name = std::move(alias);
  node->enum_values.push_back(std::move(entry));
  return ValueFormat(std::move(node));
}

ValueFormat ValueFormat::Disable(std::string value, std::string reason) const {
  auto node = std::make_shared<Node>(*node_);
  if (node->kind != Kind::Enumeration) {
    node->errors.push_back("Disable cannot be applied to a " +
                           kind_name(node->kind));
    return ValueFormat(std::move(node));
  }
  bool found = false;
  for (auto &entry : node->enum_values)
    if (entry.canonical == value) {
      entry.unavailable = reason.empty() ? "not available" : reason;
      found = true;
    }
  if (!found)
    node->errors.push_back("cannot disable the unknown value '" + value + "'");
  return ValueFormat(std::move(node));
}

auto ValueFormat::width() const -> std::optional<std::size_t> {
  switch (node_->kind) {
  case Kind::Integer:
  case Kind::Real:
  case Kind::Boolean:
  case Kind::Word:
  case Kind::Enumeration:
    return 1;
  case Kind::Quantity:
    return 2;
  case Kind::Record: {
    std::size_t total = 0;
    for (const auto &field : node_->fields) {
      const auto w = field.format.width();
      if (!w)
        return std::nullopt;
      total += *w;
    }
    return total;
  }
  case Kind::Choice: {
    std::optional<std::size_t> common;
    for (const auto &alternative : node_->alternatives) {
      const auto w = alternative.width();
      if (!w || (common && *common != *w))
        return std::nullopt;
      common = w;
    }
    return common;
  }
  case Kind::Text:
  case Kind::List:
  case Kind::QuantityList:
  case Kind::Table:
    return std::nullopt;
  }
  return std::nullopt;
}

auto ValueFormat::describe(const units::UnitSystem &system) const -> std::string {
  const auto bounds = [this]() {
    std::string text;
    if (node_->minimum)
      text += std::string(node_->minimum->inclusive ? " >= " : " > ") +
              format_number(node_->minimum->value);
    if (node_->maximum)
      text += std::string(node_->maximum->inclusive ? " <= " : " < ") +
              format_number(node_->maximum->value);
    return text;
  };
  const auto size = [this]() -> std::string {
    if (node_->min_size <= 1 && node_->max_size == unlimited)
      return "";
    if (node_->min_size == node_->max_size)
      return " (" + std::to_string(node_->min_size) + ")";
    if (node_->max_size == unlimited)
      return " (at least " + std::to_string(node_->min_size) + ")";
    return " (" + std::to_string(node_->min_size) + " to " +
           std::to_string(node_->max_size) + ")";
  };
  switch (node_->kind) {
  case Kind::Integer: return "integer" + bounds();
  case Kind::Real: return "number" + bounds();
  case Kind::Boolean: return "boolean";
  case Kind::Word: return "word";
  case Kind::Text: return "text";
  case Kind::Enumeration: {
    std::vector<std::string> names;
    for (const auto &v : node_->enum_values)
      if (v.name == v.canonical)
        names.push_back(v.name);
    return "one of " + join(names, " | ");
  }
  case Kind::Quantity:
    return node_->dimension ? units::describe(*node_->dimension, system) + bounds()
                            : "measure";
  case Kind::Record: {
    std::vector<std::string> parts;
    for (const auto &field : node_->fields)
      parts.push_back(field.name + ": " + field.format.describe(system));
    return "(" + join(parts, ", ") + ")";
  }
  case Kind::List:
    return "list" + size() + " of " + node_->element->describe(system);
  case Kind::QuantityList:
    return "list" + size() + " of " +
           (node_->dimension ? units::describe(*node_->dimension, system)
                             : std::string("measures")) +
           bounds();
  case Kind::Table: {
    std::vector<std::string> parts;
    for (const auto &column : node_->columns)
      parts.push_back(column.dimension
                          ? column.name + " " +
                                units::describe(*column.dimension, system)
                                    .substr(units::describe(*column.dimension,
                                                            system)
                                                .find('['))
                          : column.name);
    return "table" + size() + " of (" + join(parts, ", ") + ")";
  }
  case Kind::Choice: {
    std::vector<std::string> parts;
    for (const auto &alternative : node_->alternatives)
      parts.push_back(alternative.describe(system));
    return join(parts, ", or ");
  }
  }
  return "value";
}

auto ValueFormat::problems() const -> std::vector<std::string> {
  std::vector<std::string> found = node_->errors;
  const auto nested = [&found](const ValueFormat &format,
                               const std::string &where) {
    for (auto &problem : format.problems())
      found.push_back(where + ": " + problem);
  };
  switch (node_->kind) {
  case Kind::Enumeration: {
    if (node_->enum_values.empty())
      found.push_back("an enumeration needs at least one value");
    std::set<std::string> names;
    for (const auto &v : node_->enum_values)
      if (v.name.empty() || !names.insert(v.name).second)
        found.push_back("the enumeration has an empty or repeated value '" +
                        v.name + "'");
    break;
  }
  case Kind::Quantity:
  case Kind::QuantityList:
    break;
  case Kind::Record: {
    if (node_->fields.empty())
      found.push_back("a record needs at least one field");
    std::set<std::string> names;
    for (std::size_t i = 0; i < node_->fields.size(); ++i) {
      const auto &field = node_->fields[i];
      if (field.name.empty() || !names.insert(field.name).second)
        found.push_back("the record has an empty or repeated field name '" +
                        field.name + "'");
      if (i + 1 < node_->fields.size() && !field.format.width())
        found.push_back("the field '" + field.name +
                        "' has a variable number of values, so it must be "
                        "the last field of the record");
      nested(field.format, "field '" + field.name + "'");
    }
    break;
  }
  case Kind::List:
    if (!node_->element->width())
      found.push_back("the elements of a list must have a fixed number of "
                      "values, but " +
                      node_->element->describe() + " does not");
    else if (*node_->element->width() == 0)
      found.push_back("the elements of a list cannot be empty");
    nested(*node_->element, "list element");
    break;
  case Kind::Table:
    if (node_->columns.empty())
      found.push_back("a table needs at least one column");
    break;
  case Kind::Choice:
    if (node_->alternatives.size() < 2)
      found.push_back("a choice needs at least two alternatives");
    for (std::size_t i = 0; i < node_->alternatives.size(); ++i)
      nested(node_->alternatives[i], "alternative " + std::to_string(i + 1));
    break;
  default:
    break;
  }
  if (node_->min_size > node_->max_size)
    found.push_back("the minimum size is larger than the maximum size");
  if (node_->minimum && node_->maximum &&
      (node_->minimum->value > node_->maximum->value ||
       (node_->minimum->value == node_->maximum->value &&
        !(node_->minimum->inclusive && node_->maximum->inclusive))))
    found.push_back("the range is empty");
  return found;
}

namespace formats {
auto Integer() -> ValueFormat { return ValueFormat(make(ValueFormat::Kind::Integer)); }
auto Real() -> ValueFormat { return ValueFormat(make(ValueFormat::Kind::Real)); }
auto Boolean() -> ValueFormat { return ValueFormat(make(ValueFormat::Kind::Boolean)); }
auto Word() -> ValueFormat { return ValueFormat(make(ValueFormat::Kind::Word)); }
auto Text() -> ValueFormat { return ValueFormat(make(ValueFormat::Kind::Text)); }

auto Enumeration(std::vector<std::string> values) -> ValueFormat {
  auto node = make(ValueFormat::Kind::Enumeration);
  for (std::size_t i = 0; i < values.size(); ++i)
    node->enum_values.push_back(ValueFormat::EnumValue{
        values[i], static_cast<long long>(i), values[i], {}});
  return ValueFormat(std::move(node));
}

auto Quantity(const units::Dimension &dimension) -> ValueFormat {
  auto node = make(ValueFormat::Kind::Quantity);
  node->dimension = dimension;
  return ValueFormat(std::move(node));
}

auto Record(std::vector<ValueFormat::Field> fields) -> ValueFormat {
  auto node = make(ValueFormat::Kind::Record);
  node->fields = std::move(fields);
  return ValueFormat(std::move(node));
}

auto List(ValueFormat element, const std::size_t minimum,
          const std::size_t maximum) -> ValueFormat {
  auto node = make(ValueFormat::Kind::List);
  node->element = std::move(element);
  node->min_size = minimum;
  node->max_size = maximum;
  return ValueFormat(std::move(node));
}

auto QuantityList(const units::Dimension &dimension) -> ValueFormat {
  auto node = make(ValueFormat::Kind::QuantityList);
  node->dimension = dimension;
  node->min_size = 1;
  return ValueFormat(std::move(node));
}

auto Table(std::vector<ValueFormat::Column> columns) -> ValueFormat {
  auto node = make(ValueFormat::Kind::Table);
  node->columns = std::move(columns);
  node->min_size = 1;
  return ValueFormat(std::move(node));
}

auto Choice(std::vector<ValueFormat> alternatives) -> ValueFormat {
  auto node = make(ValueFormat::Kind::Choice);
  node->alternatives = std::move(alternatives);
  return ValueFormat(std::move(node));
}

auto Composition() -> ValueFormat {
  return List(Record({{"name", Word()}, {"value", Real()}}));
}
} // namespace formats

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------
namespace detail {
namespace {
using Kind = ValueFormat::Kind;

class FormatParser {
public:
  FormatParser(const std::vector<std::string> &tokens,
               const units::UnitSystem &system)
      : tokens_(tokens), system_(system) {}

  std::vector<ValueProblem> problems;

  auto whole(const ValueFormat &format, std::size_t begin, std::size_t end,
             const std::string &context) -> Value {
    const auto width = format.width();
    if (width && format.kind() != Kind::Choice) {
      if (end - begin != *width) {
        add(context,
            "expected " + format.describe(system_) + " (" +
                count(*width, "value", "values") + "), but " +
                count(end - begin, "value was", "values were") + " given",
            keyword_token);
        return {};
      }
      return fixed(format, begin, context);
    }
    switch (format.kind()) {
    case Kind::Text: return text(begin, end, context);
    case Kind::Record: return record(format, begin, end, context);
    case Kind::List: return list(format, begin, end, context);
    case Kind::QuantityList: return quantity_list(format, begin, end, context);
    case Kind::Table: return table(format, begin, end, context);
    case Kind::Choice: return choice(format, begin, end, context);
    default: return fixed(format, begin, context);
    }
  }

private:
  const std::vector<std::string> &tokens_;
  const units::UnitSystem &system_;

  static auto count(std::size_t n, const char *one, const char *many)
      -> std::string {
    return std::to_string(n) + " " + (n == 1 ? one : many);
  }

  void add(const std::string &context, std::string message, std::size_t token,
           DiagnosticCode code = DiagnosticCode::InvalidValue,
           std::string hint = {}) {
    problems.push_back(ValueProblem{
        code, context.empty() ? std::move(message) : context + ": " + message,
        token, std::move(hint)});
  }

  void check_bounds(const ValueFormat &format, double value, std::size_t token,
                    const std::string &shown, const std::string &unit,
                    const std::string &context) {
    if (const auto &lo = format.minimum();
        lo && (value < lo->value || (!lo->inclusive && value == lo->value)))
      add(context,
          "the value " + shown + " must be " + (lo->inclusive ? ">= " : "> ") +
              format_number(lo->value) + unit,
          token, DiagnosticCode::OutOfRange);
    else if (const auto &hi = format.maximum();
             hi && (value > hi->value || (!hi->inclusive && value == hi->value)))
      add(context,
          "the value " + shown + " must be " + (hi->inclusive ? "<= " : "< ") +
              format_number(hi->value) + unit,
          token, DiagnosticCode::OutOfRange);
  }

  void check_size(const ValueFormat &format, std::size_t n, const char *what,
                  const std::string &context) {
    if (n >= format.min_size() && n <= format.max_size())
      return;
    const std::size_t lo = format.min_size();
    const std::size_t hi = format.max_size();
    const std::string expected =
        lo == hi ? "exactly " + std::to_string(lo)
        : hi == unlimited ? "at least " + std::to_string(lo)
                          : "between " + std::to_string(lo) + " and " +
                                std::to_string(hi);
    add(context,
        std::to_string(n) + " " + what + " given, but " + expected +
            " are required",
        keyword_token, DiagnosticCode::OutOfRange);
  }

  auto quantity_value(const units::Dimension &dimension,
                      const ValueFormat &format, std::size_t number_token,
                      std::size_t unit_token, const std::string &context,
                      bool &ok) -> double {
    ok = false;
    double magnitude = 0.0;
    if (!parse_number(tokens_[number_token], magnitude)) {
      add(context,
          "the magnitude of a measure must be a finite number, but " +
              quote(tokens_[number_token]) + " was given",
          number_token);
      return 0.0;
    }
    try {
      const double si =
          units::to_si(magnitude, tokens_[unit_token], dimension, system_);
      check_bounds(format, si, number_token,
                   tokens_[number_token] + " " + tokens_[unit_token] + " (= " +
                       format_number(si) + " " +
                       units::si_unit(dimension, system_) + ")",
                   " " + units::si_unit(dimension, system_), context);
      ok = true;
      return si;
    } catch (const units::UnitError &error) {
      add(context, error.what(), unit_token, DiagnosticCode::InvalidUnit);
      return 0.0;
    }
  }

  auto fixed(const ValueFormat &format, std::size_t at,
             const std::string &context) -> Value {
    Value value;
    const std::string &token = tokens_[at];
    switch (format.kind()) {
    case Kind::Integer: {
      long long number = 0;
      if (!parse_number(token, number)) {
        add(context, "an integer is required, but " + quote(token) + " was given",
            at);
        return value;
      }
      check_bounds(format, static_cast<double>(number), at, token, "", context);
      value.kind = Value::Kind::Integer;
      value.integer = number;
      return value;
    }
    case Kind::Real: {
      double number = 0.0;
      if (!parse_number(token, number)) {
        add(context, "a finite number is required, but " + quote(token) +
                         " was given",
            at);
        return value;
      }
      check_bounds(format, number, at, token, "", context);
      value.kind = Value::Kind::Real;
      value.real = number;
      return value;
    }
    case Kind::Boolean: {
      bool flag = false;
      if (!parse_bool_token(token, flag)) {
        add(context, "a boolean (true/false/on/off) is required, but " +
                         quote(token) + " was given",
            at);
        return value;
      }
      value.kind = Value::Kind::Boolean;
      value.boolean = flag;
      return value;
    }
    case Kind::Word:
      value.kind = Value::Kind::Text;
      value.text = token;
      return value;
    case Kind::Enumeration: {
      for (const auto &entry : format.enum_values())
        if (entry.name == token) {
          if (!entry.unavailable.empty()) {
            add(context, quote(token) + " is not available: " + entry.unavailable,
                at, DiagnosticCode::NotAllowed);
            return value;
          }
          value.kind = Value::Kind::Text;
          value.text = entry.canonical;
          value.integer = entry.code;
          value.enumeration = true;
          return value;
        }
      std::vector<std::string> names;
      std::vector<std::string> all;
      for (const auto &entry : format.enum_values()) {
        all.push_back(entry.name);
        if (entry.name == entry.canonical)
          names.push_back(entry.name);
      }
      const std::string suggestion = closest_match(token, all);
      add(context,
          quote(token) + " is not an allowed value (allowed: " +
              join(names, ", ") + ")",
          at, DiagnosticCode::NotAllowed,
          suggestion.empty() ? std::string()
                             : "did you mean " + quote(suggestion) + "?");
      return value;
    }
    case Kind::Quantity: {
      bool ok = false;
      const double si =
          quantity_value(*format.dimension(), format, at, at + 1, context, ok);
      if (ok) {
        value.kind = Value::Kind::Real;
        value.real = si;
      }
      return value;
    }
    case Kind::Record: {
      value.kind = Value::Kind::Sequence;
      std::size_t position = at;
      for (const auto &field : format.fields()) {
        const std::size_t w = *field.format.width();
        value.items.push_back(
            whole(field.format, position, position + w,
                  join_context(context, "field '" + field.name + "'")));
        position += w;
      }
      return value;
    }
    case Kind::Choice:
      return choice(format, at, at + *format.width(), context);
    default:
      return whole(format, at, tokens_.size(), context);
    }
  }

  static auto join_context(const std::string &outer, const std::string &inner)
      -> std::string {
    return outer.empty() ? inner : outer + ", " + inner;
  }

  auto text(std::size_t begin, std::size_t end, const std::string &context)
      -> Value {
    Value value;
    if (begin == end) {
      add(context, "a text is required, but no value was given", keyword_token);
      return value;
    }
    value.kind = Value::Kind::Text;
    for (std::size_t i = begin; i < end; ++i)
      value.text += (i == begin ? "" : " ") + tokens_[i];
    return value;
  }

  auto record(const ValueFormat &format, std::size_t begin, std::size_t end,
              const std::string &context) -> Value {
    Value value;
    value.kind = Value::Kind::Sequence;
    std::size_t fixed_width = 0;
    for (std::size_t i = 0; i + 1 < format.fields().size(); ++i)
      fixed_width += *format.fields()[i].format.width();
    if (end - begin < fixed_width) {
      add(context,
          "expected " + format.describe(system_) + ", but only " +
              count(end - begin, "value was", "values were") + " given",
          keyword_token);
      return {};
    }
    std::size_t position = begin;
    for (std::size_t i = 0; i < format.fields().size(); ++i) {
      const auto &field = format.fields()[i];
      const bool last = i + 1 == format.fields().size();
      const std::size_t stop = last ? end : position + *field.format.width();
      value.items.push_back(whole(field.format, position, stop,
                                  join_context(context, "field '" + field.name + "'")));
      position = stop;
    }
    return value;
  }

  auto list(const ValueFormat &format, std::size_t begin, std::size_t end,
            const std::string &context) -> Value {
    Value value;
    value.kind = Value::Kind::Sequence;
    const ValueFormat &element = format.element();
    const std::size_t w = *element.width();
    const std::size_t n = end - begin;
    if (n % w != 0) {
      add(context,
          "expected groups of " + std::to_string(w) + " values (" +
              element.describe(system_) + "), but " +
              count(n, "value was", "values were") + " given",
          keyword_token);
      return {};
    }
    check_size(format, n / w, n / w == 1 ? "element was" : "elements were",
               context);
    for (std::size_t k = 0; k < n / w; ++k)
      value.items.push_back(
          whole(element, begin + k * w, begin + (k + 1) * w,
                n / w == 1 && w == 1
                    ? context
                    : join_context(context, "element " + std::to_string(k + 1))));
    return value;
  }

  auto quantity_list(const ValueFormat &format, std::size_t begin,
                     std::size_t end, const std::string &context) -> Value {
    Value value;
    value.kind = Value::Kind::Sequence;
    const std::vector<std::string> slice(
        tokens_.begin() + static_cast<std::ptrdiff_t>(begin),
        tokens_.begin() + static_cast<std::ptrdiff_t>(end));
    std::vector<double> magnitudes;
    std::vector<std::string> unit_names;
    if (const auto error = split_measures(slice, magnitudes, unit_names)) {
      add(context, *error, keyword_token);
      return {};
    }
    check_size(format, magnitudes.size(),
               magnitudes.size() == 1 ? "measure was" : "measures were", context);
    const bool shared = magnitudes.size() + 1 == slice.size();
    for (std::size_t k = 0; k < magnitudes.size(); ++k) {
      const std::size_t number_token = begin + (shared ? k : 2 * k);
      const std::size_t unit_token = shared ? end - 1 : begin + 2 * k + 1;
      Value item;
      if (format.dimension()) {
        bool ok = false;
        const double si = quantity_value(*format.dimension(), format,
                                         number_token, unit_token, context, ok);
        if (!ok && shared)
          break; // one message for a shared unit
        item.kind = Value::Kind::Real;
        item.real = si;
      } else {
        Value magnitude;
        magnitude.kind = Value::Kind::Real;
        magnitude.real = magnitudes[k];
        Value unit;
        unit.kind = Value::Kind::Text;
        unit.text = unit_names[k];
        item.kind = Value::Kind::Sequence;
        item.items = {magnitude, unit};
      }
      value.items.push_back(std::move(item));
    }
    return value;
  }

  auto table(const ValueFormat &format, std::size_t begin, std::size_t end,
             const std::string &context) -> Value {
    Value value;
    value.kind = Value::Kind::Sequence;
    const auto &columns = format.columns();
    const std::size_t c = columns.size();
    const bool header = std::any_of(columns.begin(), columns.end(),
                                    [](const ValueFormat::Column &column) {
                                      return column.dimension.has_value();
                                    });
    const std::size_t h = header ? c : 0;
    const std::size_t n = end - begin;
    std::vector<std::string> names;
    for (const auto &column : columns)
      names.push_back(column.name);
    if (n < h || (n - h) % c != 0) {
      add(context,
          std::string(header ? "expected one unit per column (" : "expected rows of ") +
              join(names, ", ") + (header ? ") followed by rows of " : " (") +
              std::to_string(c) + " numbers" + (header ? "" : ")") + ", but " +
              count(n, "value was", "values were") + " given",
          keyword_token);
      return {};
    }
    const std::size_t rows = (n - h) / c;
    check_size(format, rows, rows == 1 ? "row was" : "rows were", context);

    std::vector<bool> column_ok(c, true);
    if (header)
      for (std::size_t j = 0; j < c; ++j) {
        const std::string &unit = tokens_[begin + j];
        const std::string where =
            join_context(context, "units of column '" + columns[j].name + "'");
        if (!columns[j].dimension) {
          if (unit != "-") {
            add(where, "the column is dimensionless, so its unit must be '-'",
                begin + j, DiagnosticCode::InvalidUnit);
            column_ok[j] = false;
          }
        } else if (const auto error =
                       units::check_unit(unit, *columns[j].dimension)) {
          add(where, *error, begin + j, DiagnosticCode::InvalidUnit);
          column_ok[j] = false;
        }
      }

    for (std::size_t r = 0; r < rows; ++r) {
      Value row;
      row.kind = Value::Kind::Sequence;
      for (std::size_t j = 0; j < c; ++j) {
        const std::size_t token = begin + h + r * c + j;
        Value cell;
        double number = 0.0;
        if (!parse_number(tokens_[token], number)) {
          add(join_context(context, "row " + std::to_string(r + 1) +
                                        ", column '" + columns[j].name + "'"),
              "a finite number is required, but " + quote(tokens_[token]) +
                  " was given",
              token);
        } else if (columns[j].dimension && column_ok[j]) {
          number = units::to_si(number, tokens_[begin + j],
                                *columns[j].dimension, system_);
        }
        cell.kind = Value::Kind::Real;
        cell.real = number;
        row.items.push_back(std::move(cell));
      }
      value.items.push_back(std::move(row));
    }
    return value;
  }

  auto choice(const ValueFormat &format, std::size_t begin, std::size_t end,
              const std::string &context) -> Value {
    std::vector<std::string> forms;
    for (std::size_t i = 0; i < format.alternatives().size(); ++i) {
      FormatParser attempt(tokens_, system_);
      Value value = attempt.whole(format.alternatives()[i], begin, end, {});
      if (attempt.problems.empty()) {
        value.alternative = i;
        return value;
      }
      forms.push_back(format.alternatives()[i].describe(system_));
    }
    add(context,
        "the value matches none of the accepted forms: " + join(forms, "; or "),
        keyword_token);
    return {};
  }
};
} // namespace

auto parse_value(const ValueFormat &format,
                 const std::vector<std::string> &values,
                 const units::UnitSystem &system) -> ParseOutcome {
  FormatParser parser(values, system);
  ParseOutcome outcome;
  outcome.value = parser.whole(format, 0, values.size(), {});
  outcome.problems = std::move(parser.problems);
  return outcome;
}

auto format_of(const DictionaryKeyWord &keyword) -> ValueFormat {
  using namespace formats;
  switch (keyword.type()) {
  case NONE: return Text();
  case SINGLE_INT: return Integer();
  case SINGLE_DOUBLE: return Real();
  case SINGLE_BOOL: return Boolean();
  case SINGLE_STRING:
  case SINGLE_DICTIONARY:
  case SINGLE_CHAR:
    return Word();
  case SINGLE_PATH:
  case SEQUENCE_STRING:
    return Text();
  case SINGLE_MEASURE: {
    if (keyword.dimension())
      return Quantity(*keyword.dimension());
    std::vector<ValueFormat::Field> fields{{"value", Real()}, {"units", Word()}};
    return Record(std::move(fields));
  }
  case VECTOR_INT: return List(Integer());
  case VECTOR_DOUBLE: return List(Real());
  case VECTOR_STRING:
  case VECTOR_CHAR:
    return List(Word());
  case VECTOR_BOOL: return List(Boolean());
  case VECTOR_MEASURE: {
    if (keyword.dimension())
      return QuantityList(*keyword.dimension());
    auto node = std::make_shared<ValueFormat::Node>();
    node->kind = ValueFormat::Kind::QuantityList;
    node->min_size = 1;
    return ValueFormat(std::move(node));
  }
  case VECTOR_STRING_DOUBLE: return Composition();
  case VECTOR_INT_STRING:
    return List(Record({{"number", Integer()}, {"name", Word()}}));
  case STRUCTURED:
    return keyword.format() ? *keyword.format() : Text();
  }
  return Text();
}
} // namespace detail
} // namespace OpenSMOKEpp
