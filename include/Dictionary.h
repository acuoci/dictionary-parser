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

#ifndef OpenSMOKEpp_Dictionary_H
#define	OpenSMOKEpp_Dictionary_H

#include <filesystem>
#include <iosfwd>
#include <iostream>
#include <string>
#include <vector>

#include "DictionaryGrammar.h"

namespace OpenSMOKEpp
{
	/**
	 * \brief Stores and reads one parsed dictionary block.
	 *
	 * A dictionary contains keyword names, raw option strings, source line
	 * ranges, and an optional grammar used to validate keyword availability and
	 * typed reads. Values are read on demand through the `Read*` family.
	 */
	class Dictionary
	{
	public:
		/**
		 * \brief Writes all keyword entries and source line ranges.
		 *
		 * \param[out] fout Stream receiving diagnostic dictionary contents.
		 */
		void Summary(std::ostream& fout) const;

		/**
		 * \brief Sets the dictionary name.
		 *
		 * \param[in] name Name declared after the `Dictionary` token.
		 */
		void SetName(const std::string name) { name_ = name; }

		/**
		 * \brief Sets the source file name for diagnostics.
		 *
		 * \param[in] file_name Path or file name from which the dictionary was
		 * read.
		 */
		void SetFileName(const std::string file_name) { file_name_ = file_name; }

		/**
		 * \brief Sets parsed keyword data and source line ranges.
		 *
		 * \param[in] keywords Keyword names, each including leading `@`.
		 * \param[in] options Raw option strings with trailing semicolons removed.
		 * \param[in] starting_lines 1-based original start line for each keyword.
		 * \param[in] ending_lines 1-based original end line for each keyword.
		 * \warning All vectors must have identical size.
		 */
		void SetDictionary(const std::vector<std::string>& keywords, const std::vector<std::string>& options,
			               const std::vector<unsigned int>& starting_lines, const std::vector<unsigned int>& ending_lines);

		/**
		 * \brief Loads a grammar from file and validates this dictionary.
		 *
		 * \param[in] file_name Path to a grammar file.
		 * \warning Throws `std::runtime_error` if the grammar file or dictionary
		 * validation fails.
		 */
		void SetGrammar(const std::string file_name);

		/**
		 * \brief Applies a hard-coded grammar and validates this dictionary.
		 *
		 * \param[in,out] grammar Grammar object whose `DefineRules()` method is
		 * invoked before validation.
		 * \warning Throws `std::runtime_error` if grammar construction or
		 * dictionary validation fails.
		 */
		void SetGrammar(DictionaryGrammar& grammar);

		/**
		 * \brief Checks whether a keyword exists in this dictionary.
		 *
		 * \param[in] name_of_keyword Keyword name including leading `@`.
		 * \return `true` if the keyword exists, otherwise `false`.
		 * \note This function is `[[nodiscard]]`.
		 */
		[[nodiscard]] bool CheckOption(const std::string name_of_keyword);

		/**
		 * \brief Reads one integer option.
		 *
		 * \param[in] option Keyword name including leading `@`.
		 * \param[out] value Parsed signed integer.
		 * \warning Throws `std::runtime_error` if the keyword is absent, has the
		 * wrong grammar type, has the wrong token count, or cannot be converted.
		 */
		void ReadInt(const std::string option, int& value);

		/**
		 * \brief Reads one floating-point option.
		 *
		 * \param[in] option Keyword name including leading `@`.
		 * \param[out] value Parsed floating-point value.
		 * \warning Throws `std::runtime_error` on absence, type mismatch, invalid
		 * token count, or numeric conversion failure.
		 */
		void ReadDouble(const std::string option, double& value);

		/**
		 * \brief Reads one character option.
		 *
		 * \param[in] option Keyword name including leading `@`.
		 * \param[out] value Parsed character.
		 * \warning The option must contain exactly one one-character token.
		 */
		void ReadChar(const std::string option, char& value);

		/**
		 * \brief Reads one string option.
		 *
		 * \param[in] option Keyword name including leading `@`.
		 * \param[out] value Parsed token.
		 * \warning The option must contain exactly one whitespace-delimited token.
		 */
		void ReadString(const std::string option, std::string& value);

		/**
		 * \brief Reads one boolean option.
		 *
		 * \param[in] option Keyword name including leading `@`.
		 * \param[out] value Parsed boolean.
		 * \warning Accepted true tokens are `true` and `on`; accepted false
		 * tokens are `false` and `off`.
		 */
		void ReadBool(const std::string option, bool& value);

		/**
		 * \brief Reads one path option.
		 *
		 * \param[in] name_of_keyword Keyword name including leading `@`.
		 * \param[out] value Parsed filesystem path.
		 * \note The raw option string is preserved as the path; no existence check
		 * is performed.
		 */
		void ReadPath(const std::string name_of_keyword, std::filesystem::path& value);

		/**
		 * \brief Reads a raw string sequence option.
		 *
		 * \param[in] name_of_keyword Keyword name including leading `@`.
		 * \param[out] value Raw option string after semicolon removal.
		 * \note This preserves internal whitespace-normalized token order.
		 */
		void ReadSequence(const std::string name_of_keyword, std::string& value);

		/**
		 * \brief Reads one measured scalar option.
		 *
		 * \param[in] option Keyword name including leading `@`.
		 * \param[out] value Parsed floating-point magnitude.
		 * \param[out] units Unit token following the magnitude.
		 * \warning The parser validates the two-token shape but does not interpret
		 * or convert units.
		 */
		void ReadMeasure(const std::string option, double& value, std::string& units);

		/**
		 * \brief Reads one dictionary-name option.
		 *
		 * \param[in] name_of_keyword Keyword name including leading `@`.
		 * \param[out] value Parsed dictionary name token.
		 * \warning The target dictionary existence is not checked here.
		 */
		void ReadDictionary(const std::string name_of_keyword, std::string& value);

		/**
		 * \brief Reads a vector of integer tokens.
		 *
		 * \param[in] option Keyword name including leading `@`.
		 * \param[out] value Parsed integer vector.
		 * \warning Throws if the option is empty or any token fails conversion.
		 */
		void ReadOption(const std::string option, std::vector<int>& value);

		/**
		 * \brief Reads a vector of floating-point tokens.
		 *
		 * \param[in] option Keyword name including leading `@`.
		 * \param[out] value Parsed floating-point vector.
		 * \warning Throws if the option is empty or any token fails conversion.
		 */
		void ReadOption(const std::string option, std::vector<double>& value);

		/**
		 * \brief Reads a vector of character tokens.
		 *
		 * \param[in] option Keyword name including leading `@`.
		 * \param[out] value Parsed character vector.
		 * \warning Every token must contain exactly one character.
		 */
		void ReadOption(const std::string option, std::vector<char>& value);

		/**
		 * \brief Reads a vector of string tokens.
		 *
		 * \param[in] option Keyword name including leading `@`.
		 * \param[out] value Parsed string-token vector.
		 * \warning Throws if the option has no tokens.
		 */
		void ReadOption(const std::string option, std::vector<std::string>& value);

		/**
		 * \brief Reads a vector of boolean tokens.
		 *
		 * \param[in] option Keyword selector.
		 * \param[out] value Parsed boolean vector.
		 * \warning This overload is declared for API compatibility; verify
		 * implementation availability before using it.
		 */
		void ReadOption(const bool option, std::vector<bool>& value);

		/**
		 * \brief Reads repeated string/floating-point pairs.
		 *
		 * \param[in] name_of_keyword Keyword name including leading `@`.
		 * \param[out] strings Parsed string elements at even pair positions.
		 * \param[out] values Parsed floating-point elements at odd pair positions.
		 * \warning The option token count must be positive and even.
		 */
		void ReadOption(const std::string name_of_keyword, std::vector<std::string>& strings, std::vector<double>& values);

		/**
		 * \brief Reads repeated integer/string pairs.
		 *
		 * \param[in] name_of_keyword Keyword name including leading `@`.
		 * \param[out] values Parsed integer elements at even pair positions.
		 * \param[out] names Parsed string elements at odd pair positions.
		 * \warning The option token count must be positive and even.
		 */
		void ReadOption(const std::string name_of_keyword, std::vector<int>& values, std::vector<std::string>& names);

		/**
		 * \brief Reads repeated floating-point/string pairs.
		 *
		 * \param[in] name_of_keyword Keyword name including leading `@`.
		 * \param[out] values Parsed floating-point elements at even pair positions.
		 * \param[out] strings Parsed string elements at odd pair positions.
		 * \warning Used for `VECTOR_MEASURE`; unit semantics are left to callers.
		 */
		void ReadOption(const std::string name_of_keyword, std::vector<double>& values, std::vector<std::string>& strings);

		/**
		 * \brief Returns the dictionary name.
		 *
		 * \return Constant reference to the dictionary name.
		 * \note This function is `[[nodiscard]]` and `noexcept`.
		 */
		[[nodiscard]] const std::string& name() const noexcept { return name_; }

	private:
		/** \brief Dictionary name declared in the input file. */
		std::string name_;

		/** \brief Source file path or file name used in diagnostics. */
		std::string file_name_;

		/** \brief Parsed keyword names, each including leading `@`. */
		std::vector<std::string> keywords_;

		/** \brief Raw option strings corresponding to `keywords_`. */
		std::vector<std::string> options_;

		/** \brief 1-based start line for each parsed keyword. */
		std::vector<unsigned int> starting_lines_;

		/** \brief 1-based end line for each parsed keyword. */
		std::vector<unsigned int> ending_lines_;

		/** \brief Grammar used for validation and typed-read checks. */
		DictionaryGrammar grammar_;

	private:
		/**
		 * \brief Validates this dictionary against the current grammar.
		 *
		 * \warning Throws `std::runtime_error` for help requests, undefined
		 * keywords, missing compulsory keywords, missing required keywords, or
		 * conflicting keywords.
		 */
		void Checks();

		/**
		 * \brief Locates a keyword and verifies its grammar type.
		 *
		 * \param[in] name_of_keyword Keyword name including leading `@`.
		 * \param[in] expected_type Expected grammar value type.
		 * \return Zero-based index into `keywords_` and `options_`.
		 * \warning Throws `std::runtime_error` if the keyword is absent or has a
		 * different grammar type.
		 * \note This function is `[[nodiscard]]`.
		 */
		[[nodiscard]] int CheckOption(const std::string name_of_keyword, const DictionaryKeyWordTypes expected_type);

		/**
		 * \brief Raises a dictionary-scoped error.
		 *
		 * \param[in] message Diagnostic message without dictionary context.
		 * \warning Always throws `std::runtime_error`.
		 */
		void ErrorMessage(const std::string message) const;
	};
}

#endif // OpenSMOKEpp_Dictionary_H
