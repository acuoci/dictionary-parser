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

#ifndef OpenSMOKEpp_DictionaryRules_H
#define OpenSMOKEpp_DictionaryRules_H

#include <cstddef>
#include <initializer_list>
#include <string>
#include <vector>

namespace OpenSMOKEpp {
/**
 * \brief Condition under which a rule applies.
 *
 * Conditions are evaluated on the dictionary being validated. `Equals` and
 * `OneOf` use the effective value of a single-token keyword (the value in
 * the file, or the grammar default if the keyword is absent); `Present` and
 * `Absent` look only at the file.
 */
struct Condition {
  /** \brief Kind of condition. */
  enum class Kind {
    Always,  ///< The rule always applies.
    Present, ///< The keyword is written in the file.
    Absent,  ///< The keyword is not written in the file.
    Equals,  ///< The keyword's value equals `values[0]`.
    OneOf    ///< The keyword's value is one of `values`.
  };

  /** \brief Kind of condition. */
  Kind kind = Kind::Always;
  /** \brief Keyword tested by the condition. */
  std::string keyword;
  /** \brief Values for `Equals` and `OneOf`. */
  std::vector<std::string> values;

  /** \brief Text such as `when @Type is Isothermal`; empty for `Always`. */
  [[nodiscard]] auto describe() const -> std::string;
};

/**
 * \brief One structural rule on the keywords of a dictionary.
 *
 * Rules are created with the factory functions in `OpenSMOKEpp::rules` and
 * registered with `DictionaryGrammar::AddRule`.
 */
struct Rule {
  /** \brief Kind of rule. */
  enum class Kind {
    Required,    ///< `keywords[0]` must be present.
    Forbidden,   ///< `keywords[0]` must be absent.
    Exactly,     ///< exactly `count` of `keywords` must be present.
    AtLeast,     ///< at least `count` of `keywords` must be present.
    AtMost,      ///< at most `count` of `keywords` may be present.
    Requires,    ///< if `keywords[0]` is present, all the others must be.
    RequiresAny, ///< if `keywords[0]` is present, one of the others must be.
    Conflicts    ///< `keywords[0]` cannot be used with any of the others.
  };

  /** \brief Kind of rule. */
  Kind kind = Kind::Required;
  /** \brief Keywords involved (see `Kind`). */
  std::vector<std::string> keywords;
  /** \brief Count for `Exactly`, `AtLeast`, `AtMost`. */
  std::size_t count = 0;
  /** \brief Condition under which the rule applies. */
  Condition when;

  /** \brief Text such as `exactly 2 of @T, @P, @Rho`. */
  [[nodiscard]] auto describe() const -> std::string;
};

/**
 * \brief Factory functions for rules and conditions.
 *
 * \code
 * AddRule(rules::Exactly(2, {"@Temperature", "@Pressure", "@Density"}));
 * AddRule(rules::ExactlyOneOf({"@MoleFractions", "@MassFractions"}));
 * AddRule(rules::Conflicts("@Lean", {"@Rich"}));
 * AddRule(rules::When(rules::Equals("@Type", "NonIsothermal-UserDefinedVolume"),
 *                     rules::Required("@VolumeProfile")));
 * \endcode
 */
namespace rules {
/** \brief The keyword must be present. */
[[nodiscard]] auto Required(std::string keyword) -> Rule;
/** \brief The keyword must be absent (useful in conditional rules). */
[[nodiscard]] auto Forbidden(std::string keyword) -> Rule;
/** \brief Exactly `n` of the keywords must be present. */
[[nodiscard]] auto Exactly(std::size_t n, std::vector<std::string> keywords)
    -> Rule;
/** \brief At least `n` of the keywords must be present. */
[[nodiscard]] auto AtLeast(std::size_t n, std::vector<std::string> keywords)
    -> Rule;
/** \brief At most `n` of the keywords may be present. */
[[nodiscard]] auto AtMost(std::size_t n, std::vector<std::string> keywords)
    -> Rule;
/** \brief Exactly one of the keywords must be present. */
[[nodiscard]] auto ExactlyOneOf(std::vector<std::string> keywords) -> Rule;
/** \brief At least one of the keywords must be present. */
[[nodiscard]] auto AtLeastOneOf(std::vector<std::string> keywords) -> Rule;
/** \brief At most one of the keywords may be present (mutual exclusion). */
[[nodiscard]] auto AtMostOneOf(std::vector<std::string> keywords) -> Rule;
/** \brief If `keyword` is present, all `required` keywords must be present. */
[[nodiscard]] auto Requires(std::string keyword,
                            std::vector<std::string> required) -> Rule;
/** \brief If `keyword` is present, at least one of `candidates` must be. */
[[nodiscard]] auto RequiresAnyOf(std::string keyword,
                                 std::vector<std::string> candidates) -> Rule;
/** \brief `keyword` cannot be used together with any of `others`. The rule
 * is symmetric: it does not need to be declared on the other keywords. */
[[nodiscard]] auto Conflicts(std::string keyword,
                             std::vector<std::string> others) -> Rule;

/** \brief The keyword is written in the file. */
[[nodiscard]] auto Present(std::string keyword) -> Condition;
/** \brief The keyword is not written in the file. */
[[nodiscard]] auto Absent(std::string keyword) -> Condition;
/** \brief The (effective) value of the keyword equals `value`. */
[[nodiscard]] auto Equals(std::string keyword, std::string value) -> Condition;
/** \brief The (effective) value of the keyword is one of `values`. */
[[nodiscard]] auto OneOf(std::string keyword, std::vector<std::string> values)
    -> Condition;
/** \brief Makes `rule` apply only when `condition` holds. */
[[nodiscard]] auto When(Condition condition, Rule rule) -> Rule;
} // namespace rules
} // namespace OpenSMOKEpp

#endif // OpenSMOKEpp_DictionaryRules_H
