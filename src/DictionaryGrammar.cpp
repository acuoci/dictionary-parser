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

} // namespace

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
  number_of_keywords_ = 0;

  DefineRules();

  number_of_keywords_ = static_cast<unsigned int>(keywords_.size());
  list_of_keywords_names_.resize(number_of_keywords_);
  for (unsigned int i = 0; i < number_of_keywords_; i++)
    list_of_keywords_names_[i] = keywords_[i].name();

  Check();
}

void DictionaryGrammar::AddKeyWord(const DictionaryKeyWord &keyword) {
  keywords_.push_back(keyword);
}

void DictionaryGrammar::ReadFromFile(std::string_view file_name) {
  file_name_ = std::filesystem::path(std::string(file_name));
  keywords_.clear();
  list_of_keywords_names_.clear();
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

  std::vector<std::size_t> line_keywords;
  for (std::size_t i = 0; i < lines.size(); i++) {
    const auto tokens = Lexer::split_whitespace_view(lines[i]);
    if (!tokens.empty() && tokens.front() == "keyword:")
      line_keywords.push_back(i);
  }

  keywords_.reserve(line_keywords.size());
  for (const std::size_t first : line_keywords) {
    if (first + 7 > lines.size())
      ErrorMessage("A keyword block starting at line " +
                   std::to_string(first + 1) +
                   " is truncated. Each keyword definition must contain "
                   "seven lines.");
    std::vector<std::string> block(lines.begin() + static_cast<std::ptrdiff_t>(first),
                                   lines.begin() + static_cast<std::ptrdiff_t>(first + 7));
    keywords_.emplace_back(block);
    // Optional eighth line: "dimension: <SI unit> [difference]"
    if (first + 7 < lines.size()) {
      const auto tokens = Lexer::split_whitespace_view(lines[first + 7]);
      if (!tokens.empty() && tokens.front() == "dimension:")
        keywords_.back().SetDimensionFromLine(lines[first + 7]);
    }
  }

  number_of_keywords_ = static_cast<unsigned int>(keywords_.size());
  list_of_keywords_names_.resize(number_of_keywords_);
  for (unsigned int i = 0; i < number_of_keywords_; i++)
    list_of_keywords_names_[i] = keywords_[i].name();

  Check();
}

const DictionaryKeyWord *
DictionaryGrammar::FindKeyWord(std::string_view keyword_name) const noexcept {
  for (const auto &keyword : keywords_)
    if (keyword.name() == keyword_name)
      return &keyword;
  return nullptr;
}

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
    if (keywords_[i].dimension() && keywords_[i].type() != SINGLE_MEASURE &&
        keywords_[i].type() != VECTOR_MEASURE)
      problems.push_back("the keyword " + name + " declares a dimension (" +
                         units::describe(*keywords_[i].dimension()) +
                         "), but only single_measure and vector_measure "
                         "keywords can have one");
    if (!index.emplace(name, i).second &&
        reported_duplicates.insert(name).second)
      problems.push_back("the keyword " + name + " is defined more than once");
  }
  fail_if_any();

  // 2. References -------------------------------------------------------------
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
      const auto &conflicts = k.conflicting_keywords();
      if (std::find(conflicts.begin(), conflicts.end(), required) !=
          conflicts.end())
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

  // 3. Alternative groups and conflict symmetry -------------------------------
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

  // 4. Satisfiability and dead keywords ---------------------------------------
  std::vector<std::size_t> group_of(n, none);
  std::vector<std::set<std::size_t>> groups;
  for (std::size_t i = 0; i < n; ++i) {
    const DictionaryKeyWord &k = keywords_[i];
    if (!k.is_compulsory() || k.compulsory_alternatives().empty() ||
        group_of[i] != none)
      continue;
    groups.push_back(group_members(i));
    for (const std::size_t m : groups.back())
      group_of[m] = groups.size() - 1;
  }

  std::vector<char> conflict(n * n, 0);
  for (std::size_t i = 0; i < n; ++i)
    for (const auto &conflicting : keywords_[i].conflicting_keywords()) {
      const std::size_t j = index.at(conflicting);
      conflict[i * n + j] = conflict[j * n + i] = 1;
    }
  const auto conflicts = [&](const std::size_t a, const std::size_t b) {
    return conflict[a * n + b] != 0;
  };

  std::vector<char> mandatory(n, 0);
  for (std::size_t i = 0; i < n; ++i)
    mandatory[i] = keywords_[i].is_compulsory() &&
                   keywords_[i].compulsory_alternatives().empty();

  for (std::size_t a = 0; a < n; ++a)
    for (std::size_t b = a + 1; b < n; ++b)
      if (mandatory[a] && mandatory[b] && conflicts(a, b))
        problems.push_back("the compulsory keywords " + keywords_[a].name() +
                           " and " + keywords_[b].name() +
                           " conflict with each other: the grammar can never "
                           "be satisfied");
  fail_if_any();

  // Transitive closure of `required_keywords` (each closure contains itself).
  std::vector<std::vector<std::size_t>> closure(n);
  for (std::size_t i = 0; i < n; ++i) {
    std::vector<char> seen(n, 0);
    std::vector<std::size_t> stack{i};
    seen[i] = 1;
    while (!stack.empty()) {
      const std::size_t current = stack.back();
      stack.pop_back();
      closure[i].push_back(current);
      for (const auto &required : keywords_[current].required_keywords()) {
        const std::size_t next = index.at(required);
        if (!seen[next]) {
          seen[next] = 1;
          stack.push_back(next);
        }
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
    for (std::size_t x = 0; x < set.size() && reason.empty(); ++x)
      for (std::size_t y = x + 1; y < set.size() && reason.empty(); ++y) {
        const std::size_t a = set[x];
        const std::size_t b = set[y];
        const bool involves_self = (a == i || b == i);
        const std::size_t other = (a == i) ? b : a;
        if (conflicts(a, b))
          reason = involves_self ? "it requires " + name_of(other) +
                                       ", which conflicts with it"
                                 : "it requires both " + name_of(a) + " and " +
                                       name_of(b) +
                                       ", which conflict with each other";
        else if (group_of[a] != none && group_of[a] == group_of[b])
          reason = involves_self
                       ? "it requires " + name_of(other) +
                             ", which is one of its mutually exclusive "
                             "compulsory alternatives"
                       : "it requires both " + name_of(a) + " and " +
                             name_of(b) +
                             ", which are mutually exclusive compulsory "
                             "alternatives";
      }
    for (std::size_t x = 0; x < set.size() && reason.empty(); ++x) {
      const std::size_t a = set[x];
      for (std::size_t m = 0; m < n && reason.empty(); ++m) {
        if (!always[m] || m == a)
          continue;
        if (conflicts(a, m))
          reason = (a == i ? std::string("it") : "it requires " + name_of(a) +
                                                     ", which") +
                   " conflicts with " + name_of(m) +
                   ", which is always required";
        else if (group_of[a] != none && group_of[a] == group_of[m])
          reason = (a == i ? std::string("it") : "it requires " + name_of(a) +
                                                     ", which") +
                   " is a mutually exclusive alternative of " + name_of(m) +
                   ", which is always required";
      }
    }
    dead[i] = std::move(reason);
  }

  for (std::size_t i = 0; i < n; ++i)
    if (always[i] && !dead[i].empty())
      problems.push_back("the grammar can never be satisfied: " + name_of(i) +
                         " is always required, but " + dead[i]);

  std::vector<char> group_fully_dead(groups.size(), 0);
  for (std::size_t g = 0; g < groups.size(); ++g) {
    bool all_dead = true;
    for (const std::size_t m : groups[g])
      all_dead = all_dead && !dead[m].empty();
    if (all_dead) {
      group_fully_dead[g] = 1;
      std::string detail;
      for (const std::size_t m : groups[g])
        detail += "; " + name_of(m) + ": " + dead[m];
      problems.push_back("the grammar can never be satisfied: none of the "
                         "compulsory alternatives " +
                         group_names(groups[g]) + " can be used" + detail);
    }
  }

  for (std::size_t i = 0; i < n; ++i)
    if (!always[i] && !dead[i].empty() &&
        (group_of[i] == none || !group_fully_dead[group_of[i]]))
      problems.push_back("the keyword " + name_of(i) +
                         " can never be used: " + dead[i]);

  fail_if_any();
}

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
    out << std::left << keyword.comment_short() << '\n';
  }
  out << rule << '\n';
  return out.str();
}

void DictionaryGrammar::ShowListAvailableKeywords(std::ostream &out) const {
  out << '\n' << AvailableKeyWordsTable() << '\n';
}

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

auto DictionaryGrammar::CollectCompulsoryErrors(
    const std::vector<std::string> &list_of_keywords) const
    -> std::vector<Violation> {
  std::vector<Violation> violations;
  std::set<std::vector<std::string>> reported_groups;
  const auto present = [&list_of_keywords](const std::string &name) {
    return std::find(list_of_keywords.begin(), list_of_keywords.end(),
                     name) != list_of_keywords.end();
  };

  for (const auto &keyword : keywords_) {
    if (!keyword.is_compulsory())
      continue;
    if (keyword.compulsory_alternatives().empty()) {
      if (!present(keyword.name()))
        violations.push_back(Violation{
            DiagnosticCode::MissingCompulsory,
            "the compulsory keyword " + keyword.name() + " is missing",
            {},
            {},
            {}});
      continue;
    }

    std::vector<std::string> group = keyword.compulsory_alternatives();
    group.push_back(keyword.name());
    std::sort(group.begin(), group.end());
    if (!reported_groups.insert(group).second)
      continue;

    // Members found, in the order in which they appear in the dictionary.
    std::vector<std::string> found;
    for (const auto &name : list_of_keywords)
      if (std::find(group.begin(), group.end(), name) != group.end())
        found.push_back(name);

    if (found.empty()) {
      violations.push_back(Violation{DiagnosticCode::AlternativeGroup,
                                     "one of the compulsory keywords " +
                                         join(group) + " must be specified",
                                     {},
                                     {},
                                     {}});
    } else if (found.size() > 1) {
      Violation violation;
      violation.code = DiagnosticCode::AlternativeGroup;
      violation.message = "only one of the mutually exclusive keywords " +
                          join(group) + " can be specified, but " +
                          join(found) + " are present";
      violation.keyword = found.back();
      for (std::size_t i = 0; i + 1 < found.size(); ++i)
        violation.related.emplace_back(found[i], found[i] +
                                                     " is specified here");
      violations.push_back(std::move(violation));
    }
  }
  return violations;
}

auto DictionaryGrammar::CollectRequiredErrors(
    const std::vector<std::string> &list_of_keywords) const
    -> std::vector<Violation> {
  const std::unordered_set<std::string_view> present(list_of_keywords.begin(),
                                                     list_of_keywords.end());
  std::vector<Violation> violations;
  for (const auto &keyword : keywords_) {
    if (!present.contains(keyword.name()))
      continue;
    for (const auto &required : keyword.required_keywords())
      if (!present.contains(required))
        violations.push_back(Violation{DiagnosticCode::MissingRequired,
                                       "the keyword " + keyword.name() +
                                           " requires " + required +
                                           ", which is missing",
                                       keyword.name(),
                                       {},
                                       {}});
  }
  return violations;
}

auto DictionaryGrammar::CollectConflictErrors(
    const std::vector<std::string> &list_of_keywords) const
    -> std::vector<Violation> {
  const auto position = [&list_of_keywords](const std::string &name) {
    return std::find(list_of_keywords.begin(), list_of_keywords.end(), name) -
           list_of_keywords.begin();
  };
  std::vector<Violation> violations;
  std::set<std::pair<std::string, std::string>> reported;
  for (const auto &keyword : keywords_) {
    const auto first = position(keyword.name());
    if (first == static_cast<std::ptrdiff_t>(list_of_keywords.size()))
      continue;
    for (const auto &conflicting : keyword.conflicting_keywords()) {
      const auto second = position(conflicting);
      if (second == static_cast<std::ptrdiff_t>(list_of_keywords.size()))
        continue;
      const auto pair = std::minmax(keyword.name(), conflicting);
      if (!reported.emplace(pair.first, pair.second).second)
        continue;
      // Point at the keyword that appears last; the other one is a note.
      const std::string &later = first > second ? keyword.name() : conflicting;
      const std::string &earlier = first > second ? conflicting : keyword.name();
      violations.push_back(Violation{
          DiagnosticCode::Conflict,
          "the keywords " + pair.first + " and " + pair.second +
              " are mutually exclusive and cannot be used together",
          later,
          {},
          {{earlier, earlier + " is specified here"}}});
    }
  }
  return violations;
}
} // namespace OpenSMOKEpp
