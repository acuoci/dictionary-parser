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

#include "DictionaryRules.h"

#include <utility>

namespace OpenSMOKEpp {
namespace {
[[nodiscard]] auto join(const std::vector<std::string> &names,
                        const std::size_t first = 0,
                        const char *last_separator = ", ") -> std::string {
  std::string text;
  for (std::size_t i = first; i < names.size(); ++i) {
    if (i != first)
      text += (i + 1 == names.size()) ? last_separator : ", ";
    text += names[i];
  }
  return text;
}

[[nodiscard]] auto make(Rule::Kind kind, std::vector<std::string> keywords,
                        std::size_t count = 0) -> Rule {
  Rule rule;
  rule.kind = kind;
  rule.keywords = std::move(keywords);
  rule.count = count;
  return rule;
}
} // namespace

auto Condition::describe() const -> std::string {
  switch (kind) {
  case Kind::Always: return {};
  case Kind::Present: return "when " + keyword + " is specified";
  case Kind::Absent: return "when " + keyword + " is not specified";
  case Kind::Equals:
    return "when " + keyword + " is " + (values.empty() ? "" : values[0]);
  case Kind::OneOf: return "when " + keyword + " is " + join(values, 0, " or ");
  }
  return {};
}

auto Rule::describe() const -> std::string {
  std::string text;
  const std::string all = join(keywords);
  switch (kind) {
  case Kind::Required: text = keywords.at(0) + " is required"; break;
  case Kind::Forbidden: text = keywords.at(0) + " is not allowed"; break;
  case Kind::Exactly:
    text = count == 1 ? "exactly one of " + all
                      : "exactly " + std::to_string(count) + " of " + all;
    break;
  case Kind::AtLeast:
    text = count == 1 ? "at least one of " + all
                      : "at least " + std::to_string(count) + " of " + all;
    break;
  case Kind::AtMost:
    text = count == 1 ? "at most one of " + all
                      : "at most " + std::to_string(count) + " of " + all;
    break;
  case Kind::Requires:
    text = keywords.at(0) + " requires " + join(keywords, 1, " and ");
    break;
  case Kind::RequiresAny:
    text = keywords.at(0) + " requires " + join(keywords, 1, " or ");
    break;
  case Kind::Conflicts:
    text = keywords.at(0) + " cannot be used with " + join(keywords, 1, " or ");
    break;
  }
  const std::string condition = when.describe();
  return condition.empty() ? text : text + " " + condition;
}

namespace rules {
auto Required(std::string keyword) -> Rule {
  return make(Rule::Kind::Required, {std::move(keyword)});
}
auto Forbidden(std::string keyword) -> Rule {
  return make(Rule::Kind::Forbidden, {std::move(keyword)});
}
auto Exactly(const std::size_t n, std::vector<std::string> keywords) -> Rule {
  return make(Rule::Kind::Exactly, std::move(keywords), n);
}
auto AtLeast(const std::size_t n, std::vector<std::string> keywords) -> Rule {
  return make(Rule::Kind::AtLeast, std::move(keywords), n);
}
auto AtMost(const std::size_t n, std::vector<std::string> keywords) -> Rule {
  return make(Rule::Kind::AtMost, std::move(keywords), n);
}
auto ExactlyOneOf(std::vector<std::string> keywords) -> Rule {
  return Exactly(1, std::move(keywords));
}
auto AtLeastOneOf(std::vector<std::string> keywords) -> Rule {
  return AtLeast(1, std::move(keywords));
}
auto AtMostOneOf(std::vector<std::string> keywords) -> Rule {
  return AtMost(1, std::move(keywords));
}
auto Requires(std::string keyword, std::vector<std::string> required) -> Rule {
  required.insert(required.begin(), std::move(keyword));
  return make(Rule::Kind::Requires, std::move(required));
}
auto RequiresAnyOf(std::string keyword, std::vector<std::string> candidates)
    -> Rule {
  candidates.insert(candidates.begin(), std::move(keyword));
  return make(Rule::Kind::RequiresAny, std::move(candidates));
}
auto Conflicts(std::string keyword, std::vector<std::string> others) -> Rule {
  others.insert(others.begin(), std::move(keyword));
  return make(Rule::Kind::Conflicts, std::move(others));
}
auto Present(std::string keyword) -> Condition {
  return Condition{Condition::Kind::Present, std::move(keyword), {}};
}
auto Absent(std::string keyword) -> Condition {
  return Condition{Condition::Kind::Absent, std::move(keyword), {}};
}
auto Equals(std::string keyword, std::string value) -> Condition {
  return Condition{Condition::Kind::Equals, std::move(keyword),
                   {std::move(value)}};
}
auto OneOf(std::string keyword, std::vector<std::string> values) -> Condition {
  return Condition{Condition::Kind::OneOf, std::move(keyword),
                   std::move(values)};
}
auto When(Condition condition, Rule rule) -> Rule {
  rule.when = std::move(condition);
  return rule;
}
} // namespace rules
} // namespace OpenSMOKEpp
