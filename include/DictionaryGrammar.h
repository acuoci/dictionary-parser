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

#ifndef OpenSMOKEpp_DictionaryGrammar_H
#define OpenSMOKEpp_DictionaryGrammar_H

#include <filesystem>
#include <functional>
#include <optional>
#include <iosfwd>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "DictionaryDiagnostics.h"
#include "DictionaryKeyWord.h"
#include "DictionaryRules.h"

namespace OpenSMOKEpp {
/**
 * \brief Defines valid keywords and consistency rules for a dictionary.
 *
 * A grammar owns a set of `DictionaryKeyWord` declarations (type, units,
 * default, constraints) and a set of structural `Rule`s (registered with
 * `AddRule` or derived from the legacy per-keyword lists). When the grammar
 * is built (`DefineRules()` or `ReadFromFile`) its own coherence is verified;
 * when it is attached to a dictionary, the dictionary is validated for
 * undefined keywords, rule violations and invalid values. All problems found
 * are reported together in one `DictionaryError`.
 *
 * Grammar coherence checks:
 * - keyword names start with `@`, are unique, and have a valid type;
 * - every referenced keyword (alternatives, required, conflicting) is
 *   declared, is not the keyword itself, and is listed once;
 * - no keyword both requires and conflicts with the same keyword;
 * - compulsory alternatives form closed groups: every member is compulsory
 *   and lists exactly the other members;
 * - conflicts are declared symmetrically;
 * - compulsory keywords without alternatives do not conflict with each other;
 * - no keyword is unusable ("dead"): following `required_keywords`
 *   transitively must never force two conflicting keywords, a keyword that
 *   conflicts with an always-present one, or two members of the same
 *   alternative group;
 * - the grammar is satisfiable: no always-present keyword is dead and every
 *   alternative group has at least one usable member.
 */
class DictionaryGrammar {
  friend class Dictionary;

public:
  /**
   * \brief Reads grammar keyword definitions from a text file.
   *
   * \param[in] file_name Path to the grammar definition file.
   * \warning Throws `std::runtime_error` if the file cannot be read, if a
   * keyword block is malformed, or if grammar coherence checks fail.
   * \note Each keyword block is expected to contain seven lines, optionally
   * followed by an eighth line `dimension: <SI unit> [difference]` for
   * measure keywords (e.g. `dimension: m/s`).
   */
  void ReadFromFile(std::string_view file_name);

  /**
   * \brief Writes a compact grammar summary.
   *
   * \param[out] fout Stream receiving keyword names, types, and comments.
   */
  void ShortSummary(std::ostream &fout) const;

  /**
   * \brief Writes the table of available keywords.
   *
   * \param[out] out Destination stream (standard output by default).
   */
  void ShowListAvailableKeywords(std::ostream &out = std::cout) const;

  /**
   * \brief Returns the table of available keywords as text.
   *
   * \param[in] dictionary_name Name shown in the title; the grammar name is
   * used when empty.
   * \param[in] system Unit system used to show the units of measure
   * keywords.
   * \return Multi-line table with keyword names, types, and comments.
   */
  [[nodiscard]] std::string
  AvailableKeyWordsTable(std::string_view dictionary_name = {},
                         const units::UnitSystem &system = units::si_kmol) const;

  /**
   * \brief Finds the rule of a keyword.
   *
   * \param[in] keyword_name Keyword name including leading `@`.
   * \return Pointer to the rule, or `nullptr` if it is not declared.
   */
  [[nodiscard]] const DictionaryKeyWord *
  FindKeyWord(std::string_view keyword_name) const noexcept;

  /**
   * \brief Returns every rule of the grammar.
   *
   * Includes the rules registered with `AddRule` and those derived from
   * the legacy keyword declarations (compulsory keywords, compulsory
   * alternatives, required and conflicting keywords).
   */
  [[nodiscard]] const std::vector<Rule> &rules() const noexcept {
    return rules_;
  }

  /**
   * \brief Returns the keyword rules.
   */
  [[nodiscard]] const std::vector<DictionaryKeyWord> &keywords() const noexcept {
    return keywords_;
  }

  /**
   * \brief Documents the grammar in Markdown: a table of keywords (type,
   * units, default, constraints, description) followed by the rules.
   *
   * \param[in] title Heading; the grammar name is used when empty.
   * \return Markdown text.
   */
  [[nodiscard]] std::string Describe(std::string_view title = {}) const;

  /**
   * \brief Returns the grammar name used in diagnostics.
   *
   * \return Grammar name (the name of the dictionary it was attached to, if
   * any).
   */
  [[nodiscard]] std::string name() const { return name_; }

  /**
   * \brief Destroys the grammar object.
   */
  virtual ~DictionaryGrammar() = default;

private:
  /** \brief Grammar name used in diagnostic output. */
  std::string name_;

  /** \brief Source file path when the grammar was loaded from file. */
  std::filesystem::path file_name_;

  /**
   * \brief One violation of a grammar rule by a dictionary.
   *
   * The dictionary turns it into a located `Diagnostic`.
   */
  struct Violation {
    /** \brief Category. */
    DiagnosticCode code = DiagnosticCode::Other;
    /** \brief Description. */
    std::string message;
    /** \brief Keyword to point at; empty for the dictionary as a whole. */
    std::string keyword;
    /** \brief Optional suggestion. */
    std::string hint;
    /** \brief Other keywords involved, each with a note. */
    std::vector<std::pair<std::string, std::string>> related;
  };

  /**
   * \brief Returns `true` if `@Help` is present among the keywords.
   */
  [[nodiscard]] bool
  HasHelpKeyWord(const std::vector<std::string> &list_of_keywords) const;

  /**
   * \brief Lists dictionary keywords not declared by this grammar.
   *
   * \return One message per undefined keyword, with a suggestion when a
   * declared keyword has a similar spelling.
   */
  [[nodiscard]] std::vector<Violation>
  CollectUndefinedKeyWords(const std::vector<std::string> &list_of_keywords) const;

  /** \brief Rules registered with `AddRule`. */
  std::vector<Rule> user_rules_;

  /** \brief All rules (derived from legacy declarations + `user_rules_`). */
  std::vector<Rule> rules_;

  /** \brief Function returning the effective single value of a keyword. */
  using ValueLookup =
      std::function<std::optional<std::string>(std::string_view)>;

  /**
   * \brief Evaluates every rule on a dictionary.
   *
   * \param[in] list_of_keywords Keywords written in the file, in order.
   * \param[in] value_of Effective value of a single-token keyword (file
   * value or default), used by conditions.
   * \return One violation per broken rule.
   */
  [[nodiscard]] std::vector<Violation>
  CollectRuleViolations(const std::vector<std::string> &list_of_keywords,
                        const ValueLookup &value_of) const;

  /** \brief Translates legacy declarations into rules (into `rules_`). */
  void BuildRules();

  /** \brief Common final step of `UserDefined` and `ReadFromFile`. */
  void Finalize();

  /** \brief Parses a `rule:` line of a grammar file. */
  [[nodiscard]] auto ParseRuleLine(const std::string &line) const -> Rule;

protected:
  /**
   * \brief Builds a hard-coded user grammar and validates it.
   *
   * \warning Throws `std::runtime_error` if `DefineRules()` produces an
   * incoherent set of keyword rules.
   * \note Called by `Dictionary::SetGrammar(DictionaryGrammar&)`.
   */
  void UserDefined();

  /** \brief Number of keyword rules in `keywords_`. */
  unsigned int number_of_keywords_ = 0;

  /** \brief Keyword rules defined by this grammar. */
  std::vector<DictionaryKeyWord> keywords_;

  /** \brief Cached keyword names, parallel to `keywords_`. */
  std::vector<std::string> list_of_keywords_names_;

  /**
   * \brief Validates internal grammar coherence.
   *
   * \warning Throws `std::runtime_error` listing every problem found.
   */
  void Check();

  /**
   * \brief Adds one keyword rule to this grammar.
   *
   * \param[in] keyword Keyword rule to append.
   * \note Derived grammar classes call this inside `DefineRules()`.
   */
  void AddKeyWord(const DictionaryKeyWord &keyword);

  /**
   * \brief Adds a structural rule (see `OpenSMOKEpp::rules`).
   *
   * \code
   * AddRule(rules::Exactly(2, {"@Temperature", "@Pressure", "@Density"}));
   * AddRule(rules::When(rules::Equals("@Type", "UserDefinedVolume"),
   *                     rules::Required("@VolumeProfile")));
   * \endcode
   *
   * \param[in] rule Rule to add.
   * \note Derived grammar classes call this inside `DefineRules()`.
   */
  void AddRule(Rule rule);

  /**
   * \brief Defines hard-coded keyword rules in derived grammar classes.
   *
   * \warning The base implementation throws `std::runtime_error`; derived
   * grammar classes must override this function when using
   * `Dictionary::SetGrammar(DictionaryGrammar&)`.
   */
  virtual void DefineRules();

  /**
   * \brief Raises a grammar-scoped error.
   *
   * \param[in] message Diagnostic message without grammar prefix.
   * \warning Always throws `DictionaryError` (code `GrammarIncoherent`).
   */
  [[noreturn]] void ErrorMessage(std::string_view message) const;

  /** \brief Context line used in exceptions, e.g. `Grammar of dictionary x`. */
  [[nodiscard]] auto Context() const -> std::string;
};
} // namespace OpenSMOKEpp

#endif /* OpenSMOKEpp_DictionaryGrammar_H */
