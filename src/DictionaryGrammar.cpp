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

#include "DictionaryGrammar.h"
#include "DictionaryLexer.h"
#include "DictionaryUtilities.h"
#include "DictionaryValues.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace OpenSMOKEpp {
namespace {
constexpr std::size_t none = static_cast<std::size_t>(-1);

[[nodiscard]] auto join(const std::vector<std::string> &names)
    -> std::string {
  std::string result;
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (i != 0)
      result += ", ";
    result += names[i];
  }
  return result;
}

[[nodiscard]] auto is_numeric_type(const DictionaryKeyWordTypes type) -> bool {
  switch (type) {
  case SINGLE_INT:
  case SINGLE_DOUBLE:
  case SINGLE_MEASURE:
  case VECTOR_INT:
  case VECTOR_DOUBLE:
  case VECTOR_MEASURE:
  case VECTOR_STRING_DOUBLE:
  case VECTOR_INT_STRING:
    return true;
  default:
    return false;
  }
}

[[nodiscard]] auto is_list_type(const DictionaryKeyWordTypes type) -> bool {
  switch (type) {
  case VECTOR_INT:
  case VECTOR_DOUBLE:
  case VECTOR_STRING:
  case VECTOR_CHAR:
  case VECTOR_BOOL:
  case VECTOR_MEASURE:
  case VECTOR_STRING_DOUBLE:
  case VECTOR_INT_STRING:
    return true;
  default:
    return false;
  }
}

[[nodiscard]] auto is_single_token(const DictionaryKeyWord &keyword) -> bool {
  const auto type = keyword.type();
  if (type == STRUCTURED) {
    if (!keyword.format() || keyword.format()->width() != std::size_t{1})
      return false;
    const auto kind = keyword.format()->kind();
    return kind == ValueFormat::Kind::Integer ||
           kind == ValueFormat::Kind::Boolean ||
           kind == ValueFormat::Kind::Word ||
           kind == ValueFormat::Kind::Enumeration;
  }
  return type == SINGLE_STRING || type == SINGLE_BOOL || type == SINGLE_INT ||
         type == SINGLE_CHAR || type == SINGLE_DICTIONARY;
}

// Equality of two single-token values of a keyword (booleans by value,
// enumeration aliases by their canonical value).
[[nodiscard]] auto same_value(const DictionaryKeyWord &keyword,
                              const std::string &a, const std::string &b)
    -> bool {
  const ValueFormat format = detail::format_of(keyword);
  const auto x = detail::parse_value(format, {a}, units::si_kmol);
  const auto y = detail::parse_value(format, {b}, units::si_kmol);
  if (!x.problems.empty() || !y.problems.empty())
    return a == b;
  if (x.value.kind != y.value.kind)
    return false;
  switch (x.value.kind) {
  case Value::Kind::Integer: return x.value.integer == y.value.integer;
  case Value::Kind::Boolean: return x.value.boolean == y.value.boolean;
  case Value::Kind::Real: return x.value.real == y.value.real;
  case Value::Kind::Text: return x.value.text == y.value.text;
  default: return a == b;
  }
}

[[nodiscard]] auto format_number(const double value) -> std::string {
  std::ostringstream out;
  out << value;
  return out.str();
}
} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
auto DictionaryGrammar::Context() const -> std::string {
  std::string context = "Grammar";
  if (!name_.empty())
    context += " of dictionary " + name_;
  if (!file_name_.empty())
    context += " (file " + file_name_.filename().string() + ")";
  return context;
}

void DictionaryGrammar::ErrorMessage(std::string_view message) const {
  Diagnostic diagnostic;
  diagnostic.code = DiagnosticCode::GrammarIncoherent;
  diagnostic.message = std::string(message);
  throw DictionaryError({std::move(diagnostic)}, Context());
}

void DictionaryGrammar::DefineRules() {
  ErrorMessage("No grammar rules were defined by the user.");
}

void DictionaryGrammar::UserDefined() {
  keywords_.clear();
  list_of_keywords_names_.clear();
  user_rules_.clear();
  rules_.clear();
  number_of_keywords_ = 0;

  DefineRules();
  Finalize();
}

void DictionaryGrammar::Finalize() {
  number_of_keywords_ = static_cast<unsigned int>(keywords_.size());
  list_of_keywords_names_.resize(number_of_keywords_);
  for (unsigned int i = 0; i < number_of_keywords_; i++)
    list_of_keywords_names_[i] = keywords_[i].name();
  Check();
}

void DictionaryGrammar::AddKeyWord(const DictionaryKeyWord &keyword) {
  keywords_.push_back(keyword);
}

void DictionaryGrammar::AddRule(Rule rule) {
  user_rules_.push_back(std::move(rule));
}

auto DictionaryGrammar::ParseRuleLine(const std::string &line) const -> Rule {
  const auto words = Lexer::split_whitespace_copy(line);
  const auto fail = [&]() -> Rule {
    ErrorMessage("invalid rule line '" + Lexer::trim_copy(line) +
                 "'; expected for example 'rule: exactly 2 @T @P @Rho' or "
                 "'rule: required @V when @Type = Variable'");
  };
  if (words.size() < 3)
    return fail();

  // Split off the condition.
  std::vector<std::string> body(words.begin() + 1, words.end());
  Condition condition;
  if (const auto it = std::find(body.begin(), body.end(), "when");
      it != body.end()) {
    std::vector<std::string> c(it + 1, body.end());
    body.erase(it, body.end());
    if (c.size() == 2 && c[1] == "present")
      condition = rules::Present(c[0]);
    else if (c.size() == 2 && c[1] == "absent")
      condition = rules::Absent(c[0]);
    else if (c.size() == 3 && c[1] == "=")
      condition = rules::Equals(c[0], c[2]);
    else if (c.size() >= 3 && c[1] == "in")
      condition = rules::OneOf(c[0], std::vector<std::string>(c.begin() + 2,
                                                              c.end()));
    else
      return fail();
  }
  if (body.size() < 2)
    return fail();

  const std::string kind = body[0];
  std::vector<std::string> names(body.begin() + 1, body.end());
  const auto count = [&]() -> std::size_t {
    std::size_t n = 0;
    for (const char ch : names.front()) {
      if (ch < '0' || ch > '9')
        fail();
      n = n * 10 + static_cast<std::size_t>(ch - '0');
    }
    names.erase(names.begin());
    return n;
  };
  const auto split = [&]() {
    if (names.size() < 2)
      fail();
    std::string first = names.front();
    names.erase(names.begin());
    return first;
  };

  Rule rule;
  if (kind == "required" && names.size() == 1)
    rule = rules::Required(names[0]);
  else if (kind == "forbidden" && names.size() == 1)
    rule = rules::Forbidden(names[0]);
  else if (kind == "exactly" && names.size() >= 2) {
    const std::size_t n = count();
    rule = rules::Exactly(n, names);
  } else if (kind == "at_least" && names.size() >= 2) {
    const std::size_t n = count();
    rule = rules::AtLeast(n, names);
  } else if (kind == "at_most" && names.size() >= 2) {
    const std::size_t n = count();
    rule = rules::AtMost(n, names);
  } else if (kind == "exactly_one_of")
    rule = rules::ExactlyOneOf(names);
  else if (kind == "at_least_one_of")
    rule = rules::AtLeastOneOf(names);
  else if (kind == "at_most_one_of")
    rule = rules::AtMostOneOf(names);
  else if (kind == "requires") {
    std::string first = split();
    rule = rules::Requires(first, names);
  } else if (kind == "requires_any") {
    std::string first = split();
    rule = rules::RequiresAnyOf(first, names);
  } else if (kind == "conflicts") {
    std::string first = split();
    rule = rules::Conflicts(first, names);
  } else
    return fail();
  rule.when = condition;
  return rule;
}

void DictionaryGrammar::ReadFromFile(std::string_view file_name) {
  file_name_ = std::filesystem::path(std::string(file_name));
  keywords_.clear();
  list_of_keywords_names_.clear();
  user_rules_.clear();
  rules_.clear();
  number_of_keywords_ = 0;

  std::ifstream fInput(file_name_);
  if (!fInput.is_open())
    throw DictionaryError(
        {Diagnostic{Severity::Error, DiagnosticCode::Other,
                    SourceLocation{nullptr, file_name_.string(), 0, 0, 0},
                    "unable to open the grammar file", {}, {}}},
        Context());

  std::vector<std::string> lines;
  std::string line;
  while (std::getline(fInput, line))
    lines.push_back(line);
  if (fInput.bad())
    throw DictionaryError(
        {Diagnostic{Severity::Error, DiagnosticCode::Other,
                    SourceLocation{nullptr, file_name_.string(), 0, 0, 0},
                    "I/O error while reading the grammar file", {}, {}}},
        Context());

  std::size_t i = 0;
  while (i < lines.size()) {
    const auto tokens = Lexer::split_whitespace_view(lines[i]);
    if (tokens.empty()) {
      ++i;
      continue;
    }
    if (tokens.front() == "rule:") {
      user_rules_.push_back(ParseRuleLine(lines[i]));
      ++i;
      continue;
    }
    if (tokens.front() != "keyword:") {
      ++i; // free text, as in the original format
      continue;
    }
    if (i + 7 > lines.size())
      ErrorMessage("A keyword block starting at line " + std::to_string(i + 1) +
                   " is truncated. Each keyword definition must contain "
                   "seven lines.");
    std::vector<std::string> block(
        lines.begin() + static_cast<std::ptrdiff_t>(i),
        lines.begin() + static_cast<std::ptrdiff_t>(i + 7));
    keywords_.emplace_back(block);
    i += 7;
    // Optional property lines: dimension:, default:, allowed_values:, ...
    while (i < lines.size()) {
      const std::string trimmed = Lexer::trim_copy(lines[i]);
      if (trimmed.empty() || !keywords_.back().SetPropertyFromLine(trimmed))
        break;
      ++i;
    }
  }

  Finalize();
}

const DictionaryKeyWord *
DictionaryGrammar::FindKeyWord(std::string_view keyword_name) const noexcept {
  for (const auto &keyword : keywords_)
    if (keyword.name() == keyword_name)
      return &keyword;
  return nullptr;
}

void DictionaryGrammar::BuildRules() {
  rules_.clear();
  std::set<std::vector<std::string>> groups;
  std::set<std::pair<std::string, std::string>> conflicts;
  for (const auto &keyword : keywords_) {
    if (keyword.is_compulsory() && keyword.compulsory_alternatives().empty())
      rules_.push_back(rules::Required(keyword.name()));
    if (keyword.is_compulsory() && !keyword.compulsory_alternatives().empty()) {
      std::vector<std::string> group = keyword.compulsory_alternatives();
      group.push_back(keyword.name());
      std::sort(group.begin(), group.end());
      if (groups.insert(group).second)
        rules_.push_back(rules::ExactlyOneOf(group));
    }
    if (!keyword.required_keywords().empty())
      rules_.push_back(
          rules::Requires(keyword.name(), keyword.required_keywords()));
    for (const auto &other : keyword.conflicting_keywords()) {
      const auto pair = std::minmax(keyword.name(), other);
      if (conflicts.emplace(pair.first, pair.second).second)
        rules_.push_back(rules::Conflicts(pair.first, {pair.second}));
    }
  }
  rules_.insert(rules_.end(), user_rules_.begin(), user_rules_.end());
}

// ---------------------------------------------------------------------------
// Coherence analysis
// ---------------------------------------------------------------------------
void DictionaryGrammar::Check() {
  const std::size_t n = keywords_.size();
  std::vector<std::string> problems;
  const auto fail_if_any = [&]() {
    if (problems.empty())
      return;
    std::vector<Diagnostic> diagnostics;
    diagnostics.reserve(problems.size());
    for (auto &problem : problems) {
      Diagnostic diagnostic;
      diagnostic.code = DiagnosticCode::GrammarIncoherent;
      diagnostic.message = std::move(problem);
      diagnostics.push_back(std::move(diagnostic));
    }
    throw DictionaryError(std::move(diagnostics), Context());
  };

  // 1. Names, duplicates, types ------------------------------------------
  std::unordered_map<std::string_view, std::size_t> index;
  std::unordered_set<std::string_view> reported_duplicates;
  index.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    const std::string &name = keywords_[i].name();
    if (name.size() < 2 || name.front() != '@')
      problems.push_back("invalid keyword name '" + name +
                         "': names must start with '@' followed by at least "
                         "one character");
    else if (name.find_first_of(" \t\r\n;{}\"") != std::string::npos)
      problems.push_back("invalid keyword name '" + name +
                         "': names cannot contain blanks, ';', '{', '}' or "
                         "'\"'");
    if (keyword_type_name(keywords_[i].type()) == "unknown")
      problems.push_back("the keyword " + name + " has an invalid type");
    if (!index.emplace(name, i).second &&
        reported_duplicates.insert(name).second)
      problems.push_back("the keyword " + name + " is defined more than once");
  }
  fail_if_any();

  // 2. Legacy references ---------------------------------------------------
  const auto check_list = [&](const std::size_t i,
                              const std::vector<std::string> &list,
                              const char *label) {
    std::unordered_set<std::string_view> seen;
    const std::string &name = keywords_[i].name();
    for (const auto &reference : list) {
      if (reference == name)
        problems.push_back("the keyword " + name +
                           " lists itself among its " + label);
      else if (!index.contains(reference))
        problems.push_back("the keyword " + name +
                           " lists the undefined keyword " + reference +
                           " among its " + label);
      if (!seen.insert(reference).second)
        problems.push_back("the keyword " + name + " lists " + reference +
                           " more than once among its " + label);
    }
  };

  for (std::size_t i = 0; i < n; ++i) {
    const DictionaryKeyWord &k = keywords_[i];
    check_list(i, k.compulsory_alternatives(), "compulsory alternatives");
    check_list(i, k.required_keywords(), "required keywords");
    check_list(i, k.conflicting_keywords(), "conflicting keywords");

    if (!k.is_compulsory() && !k.compulsory_alternatives().empty())
      problems.push_back("the keyword " + k.name() +
                         " is not compulsory but declares compulsory "
                         "alternatives");

    for (const auto &required : k.required_keywords()) {
      const auto &conflicting = k.conflicting_keywords();
      if (std::find(conflicting.begin(), conflicting.end(), required) !=
          conflicting.end())
        problems.push_back("the keyword " + k.name() +
                           " both requires and conflicts with " + required);
      const auto &alternatives = k.compulsory_alternatives();
      if (std::find(alternatives.begin(), alternatives.end(), required) !=
          alternatives.end())
        problems.push_back("the keyword " + k.name() + " requires " +
                           required +
                           ", which is one of its mutually exclusive "
                           "compulsory alternatives");
    }
  }
  fail_if_any();

  // 3. Legacy alternative groups and conflict symmetry ------------------------
  const auto group_members = [&](const std::size_t i) {
    std::set<std::size_t> group{i};
    for (const auto &alternative : keywords_[i].compulsory_alternatives())
      group.insert(index.at(alternative));
    return group;
  };
  const auto group_names = [&](const std::set<std::size_t> &group) {
    std::vector<std::string> names;
    for (const std::size_t m : group)
      names.push_back(keywords_[m].name());
    return "{" + join(names) + "}";
  };

  std::set<std::pair<std::size_t, std::size_t>> reported_pairs;
  for (std::size_t i = 0; i < n; ++i) {
    const DictionaryKeyWord &k = keywords_[i];
    if (!k.is_compulsory() || k.compulsory_alternatives().empty())
      continue;
    const auto group_i = group_members(i);
    for (const auto &alternative : k.compulsory_alternatives()) {
      const std::size_t m = index.at(alternative);
      if (!keywords_[m].is_compulsory()) {
        problems.push_back("the keyword " + k.name() + " lists " +
                           alternative +
                           " as a compulsory alternative, but " +
                           alternative + " is not compulsory");
        continue;
      }
      const auto group_m = group_members(m);
      if (group_m != group_i &&
          reported_pairs.emplace(std::min(i, m), std::max(i, m)).second)
        problems.push_back(
            "inconsistent compulsory alternatives: " + k.name() +
            " defines the group " + group_names(group_i) + " but " +
            alternative + " defines " + group_names(group_m) +
            "; every member of a group must list all the other members");
    }
  }

  for (std::size_t i = 0; i < n; ++i)
    for (const auto &conflicting : keywords_[i].conflicting_keywords()) {
      const auto &back = keywords_[index.at(conflicting)].conflicting_keywords();
      if (std::find(back.begin(), back.end(), keywords_[i].name()) ==
          back.end())
        problems.push_back(
            "the keyword " + keywords_[i].name() + " conflicts with " +
            conflicting + ", but " + conflicting + " does not list " +
            keywords_[i].name() +
            " among its conflicting keywords (conflicts must be declared on "
            "both keywords)");
    }
  fail_if_any();

  // 4. Rules registered with AddRule ----------------------------------------
  for (const auto &rule : user_rules_) {
    const std::string what = "the rule '" + rule.describe() + "'";
    std::set<std::string> seen;
    for (const auto &keyword : rule.keywords) {
      if (!index.contains(keyword))
        problems.push_back(what + " refers to the undefined keyword " +
                           keyword);
      if (!seen.insert(keyword).second)
        problems.push_back(what + " lists " + keyword + " more than once");
    }
    const std::size_t size = rule.keywords.size();
    switch (rule.kind) {
    case Rule::Kind::Required:
    case Rule::Kind::Forbidden:
      if (size != 1)
        problems.push_back(what + " must name exactly one keyword");
      break;
    case Rule::Kind::Exactly:
    case Rule::Kind::AtLeast:
      if (size < 2)
        problems.push_back(what + " must name at least two keywords");
      else if (rule.count < 1 || rule.count > size)
        problems.push_back(what + " can never be satisfied: the count must "
                                  "be between 1 and " +
                           std::to_string(size));
      break;
    case Rule::Kind::AtMost:
      if (size < 2)
        problems.push_back(what + " must name at least two keywords");
      else if (rule.count < 1 || rule.count >= size)
        problems.push_back(what + " has no effect or forbids every keyword: "
                                  "the count must be between 1 and " +
                           std::to_string(size - 1));
      break;
    case Rule::Kind::Requires:
    case Rule::Kind::RequiresAny:
    case Rule::Kind::Conflicts:
      if (size < 2)
        problems.push_back(what + " must name a keyword and at least one "
                                  "other keyword");
      break;
    }

    const Condition &c = rule.when;
    if (c.kind == Condition::Kind::Always)
      continue;
    const auto it = index.find(c.keyword);
    if (it == index.end()) {
      problems.push_back(what + " has a condition on the undefined keyword " +
                         c.keyword);
      continue;
    }
    const DictionaryKeyWord &tested = keywords_[it->second];
    if (c.kind != Condition::Kind::Equals && c.kind != Condition::Kind::OneOf)
      continue;
    if (!is_single_token(tested)) {
      problems.push_back(what + " compares the value of " + c.keyword +
                         ", whose type (" + tested.type_ascii() +
                         ") is not a single string, boolean, integer, "
                         "character or dictionary name");
      continue;
    }
    if (c.values.empty())
      problems.push_back(what + " has a condition without values");
    for (const auto &value : c.values) {
      const auto issues = detail::validate_value(tested, {value}, units::si_kmol);
      if (!issues.empty())
        problems.push_back(what + " compares " + c.keyword + " with " +
                           detail::quote(value) +
                           ", which is not a valid value: " +
                           issues.front().message);
    }
  }
  fail_if_any();

  BuildRules();

  // 5. Keyword properties ------------------------------------------------------
  std::unordered_set<std::string> required_by_rule;
  for (const auto &rule : rules_)
    if (rule.kind == Rule::Kind::Required &&
        rule.when.kind == Condition::Kind::Always)
      required_by_rule.insert(rule.keywords.front());

  for (const auto &k : keywords_) {
    const std::string &name = k.name();
    const auto type = k.type();
    if (type == STRUCTURED) {
      if (!k.format())
        problems.push_back("the structured keyword " + name +
                           " has no format (structured keywords can only be "
                           "declared in C++ grammars)");
      else
        for (const auto &problem : k.format()->problems())
          problems.push_back("the format of " + name + " is not valid: " +
                             problem);
      if (k.dimension() || !k.allowed_values().empty() || k.minimum() ||
          k.maximum() || k.size_min() || k.size_max() || k.sum())
        problems.push_back("the structured keyword " + name +
                           " declares a dimension, allowed values, a range, "
                           "a size or a sum: use the format modifiers "
                           "instead (e.g. formats::Real().Minimum(0))");
    }
    if (k.dimension() && type != SINGLE_MEASURE && type != VECTOR_MEASURE &&
        type != STRUCTURED)
      problems.push_back("the keyword " + name + " declares a dimension (" +
                         units::describe(*k.dimension()) +
                         "), but only single_measure and vector_measure "
                         "keywords can have one");
    if (!k.allowed_values().empty() && type != STRUCTURED) {
      if (type != SINGLE_STRING && type != VECTOR_STRING)
        problems.push_back("the keyword " + name +
                           " declares allowed values, but only single_string "
                           "and vector_string keywords can have them");
      std::set<std::string> seen;
      for (const auto &value : k.allowed_values())
        if (value.empty() || !seen.insert(value).second)
          problems.push_back("the keyword " + name +
                             " has an empty or repeated allowed value '" +
                             value + "'");
    }
    if ((k.minimum() || k.maximum()) && type != STRUCTURED) {
      if (!is_numeric_type(type))
        problems.push_back("the keyword " + name +
                           " declares a numeric range, but its type (" +
                           k.type_ascii() + ") is not numeric");
      else if ((type == SINGLE_MEASURE || type == VECTOR_MEASURE) &&
               !k.dimension())
        problems.push_back("the keyword " + name +
                           " declares a numeric range for a measure, which "
                           "requires a dimension (the range is in SI units)");
      if (k.minimum() && k.maximum() &&
          (k.minimum()->value > k.maximum()->value ||
           (k.minimum()->value == k.maximum()->value &&
            !(k.minimum()->inclusive && k.maximum()->inclusive))))
        problems.push_back("the keyword " + name + " has an empty range: " +
                           format_number(k.minimum()->value) + " to " +
                           format_number(k.maximum()->value));
    }
    if ((k.size_min() || k.size_max()) && type != STRUCTURED) {
      if (!is_list_type(type))
        problems.push_back("the keyword " + name +
                           " declares a list size, but its type (" +
                           k.type_ascii() + ") is not a list");
      if (k.size_min() && k.size_max() && *k.size_min() > *k.size_max())
        problems.push_back("the keyword " + name +
                           " has a minimum size larger than its maximum size");
    }
    if (k.sum() && type != STRUCTURED) {
      if (type != VECTOR_DOUBLE && type != VECTOR_STRING_DOUBLE)
        problems.push_back("the keyword " + name +
                           " declares a sum, but only vector_double and "
                           "vector_string_double keywords can have one");
      if (k.sum()->second < 0.0)
        problems.push_back("the keyword " + name +
                           " has a negative sum tolerance");
    }
    if (k.default_value()) {
      if (k.is_compulsory() || required_by_rule.contains(name)) {
        problems.push_back("the keyword " + name +
                           " is compulsory, so it cannot have a default value");
      } else {
        try {
          std::vector<std::string> values;
          for (const auto &token :
               Lexer::tokenize(*k.default_value(), "default of " + name))
            values.push_back(token.text);
          const auto issues =
              detail::validate_value(k, values, units::si_kmol);
          for (const auto &issue : issues)
            problems.push_back("the default value '" + *k.default_value() +
                               "' of " + name + " is not valid: " +
                               issue.message);
        } catch (const DictionaryError &error) {
          problems.push_back("the default value '" + *k.default_value() +
                             "' of " + name + " cannot be parsed: " +
                             error.diagnostics().front().message);
        }
      }
    }
  }
  fail_if_any();

  // 6. Satisfiability and dead keywords (unconditional rules) ------------------
  std::vector<char> conflict(n * n, 0);
  std::vector<std::vector<std::size_t>> requires_edges(n);
  std::vector<char> mandatory(n, 0);
  std::vector<std::string> forbidden(n);
  std::vector<std::vector<std::size_t>> groups_of(n); // exactly-one groups
  struct Counting {
    const Rule *rule;
    std::vector<std::size_t> members;
  };
  std::vector<Counting> counting;

  const auto set_conflict = [&](const std::size_t a, const std::size_t b) {
    conflict[a * n + b] = conflict[b * n + a] = 1;
  };
  std::size_t group_count = 0;
  for (const auto &rule : rules_) {
    if (rule.when.kind != Condition::Kind::Always)
      continue;
    std::vector<std::size_t> ids;
    for (const auto &keyword : rule.keywords)
      ids.push_back(index.at(keyword));
    switch (rule.kind) {
    case Rule::Kind::Required: mandatory[ids[0]] = 1; break;
    case Rule::Kind::Forbidden:
      forbidden[ids[0]] = "a rule forbids it";
      break;
    case Rule::Kind::Requires:
      for (std::size_t k = 1; k < ids.size(); ++k)
        requires_edges[ids[0]].push_back(ids[k]);
      break;
    case Rule::Kind::Conflicts:
      for (std::size_t k = 1; k < ids.size(); ++k)
        set_conflict(ids[0], ids[k]);
      break;
    case Rule::Kind::Exactly:
    case Rule::Kind::AtLeast:
    case Rule::Kind::AtMost:
      if ((rule.kind == Rule::Kind::Exactly || rule.kind == Rule::Kind::AtMost) &&
          rule.count == 1) {
        for (std::size_t a = 0; a < ids.size(); ++a)
          for (std::size_t b = a + 1; b < ids.size(); ++b)
            set_conflict(ids[a], ids[b]);
      }
      if (rule.kind == Rule::Kind::Exactly && rule.count == 1) {
        for (const std::size_t id : ids)
          groups_of[id].push_back(group_count);
        ++group_count;
      }
      counting.push_back({&rule, ids});
      break;
    case Rule::Kind::RequiresAny:
      break;
    }
  }
  const auto conflicts = [&](const std::size_t a, const std::size_t b) {
    return conflict[a * n + b] != 0;
  };
  const auto same_group = [&](const std::size_t a, const std::size_t b) {
    for (const std::size_t g : groups_of[a])
      if (std::find(groups_of[b].begin(), groups_of[b].end(), g) !=
          groups_of[b].end())
        return true;
    return false;
  };

  for (std::size_t a = 0; a < n; ++a)
    for (std::size_t b = a + 1; b < n; ++b)
      if (mandatory[a] && mandatory[b] && conflicts(a, b))
        problems.push_back("the compulsory keywords " + keywords_[a].name() +
                           " and " + keywords_[b].name() +
                           " conflict with each other: the grammar can never "
                           "be satisfied");
  fail_if_any();

  // Transitive closure of the requirements (each closure contains itself).
  std::vector<std::vector<std::size_t>> closure(n);
  for (std::size_t i = 0; i < n; ++i) {
    std::vector<char> seen(n, 0);
    std::vector<std::size_t> stack{i};
    seen[i] = 1;
    while (!stack.empty()) {
      const std::size_t current = stack.back();
      stack.pop_back();
      closure[i].push_back(current);
      for (const std::size_t next : requires_edges[current])
        if (!seen[next]) {
          seen[next] = 1;
          stack.push_back(next);
        }
    }
    std::sort(closure[i].begin(), closure[i].end()); // deterministic messages
  }

  std::vector<char> always(n, 0);
  for (std::size_t m = 0; m < n; ++m)
    if (mandatory[m])
      for (const std::size_t r : closure[m])
        always[r] = 1;

  const auto name_of = [&](const std::size_t i) -> const std::string & {
    return keywords_[i].name();
  };

  std::vector<std::string> dead(n);
  for (std::size_t i = 0; i < n; ++i) {
    const auto &set = closure[i];
    std::string reason;
    for (const std::size_t a : set)
      if (reason.empty() && !forbidden[a].empty())
        reason = a == i ? std::string("a rule forbids it")
                        : "it requires " + name_of(a) +
                              ", which a rule forbids";
    for (std::size_t x = 0; x < set.size() && reason.empty(); ++x)
      for (std::size_t y = x + 1; y < set.size() && reason.empty(); ++y) {
        const std::size_t a = set[x];
        const std::size_t b = set[y];
        const bool involves_self = (a == i || b == i);
        const std::size_t other = (a == i) ? b : a;
        if (same_group(a, b))
          reason = involves_self
                       ? "it requires " + name_of(other) +
                             ", which is one of its mutually exclusive "
                             "compulsory alternatives"
                       : "it requires both " + name_of(a) + " and " +
                             name_of(b) +
                             ", which are mutually exclusive compulsory "
                             "alternatives";
        else if (conflicts(a, b))
          reason = involves_self ? "it requires " + name_of(other) +
                                       ", which conflicts with it"
                                 : "it requires both " + name_of(a) + " and " +
                                       name_of(b) +
                                       ", which conflict with each other";
      }
    for (std::size_t x = 0; x < set.size() && reason.empty(); ++x) {
      const std::size_t a = set[x];
      for (std::size_t m = 0; m < n && reason.empty(); ++m) {
        if (!always[m] || m == a)
          continue;
        if (same_group(a, m))
          reason = (a == i ? std::string("it") : "it requires " + name_of(a) +
                                                     ", which") +
                   " is a mutually exclusive alternative of " + name_of(m) +
                   ", which is always required";
        else if (conflicts(a, m))
          reason = (a == i ? std::string("it") : "it requires " + name_of(a) +
                                                     ", which") +
                   " conflicts with " + name_of(m) +
                   ", which is always required";
      }
    }
    dead[i] = std::move(reason);
  }

  for (std::size_t i = 0; i < n; ++i)
    if (always[i] && !dead[i].empty())
      problems.push_back("the grammar can never be satisfied: " + name_of(i) +
                         " is always required, but " + dead[i]);

  // Counting rules: enough usable members, not too many always present.
  std::vector<char> covered(n, 0); // dead keys already explained by a group
  for (const auto &group : counting) {
    const Rule &rule = *group.rule;
    std::size_t usable = 0;
    std::size_t forced = 0;
    std::vector<std::string> names;
    for (const std::size_t m : group.members) {
      usable += dead[m].empty() ? 1 : 0;
      forced += always[m] ? 1 : 0;
      names.push_back(name_of(m));
    }
    const bool needs = rule.kind != Rule::Kind::AtMost;
    if (needs && usable < rule.count) {
      for (const std::size_t m : group.members)
        covered[m] = 1;
      if (rule.kind == Rule::Kind::Exactly && rule.count == 1 &&
          usable == 0) {
        std::string detail;
        for (const std::size_t m : group.members)
          detail += "; " + name_of(m) + ": " + dead[m];
        problems.push_back("the grammar can never be satisfied: none of the "
                           "compulsory alternatives {" +
                           join(names) + "} can be used" + detail);
      } else {
        problems.push_back("the grammar can never be satisfied: the rule '" +
                           rule.describe() + "' needs " +
                           std::to_string(rule.count) +
                           " keywords, but only " + std::to_string(usable) +
                           " of them can be used");
      }
    }
    if (rule.kind != Rule::Kind::AtLeast && forced > rule.count)
      problems.push_back("the grammar can never be satisfied: the rule '" +
                         rule.describe() + "' allows " +
                         std::to_string(rule.count) + ", but " +
                         std::to_string(forced) +
                         " of its keywords are always required");
  }

  for (std::size_t i = 0; i < n; ++i)
    if (!always[i] && !dead[i].empty() && !covered[i])
      problems.push_back("the keyword " + name_of(i) +
                         " can never be used: " + dead[i]);

  fail_if_any();
}

// ---------------------------------------------------------------------------
// Documentation
// ---------------------------------------------------------------------------
void DictionaryGrammar::ShortSummary(std::ostream &fout) const {
  fout << "Grammar";
  if (!name_.empty())
    fout << " of dictionary " << name_;
  if (!file_name_.empty())
    fout << " defined in file " << file_name_.filename();
  fout << std::endl;
  fout << std::string(101, '-') << std::endl << std::endl;
  for (const auto &keyword : keywords_) {
    keyword.ShortSummary(fout);
    fout << std::endl;
  }
  fout << std::string(101, '-') << std::endl;
}

std::string
DictionaryGrammar::AvailableKeyWordsTable(std::string_view dictionary_name,
                                          const units::UnitSystem &system) const {
  const std::string title =
      dictionary_name.empty() ? name_ : std::string(dictionary_name);
  std::ostringstream out;
  const std::string rule(82, '-');
  out << rule << '\n'
      << "List of available options in Dictionary " << title << '\n'
      << rule << '\n';
  for (const auto &keyword : keywords_) {
    out << std::setw(45) << std::left << keyword.name();
    out << std::setw(20) << std::left << keyword.type_ascii();
    if (keyword.dimension())
      out << "[" << units::si_unit(*keyword.dimension(), system) << "] ";
    out << std::left << keyword.comment_short();
    if (keyword.format())
      out << " (format: " << keyword.format()->describe(system) << ")";
    if (keyword.default_value())
      out << " (default: " << *keyword.default_value() << ")";
    if (!keyword.allowed_values().empty()) {
      out << " (allowed:";
      for (const auto &value : keyword.allowed_values())
        out << ' ' << value;
      out << ")";
    }
    if (keyword.deprecated())
      out << " (deprecated)";
    out << '\n';
  }
  if (!rules_.empty()) {
    out << rule << '\n' << "Rules\n";
    for (const auto &r : rules_)
      out << "  - " << r.describe() << '\n';
  }
  out << rule << '\n';
  return out.str();
}

void DictionaryGrammar::ShowListAvailableKeywords(std::ostream &out) const {
  out << '\n' << AvailableKeyWordsTable() << '\n';
}

std::string DictionaryGrammar::Describe(std::string_view title) const {
  const auto escape = [](std::string text) {
    std::string result;
    for (const char c : text) {
      if (c == '|')
        result += "\\|";
      else
        result += c;
    }
    return result;
  };
  std::unordered_map<std::string, std::string> required;
  for (const auto &rule : rules_)
    if (rule.kind == Rule::Kind::Required)
      required[rule.keywords.front()] =
          rule.when.kind == Condition::Kind::Always ? "yes"
                                                    : rule.when.describe();

  std::ostringstream out;
  const std::string heading =
      title.empty() ? (name_.empty() ? std::string("Grammar") : name_)
                    : std::string(title);
  out << "## " << heading << "\n\n";
  out << "| Keyword | Type | Units (SI) | Default | Required | Constraints | "
         "Description |\n";
  out << "| --- | --- | --- | --- | --- | --- | --- |\n";
  for (const auto &k : keywords_) {
    std::vector<std::string> constraints;
    if (!k.allowed_values().empty())
      constraints.push_back("one of: " + join(k.allowed_values()));
    if (k.minimum())
      constraints.push_back(std::string(k.minimum()->inclusive ? "≥ " : "> ") +
                            format_number(k.minimum()->value));
    if (k.maximum())
      constraints.push_back(std::string(k.maximum()->inclusive ? "≤ " : "< ") +
                            format_number(k.maximum()->value));
    if (k.size_min() || k.size_max()) {
      const std::size_t lo = k.size_min().value_or(0);
      const std::size_t hi = k.size_max().value_or(static_cast<std::size_t>(-1));
      constraints.push_back(
          lo == hi ? std::to_string(lo) + " elements"
          : hi == static_cast<std::size_t>(-1)
              ? "at least " + std::to_string(lo) + " elements"
              : std::to_string(lo) + " to " + std::to_string(hi) + " elements");
    }
    if (k.sum())
      constraints.push_back("sum = " + format_number(k.sum()->first));
    if (k.deprecated())
      constraints.push_back("deprecated" + (k.deprecated()->empty()
                                                ? std::string()
                                                : ": " + *k.deprecated()));
    const auto it = required.find(k.name());
    out << "| `" << k.name() << "` | "
        << (k.format() ? escape(k.format()->describe()) : k.type_ascii())
        << " | "
        << (k.dimension() ? units::si_unit(*k.dimension()) : std::string())
        << " | "
        << (k.default_value() ? "`" + escape(*k.default_value()) + "`"
                              : std::string())
        << " | " << (it != required.end() ? escape(it->second) : std::string())
        << " | " << escape(join(constraints)) << " | "
        << escape(Lexer::trim_copy(k.comment_short())) << " |\n";
  }
  bool has_other_rules = false;
  for (const auto &rule : rules_)
    has_other_rules = has_other_rules || rule.kind != Rule::Kind::Required ||
                      rule.when.kind != Condition::Kind::Always;
  if (has_other_rules) {
    out << "\n**Rules**\n\n";
    for (const auto &rule : rules_)
      if (rule.kind != Rule::Kind::Required ||
          rule.when.kind != Condition::Kind::Always)
        out << "- " << escape(rule.describe()) << "\n";
  }
  return out.str();
}

// ---------------------------------------------------------------------------
// Validation of a dictionary
// ---------------------------------------------------------------------------
bool DictionaryGrammar::HasHelpKeyWord(
    const std::vector<std::string> &list_of_keywords) const {
  return std::find(list_of_keywords.begin(), list_of_keywords.end(),
                   "@Help") != list_of_keywords.end();
}

auto DictionaryGrammar::CollectUndefinedKeyWords(
    const std::vector<std::string> &list_of_keywords) const
    -> std::vector<Violation> {
  std::vector<Violation> violations;
  for (const auto &keyword : list_of_keywords) {
    if (FindKeyWord(keyword) != nullptr)
      continue;
    Violation violation;
    violation.code = DiagnosticCode::UnknownKeyword;
    violation.message =
        "the keyword " + keyword + " is not recognized in this dictionary";
    violation.keyword = keyword;
    const std::string suggestion =
        detail::closest_match(keyword, list_of_keywords_names_);
    if (!suggestion.empty())
      violation.hint = "did you mean " + suggestion + "?";
    violations.push_back(std::move(violation));
  }
  return violations;
}

auto DictionaryGrammar::CollectRuleViolations(
    const std::vector<std::string> &list_of_keywords,
    const ValueLookup &value_of) const -> std::vector<Violation> {
  const auto position = [&list_of_keywords](const std::string &name) {
    const auto it =
        std::find(list_of_keywords.begin(), list_of_keywords.end(), name);
    return it == list_of_keywords.end()
               ? none
               : static_cast<std::size_t>(it - list_of_keywords.begin());
  };
  const auto present = [&](const std::string &name) {
    return position(name) != none;
  };
  const auto value_matches = [&](const std::string &keyword,
                                 const std::string &expected) {
    const auto value = value_of(keyword);
    if (!value)
      return false;
    const DictionaryKeyWord *declared = FindKeyWord(keyword);
    return declared != nullptr ? same_value(*declared, *value, expected)
                               : *value == expected;
  };
  const auto holds = [&](const Condition &c) {
    switch (c.kind) {
    case Condition::Kind::Always: return true;
    case Condition::Kind::Present: return present(c.keyword);
    case Condition::Kind::Absent: return !present(c.keyword);
    case Condition::Kind::Equals:
      return !c.values.empty() && value_matches(c.keyword, c.values[0]);
    case Condition::Kind::OneOf:
      return std::any_of(c.values.begin(), c.values.end(),
                         [&](const std::string &v) {
                           return value_matches(c.keyword, v);
                         });
    }
    return true;
  };

  std::vector<Violation> violations;
  for (const auto &rule : rules_) {
    if (!holds(rule.when))
      continue;
    const std::string condition = rule.when.describe();
    const std::string suffix = condition.empty() ? "" : " (" + condition + ")";
    const auto add = [&](DiagnosticCode code, std::string message,
                         std::string keyword,
                         std::vector<std::pair<std::string, std::string>>
                             related = {}) {
      if (!condition.empty() && present(rule.when.keyword))
        related.emplace_back(rule.when.keyword,
                             rule.when.keyword + " is specified here");
      violations.push_back(Violation{code, std::move(message) + suffix,
                                     std::move(keyword), {},
                                     std::move(related)});
    };

    // Members present, in file order.
    std::vector<std::string> found;
    for (const auto &name : list_of_keywords)
      if (std::find(rule.keywords.begin(), rule.keywords.end(), name) !=
          rule.keywords.end())
        found.push_back(name);
    const auto notes_for = [](const std::vector<std::string> &names,
                              std::size_t count) {
      std::vector<std::pair<std::string, std::string>> related;
      for (std::size_t i = 0; i < count; ++i)
        related.emplace_back(names[i], names[i] + " is specified here");
      return related;
    };

    const std::string &subject = rule.keywords.front();
    switch (rule.kind) {
    case Rule::Kind::Required:
      if (!present(subject))
        add(DiagnosticCode::MissingCompulsory,
            "the compulsory keyword " + subject + " is missing", {});
      break;
    case Rule::Kind::Forbidden:
      if (present(subject))
        add(DiagnosticCode::Forbidden,
            "the keyword " + subject + " cannot be used", subject);
      break;
    case Rule::Kind::Exactly:
    case Rule::Kind::AtLeast:
    case Rule::Kind::AtMost: {
      const std::size_t k = found.size();
      const bool too_few = rule.kind != Rule::Kind::AtMost && k < rule.count;
      const bool too_many = rule.kind != Rule::Kind::AtLeast && k > rule.count;
      if (!too_few && !too_many)
        break;
      const bool exactly_one =
          rule.kind == Rule::Kind::Exactly && rule.count == 1;
      if (exactly_one && too_few)
        add(DiagnosticCode::AlternativeGroup,
            "one of the compulsory keywords " + join(rule.keywords) +
                " must be specified",
            {});
      else if (exactly_one)
        add(DiagnosticCode::AlternativeGroup,
            "only one of the mutually exclusive keywords " +
                join(rule.keywords) + " can be specified, but " + join(found) +
                " are present",
            found.back(), notes_for(found, found.size() - 1));
      else {
        const std::string count_text =
            (rule.kind == Rule::Kind::Exactly
                 ? "exactly "
                 : (rule.kind == Rule::Kind::AtLeast ? "at least "
                                                     : "at most ")) +
            (rule.count == 1 ? std::string("one")
                             : std::to_string(rule.count)) +
            " of " + join(rule.keywords);
        const std::string verb = rule.kind == Rule::Kind::AtMost
                                     ? " can be specified, but "
                                     : " must be specified, but ";
        const std::string found_text =
            k == 0 ? std::string("none is present")
                   : (k == 1 ? std::string("only 1 is present")
                             : std::to_string(k) + " are present") +
                         " (" + join(found) + ")";
        if (too_few)
          add(DiagnosticCode::Cardinality, count_text + verb + found_text, {});
        else
          add(DiagnosticCode::Cardinality, count_text + verb + found_text,
              found.back(), notes_for(found, found.size() - 1));
      }
      break;
    }
    case Rule::Kind::Requires:
      if (present(subject))
        for (std::size_t i = 1; i < rule.keywords.size(); ++i)
          if (!present(rule.keywords[i]))
            add(DiagnosticCode::MissingRequired,
                "the keyword " + subject + " requires " + rule.keywords[i] +
                    ", which is missing",
                subject);
      break;
    case Rule::Kind::RequiresAny:
      if (present(subject) &&
          std::none_of(rule.keywords.begin() + 1, rule.keywords.end(),
                       present)) {
        std::vector<std::string> candidates(rule.keywords.begin() + 1,
                                            rule.keywords.end());
        add(DiagnosticCode::MissingRequired,
            "the keyword " + subject + " requires one of " +
                join(candidates) + ", but none is present",
            subject);
      }
      break;
    case Rule::Kind::Conflicts:
      if (present(subject))
        for (std::size_t i = 1; i < rule.keywords.size(); ++i) {
          const std::string &other = rule.keywords[i];
          if (!present(other))
            continue;
          const auto pair = std::minmax(subject, other);
          const bool subject_later = position(subject) > position(other);
          const std::string &later = subject_later ? subject : other;
          const std::string &earlier = subject_later ? other : subject;
          add(DiagnosticCode::Conflict,
              "the keywords " + pair.first + " and " + pair.second +
                  " are mutually exclusive and cannot be used together",
              later, {{earlier, earlier + " is specified here"}});
        }
      break;
    }
  }
  return violations;
}
} // namespace OpenSMOKEpp
