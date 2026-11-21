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

#include "DictionaryValues.h"
#include "DictionaryUtilities.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace OpenSMOKEpp::detail {
namespace {
[[nodiscard]] auto equals_ignore_case(const std::string_view lhs,
                                      const std::string_view rhs) noexcept
    -> bool {
  if (lhs.size() != rhs.size())
    return false;
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    const auto lower = [](const char c) {
      return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    };
    if (lower(lhs[i]) != lower(rhs[i]))
      return false;
  }
  return true;
}

[[nodiscard]] auto format_number(const double value) -> std::string {
  std::ostringstream out;
  out << value;
  return out.str();
}

[[nodiscard]] auto count_text(const std::size_t n, const char *singular,
                              const char *plural) -> std::string {
  return std::to_string(n) + " " + (n == 1 ? singular : plural);
}

// A number found in the value, with the token it comes from.
struct Number {
  double value;
  std::size_t token;
  std::string original; // as written, e.g. "5 C"; empty if same as value
};
} // namespace

auto is_number(const std::string_view token) noexcept -> bool {
  double value = 0.0;
  return parse_number(token, value);
}

auto parse_bool_token(const std::string_view token, bool &value) noexcept
    -> bool {
  if (equals_ignore_case(token, "true") || equals_ignore_case(token, "on")) {
    value = true;
    return true;
  }
  if (equals_ignore_case(token, "false") || equals_ignore_case(token, "off")) {
    value = false;
    return true;
  }
  return false;
}

auto quote(const std::string_view token) -> std::string {
  return "'" + std::string(token) + "'";
}

auto split_measures(const std::vector<std::string> &tokens,
                    std::vector<double> &values,
                    std::vector<std::string> &units)
    -> std::optional<std::string> {
  const std::size_t n = tokens.size();
  values.clear();
  units.clear();
  const char *layout = "pairs of a number and units (e.g. '1 m 20 cm') or "
                       "numbers followed by one unit (e.g. '1000 1100 K') are "
                       "required";
  if (n < 2)
    return std::string(layout) + ", but " + std::to_string(n) +
           " value(s) were given";

  bool shared = !is_number(tokens.back());
  for (std::size_t i = 0; i + 1 < n && shared; ++i)
    shared = is_number(tokens[i]);
  if (shared) {
    values.resize(n - 1);
    for (std::size_t i = 0; i + 1 < n; ++i)
      (void)parse_number(tokens[i], values[i]);
    units.assign(n - 1, tokens.back());
    return std::nullopt;
  }

  if (n % 2 != 0)
    return std::string(layout) + ", but " + std::to_string(n) +
           " values were given";
  values.resize(n / 2);
  units.resize(n / 2);
  for (std::size_t i = 0; i < n; i += 2) {
    if (!parse_number(tokens[i], values[i / 2]))
      return "measure " + std::to_string(i / 2 + 1) +
             " must start with a finite number, but " + quote(tokens[i]) +
             " was given";
    if (is_number(tokens[i + 1]))
      return "measure " + std::to_string(i / 2 + 1) +
             " has no units: " + std::string(layout);
    units[i / 2] = tokens[i + 1];
  }
  return std::nullopt;
}

auto validate_value(const DictionaryKeyWord &keyword,
                    const std::vector<std::string> &values,
                    const units::UnitSystem &system)
    -> std::vector<ValueProblem> {
  std::vector<ValueProblem> problems;
  std::vector<Number> numbers;       // numeric values (SI for measures)
  std::vector<std::size_t> strings;  // tokens subject to allowed values
  std::size_t elements = values.size();
  const std::size_t n = values.size();

  const auto problem = [&problems](std::string message,
                                   std::size_t token = keyword_token,
                                   DiagnosticCode code =
                                       DiagnosticCode::InvalidValue) {
    problems.push_back(ValueProblem{code, std::move(message), token, {}});
  };
  const auto exactly_one = [&](const char *what) {
    if (n == 1)
      return true;
    problem(std::string("exactly one ") + what + " is required, but " +
            std::to_string(n) + " values were given");
    return false;
  };
  const auto at_least_one = [&](const char *what) {
    if (n >= 1)
      return true;
    problem(std::string("a list of ") + what +
            " is required, but no value was given");
    return false;
  };
  const auto check_double = [&](std::size_t i, const char *what) {
    double value = 0.0;
    if (parse_number(values[i], value)) {
      numbers.push_back({value, i, {}});
      return true;
    }
    problem(std::string(what) + " must be a finite number, but " +
                quote(values[i]) + " was given",
            i);
    return false;
  };
  const auto check_int = [&](std::size_t i, const char *what) {
    int value = 0;
    if (parse_number(values[i], value)) {
      numbers.push_back({static_cast<double>(value), i, {}});
      return true;
    }
    problem(std::string(what) + " must be an integer, but " +
                quote(values[i]) + " was given",
            i);
    return false;
  };
  const auto check_bool = [&](std::size_t i) {
    bool flag = false;
    if (!parse_bool_token(values[i], flag))
      problem("a boolean (true/false/on/off) is required, but " +
                  quote(values[i]) + " was given",
              i);
  };
  const auto check_char = [&](std::size_t i) {
    if (values[i].size() != 1)
      problem("a single character is required, but " + quote(values[i]) +
                  " was given",
              i);
  };
  const auto convert = [&](double magnitude, const std::string &unit,
                           std::size_t value_token, std::size_t unit_token) {
    if (!keyword.dimension())
      return;
    try {
      numbers.push_back(
          {units::to_si(magnitude, unit, *keyword.dimension(), system),
           value_token, values[value_token] + " " + unit});
    } catch (const units::UnitError &error) {
      problem(error.what(), unit_token, DiagnosticCode::InvalidUnit);
    }
  };

  switch (keyword.type()) {
  case STRUCTURED:
    if (keyword.format())
      return parse_value(*keyword.format(), values, system).problems;
    break;
  case NONE:
    if (n != 0)
      problem("no value is expected, but " + std::to_string(n) +
              " values were given");
    break;
  case SINGLE_INT:
    if (exactly_one("integer"))
      check_int(0, "the value");
    break;
  case SINGLE_DOUBLE:
    if (exactly_one("number (double)"))
      check_double(0, "the value");
    break;
  case SINGLE_STRING:
  case SINGLE_DICTIONARY:
    if (exactly_one(keyword.type() == SINGLE_STRING
                        ? "string (use quotes for text containing blanks)"
                        : "dictionary name"))
      strings.push_back(0);
    break;
  case SINGLE_CHAR:
    if (exactly_one("character"))
      check_char(0);
    break;
  case SINGLE_BOOL:
    if (exactly_one("boolean (true/false/on/off)"))
      check_bool(0);
    break;
  case SINGLE_PATH:
    if (n == 0)
      problem("a path is required, but no value was given");
    break;
  case SEQUENCE_STRING:
    break;
  case SINGLE_MEASURE: {
    if (n != 2) {
      problem("a measure requires exactly two values (number and units, e.g. "
              "'300 K'), but " +
              std::to_string(n) + " were given");
      break;
    }
    double magnitude = 0.0;
    if (!parse_number(values[0], magnitude)) {
      problem("the magnitude of a measure must be a finite number, but " +
                  quote(values[0]) + " was given",
              0);
      break;
    }
    if (keyword.dimension())
      convert(magnitude, values[1], 0, 1);
    else
      numbers.push_back({magnitude, 0, {}});
    elements = 1;
    break;
  }
  case VECTOR_INT:
    if (at_least_one("integers"))
      for (std::size_t i = 0; i < n; ++i)
        check_int(i, ("value " + std::to_string(i + 1)).c_str());
    break;
  case VECTOR_DOUBLE:
    if (at_least_one("numbers (double)"))
      for (std::size_t i = 0; i < n; ++i)
        check_double(i, ("value " + std::to_string(i + 1)).c_str());
    break;
  case VECTOR_STRING:
    if (at_least_one("strings"))
      for (std::size_t i = 0; i < n; ++i)
        strings.push_back(i);
    break;
  case VECTOR_CHAR:
    if (at_least_one("characters"))
      for (std::size_t i = 0; i < n; ++i)
        check_char(i);
    break;
  case VECTOR_BOOL:
    if (at_least_one("booleans"))
      for (std::size_t i = 0; i < n; ++i)
        check_bool(i);
    break;
  case VECTOR_MEASURE: {
    std::vector<double> magnitudes;
    std::vector<std::string> unit_names;
    if (const auto error = split_measures(values, magnitudes, unit_names)) {
      problem(*error);
      break;
    }
    const bool shared = magnitudes.size() + 1 == n;
    elements = magnitudes.size();
    for (std::size_t k = 0; k < magnitudes.size(); ++k) {
      const std::size_t value_token = shared ? k : 2 * k;
      const std::size_t unit_token = shared ? n - 1 : 2 * k + 1;
      if (!keyword.dimension()) {
        numbers.push_back({magnitudes[k], value_token, {}});
        continue;
      }
      const std::size_t before = problems.size();
      convert(magnitudes[k], unit_names[k], value_token, unit_token);
      if (problems.size() != before && shared)
        break; // one message for a shared unit
    }
    break;
  }
  case VECTOR_STRING_DOUBLE:
    if (n == 0 || n % 2 != 0) {
      problem("pairs of a name and a number (double) are required, e.g. "
              "'CH4 0.5 O2 0.5', but " +
              std::to_string(n) + " values were given");
      break;
    }
    elements = n / 2;
    for (std::size_t i = 0; i < n; i += 2) {
      double value = 0.0;
      if (parse_number(values[i + 1], value))
        numbers.push_back({value, i + 1, {}});
      else
        problem("the value paired with " + quote(values[i]) +
                    " must be a finite number (double), but " +
                    quote(values[i + 1]) + " was given",
                i + 1);
    }
    break;
  case VECTOR_INT_STRING:
    if (n == 0 || n % 2 != 0) {
      problem("pairs of an integer and a name are required, but " +
              std::to_string(n) + " values were given");
      break;
    }
    elements = n / 2;
    for (std::size_t i = 0; i < n; i += 2)
      check_int(i, ("pair " + std::to_string(i / 2 + 1) + " number").c_str());
    break;
  }

  if (!problems.empty())
    return problems;

  // Allowed values.
  const auto &allowed = keyword.allowed_values();
  if (!allowed.empty())
    for (const std::size_t i : strings)
      if (std::find(allowed.begin(), allowed.end(), values[i]) ==
          allowed.end()) {
        std::string list;
        for (std::size_t k = 0; k < allowed.size(); ++k)
          list += (k == 0 ? "" : ", ") + allowed[k];
        ValueProblem p{DiagnosticCode::NotAllowed,
                       quote(values[i]) + " is not an allowed value (allowed: " +
                           list + ")",
                       i,
                       {}};
        const std::string suggestion = closest_match(values[i], allowed);
        if (!suggestion.empty())
          p.hint = "did you mean " + quote(suggestion) + "?";
        problems.push_back(std::move(p));
      }

  // Numeric bounds (SI units for measures).
  const bool measure =
      keyword.type() == SINGLE_MEASURE || keyword.type() == VECTOR_MEASURE;
  const std::string unit =
      measure && keyword.dimension()
          ? " " + units::si_unit(*keyword.dimension(), system)
          : std::string();
  const auto shown = [&](const Number &number) {
    if (number.original.empty())
      return format_number(number.value);
    return number.original + " (= " + format_number(number.value) + unit + ")";
  };
  for (const auto &number : numbers) {
    if (const auto &lo = keyword.minimum();
        lo && (number.value < lo->value ||
               (!lo->inclusive && number.value == lo->value)))
      problems.push_back(ValueProblem{
          DiagnosticCode::OutOfRange,
          "the value " + shown(number) + " must be " +
              (lo->inclusive ? ">= " : "> ") + format_number(lo->value) + unit,
          number.token,
          {}});
    else if (const auto &hi = keyword.maximum();
             hi && (number.value > hi->value ||
                    (!hi->inclusive && number.value == hi->value)))
      problems.push_back(ValueProblem{
          DiagnosticCode::OutOfRange,
          "the value " + shown(number) + " must be " +
              (hi->inclusive ? "<= " : "< ") + format_number(hi->value) + unit,
          number.token,
          {}});
  }

  // List size.
  if (keyword.size_min() || keyword.size_max()) {
    const std::size_t lo = keyword.size_min().value_or(0);
    const std::size_t hi =
        keyword.size_max().value_or(static_cast<std::size_t>(-1));
    if (elements < lo || elements > hi) {
      const std::string expected =
          lo == hi ? "exactly " + std::to_string(lo)
          : hi == static_cast<std::size_t>(-1)
              ? "at least " + std::to_string(lo)
              : "between " + std::to_string(lo) + " and " + std::to_string(hi);
      problems.push_back(ValueProblem{
          DiagnosticCode::OutOfRange,
          count_text(elements, "element was", "elements were") +
              " given, but " + expected + " are required",
          keyword_token,
          {}});
    }
  }

  // Sum.
  if (const auto &sum = keyword.sum()) {
    double total = 0.0;
    for (const auto &number : numbers)
      total += number.value;
    if (std::abs(total - sum->first) > sum->second) {
      std::ostringstream text;
      text.precision(12);
      text << "the values add up to " << total << ", but they must add up to "
           << sum->first << " (tolerance " << sum->second << ")";
      problems.push_back(ValueProblem{DiagnosticCode::OutOfRange, text.str(),
                                      keyword_token, {}});
    }
  }
  return problems;
}
} // namespace OpenSMOKEpp::detail
