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
#define	OpenSMOKEpp_DictionaryGrammar_H

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

#include "DictionaryKeyWord.h"

namespace OpenSMOKEpp
{
	/**
	 * \brief Defines valid keywords and consistency rules for a dictionary.
	 *
	 * A grammar owns a set of `DictionaryKeyWord` rules and validates a parsed
	 * dictionary for undefined keywords, compulsory keywords, required keyword
	 * dependencies, conflicting keywords, and requested keyword value types.
	 */
	class DictionaryGrammar {
		friend class Dictionary;

	public:
		/**
		 * \brief Reads grammar keyword definitions from a text file.
		 *
		 * \param[in] file_name Path to the grammar definition file.
		 * \warning Throws `std::runtime_error` if the file cannot be read, if a
		 * keyword block is malformed, or if grammar consistency checks fail.
		 * \note Each keyword block is expected to contain seven lines.
		 */
		void ReadFromFile(const std::string file_name);

		/**
		 * \brief Writes a compact grammar summary.
		 *
		 * \param[out] fout Stream receiving keyword names, types, and comments.
		 */
		void ShortSummary(std::ostream& fout) const;

		/**
		 * \brief Writes the list of available keywords to the standard output.
		 *
		 * \note This function is used for `@Help` diagnostics and undefined
		 * keyword diagnostics.
		 */
		void ShowListAvailableKeywords();

		/**
		 * \brief Returns the grammar name.
		 *
		 * \return Grammar name string.
		 * \note This function is `[[nodiscard]]`.
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

	protected:
		/**
		 * \brief Builds a hard-coded user grammar and validates it.
		 *
		 * \warning Throws `std::runtime_error` if `DefineRules()` fails to add a
		 * consistent set of keyword rules.
		 * \note Called by `Dictionary::SetGrammar(DictionaryGrammar&)`.
		 */
		void UserDefined();

		/**
		 * \brief Checks whether `@Help` is present.
		 *
		 * \param[in] list_of_keywords Parsed keyword names from a dictionary.
		 * \return `true` if `@Help` is present, otherwise `false`.
		 * \note This function is `[[nodiscard]]`.
		 */
		[[nodiscard]] bool CheckForHelpKeyWord(std::vector<std::string>& list_of_keywords);

		/**
		 * \brief Checks for dictionary keywords not defined by this grammar.
		 *
		 * \param[in] list_of_keywords Parsed keyword names from a dictionary.
		 * \return `true` if all keywords are known, otherwise `false`.
		 * \note This function is `[[nodiscard]]`.
		 */
		[[nodiscard]] bool CheckUndefinedKeyWords(std::vector<std::string>& list_of_keywords);

		/**
		 * \brief Checks whether compulsory keyword rules are satisfied.
		 *
		 * \param[in] list_of_keywords Parsed keyword names from a dictionary.
		 * \return `true` if all compulsory requirements are satisfied, otherwise
		 * `false`.
		 * \note Compulsory alternatives are treated as an exclusive set.
		 * \note This function is `[[nodiscard]]`.
		 */
		[[nodiscard]] bool CheckCompulsoryKeyWords(std::vector<std::string>& list_of_keywords);

		/**
		 * \brief Checks required-keyword dependencies.
		 *
		 * \param[in] list_of_keywords Parsed keyword names from a dictionary.
		 * \return `true` if every present keyword has all required companion
		 * keywords, otherwise `false`.
		 * \note This function is `[[nodiscard]]`.
		 */
		[[nodiscard]] bool CheckRequiredKeyWords(std::vector<std::string>& list_of_keywords);

		/**
		 * \brief Checks mutually exclusive keyword rules.
		 *
		 * \param[in] list_of_keywords Parsed keyword names from a dictionary.
		 * \return `true` if no conflicting keyword pair is present, otherwise
		 * `false`.
		 * \note This function is `[[nodiscard]]`.
		 */
		[[nodiscard]] bool CheckConflictingKeyWords(std::vector<std::string>& list_of_keywords);

		/**
		 * \brief Checks whether a keyword has an expected declared value type.
		 *
		 * \param[in] keyword_name Keyword name including leading `@`.
		 * \param[in] expected_type Type required by the caller.
		 * \return `true` if the grammar contains `keyword_name` with
		 * `expected_type`, otherwise `false`.
		 * \note This function is `[[nodiscard]]`.
		 */
		[[nodiscard]] bool CheckType(const std::string keyword_name, const DictionaryKeyWordTypes expected_type);

		/** \brief Number of keyword rules in `keywords_`. */
		unsigned int number_of_keywords_;

		/** \brief Keyword rules defined by this grammar. */
		std::vector<DictionaryKeyWord> keywords_;

		/** \brief Cached keyword names, parallel to `keywords_`. */
		std::vector<std::string> list_of_keywords_names_;

		/**
		 * \brief Validates internal grammar consistency.
		 *
		 * \warning Throws `std::runtime_error` if duplicate keyword names or
		 * inconsistent relationships are detected.
		 */
		void Check();

		/**
		 * \brief Verifies that all referenced related keywords exist.
		 *
		 * \warning Throws `std::runtime_error` if compulsory alternatives,
		 * required keywords, or conflicting keywords refer to undefined names.
		 */
		void CheckExistence();

		/**
		 * \brief Adds one keyword rule to this grammar.
		 *
		 * \param[in] keyword Keyword rule to append.
		 * \note Derived grammar classes call this inside `DefineRules()`.
		 */
		void AddKeyWord(const DictionaryKeyWord& keyword);

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
		void ErrorMessage(const std::string message) const;
	};
}

#endif	/* OpenSMOKEpp_DictionaryGrammar_H */
