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
#include <iosfwd>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "DictionaryKeyWord.h"

namespace OpenSMOKEpp {
/**
 * \brief Defines valid keywords and consistency rules for a dictionary.
 *
 * A grammar owns a set of `DictionaryKeyWord` rules. When the grammar is
 * built (`DefineRules()` or `ReadFromFile`) its own coherence is verified;
 * when it is attached to a dictionary, the dictionary is validated for
 * undefined keywords, compulsory keywords, required keyword dependencies and
 * conflicting keywords. All problems found are reported together in one
 * exception.
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
   * \note Each keyword block is expected to contain seven lines.
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
   * \return Multi-line table with keyword names, types, and comments.
   */
  [[nodiscard]] std::string
  AvailableKeyWordsTable(std::string_view dictionary_name = {}) const;

  /**
   * \brief Finds the rule of a keyword.
   *
   * \param[in] keyword_name Keyword name including leading `@`.
   * \return Pointer to the rule, or `nullptr` if it is not declared.
   */
  [[nodiscard]] const DictionaryKeyWord *
  FindKeyWord(std::string_view keyword_name) const noexcept;

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
  [[nodiscard]] std::vector<std::string>
  CollectUndefinedKeyWords(const std::vector<std::string> &list_of_keywords) const;

  /**
   * \brief Lists violations of compulsory keyword rules.
   *
   * \return One message per missing compulsory keyword and per alternative
   * group with zero or more than one member present.
   */
  [[nodiscard]] std::vector<std::string>
  CollectCompulsoryErrors(const std::vector<std::string> &list_of_keywords) const;

  /**
   * \brief Lists missing required companion keywords.
   */
  [[nodiscard]] std::vector<std::string>
  CollectRequiredErrors(const std::vector<std::string> &list_of_keywords) const;

  /**
   * \brief Lists pairs of mutually exclusive keywords used together.
   */
  [[nodiscard]] std::vector<std::string>
  CollectConflictErrors(const std::vector<std::string> &list_of_keywords) const;

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
   * \warning Always throws `std::runtime_error`.
   */
  [[noreturn]] void ErrorMessage(std::string_view message) const;
};
} // namespace OpenSMOKEpp

#endif /* OpenSMOKEpp_DictionaryGrammar_H */
