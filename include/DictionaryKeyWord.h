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

#ifndef OpenSMOKEpp_DictionaryKeyWord_H
#define	OpenSMOKEpp_DictionaryKeyWord_H

#include <iosfwd>
#include <string>
#include <vector>

namespace OpenSMOKEpp
{
	/**
	 * \brief Enumerates supported dictionary keyword value types.
	 *
	 * The type controls which `Dictionary::Read*` overload can legally read a
	 * keyword after grammar validation.
	 */
	enum DictionaryKeyWordTypes {
		/** \brief No value type. */
		NONE,
		/** \brief One signed integer token. */
		SINGLE_INT,
		/** \brief One floating-point token. */
		SINGLE_DOUBLE,
		/** \brief One string token. */
		SINGLE_STRING,
		/** \brief One single-character token. */
		SINGLE_CHAR,
		/** \brief One boolean token: `true`, `false`, `on`, or `off`. */
		SINGLE_BOOL,
		/** \brief One floating-point token followed by one unit token. */
		SINGLE_MEASURE,
		/** \brief One path value stored as `std::filesystem::path`. */
		SINGLE_PATH,
		/** \brief One dictionary-name token. */
		SINGLE_DICTIONARY,
		/** \brief Sequence of signed integer tokens. */
		VECTOR_INT,
		/** \brief Sequence of floating-point tokens. */
		VECTOR_DOUBLE,
		/** \brief Sequence of string tokens. */
		VECTOR_STRING,
		/** \brief Sequence of single-character tokens. */
		VECTOR_CHAR,
		/** \brief Sequence of boolean tokens. */
		VECTOR_BOOL,
		/** \brief Sequence of repeated floating-point/unit token pairs. */
		VECTOR_MEASURE,
		/** \brief Sequence of repeated string/floating-point token pairs. */
		VECTOR_STRING_DOUBLE,
		/** \brief Sequence of repeated integer/string token pairs. */
		VECTOR_INT_STRING,
		/** \brief Raw option string preserving all tokens after the keyword. */
		SEQUENCE_STRING
	};

	/**
	 * \brief Defines one legal keyword in a dictionary grammar.
	 *
	 * A keyword rule stores its name, value type, short diagnostic comment,
	 * compulsory status, compulsory alternatives, required keywords, and
	 * mutually conflicting keywords.
	 */
	class DictionaryKeyWord {
	public:
		/**
		 * \brief Constructs an empty keyword rule.
		 *
		 * \warning The object is not semantically valid until initialized by a
		 * constructor that sets name and type or by grammar-file parsing.
		 */
		DictionaryKeyWord() {};

		/**
		 * \brief Constructs a keyword rule without dependency relationships.
		 *
		 * \param[in] name Keyword name, including the leading `@`.
		 * \param[in] type Expected value type.
		 * \param[in] short_comment Human-readable short description.
		 * \param[in] is_compulsory `true` if this keyword must appear unless a
		 * compulsory alternative satisfies the rule.
		 */
		DictionaryKeyWord(const std::string& name, const DictionaryKeyWordTypes type,
									const std::string& short_comment, const bool is_compulsory);

		/**
		 * \brief Constructs a keyword rule with dependency relationships.
		 *
		 * \param[in] name Keyword name, including the leading `@`.
		 * \param[in] type Expected value type.
		 * \param[in] short_comment Human-readable short description.
		 * \param[in] is_compulsory `true` if the keyword is compulsory.
		 * \param[in] compulsory_alternatives Space-delimited alternatives, or
		 * `none`.
		 * \param[in] required_keywords Space-delimited keywords required when
		 * this keyword is present, or `none`.
		 * \param[in] conflicting_keywords Space-delimited keywords mutually
		 * exclusive with this keyword, or `none`.
		 */
		DictionaryKeyWord(const std::string& name, const DictionaryKeyWordTypes type,
									const std::string& short_comment, const bool is_compulsory,
									const std::string& compulsory_alternatives, const std::string& required_keywords,
									const std::string& conflicting_keywords);

		/**
		 * \brief Constructs a keyword rule from seven grammar-file lines.
		 *
		 * \param[in,out] lines Grammar block lines. Lines are trimmed in place.
		 * \warning Throws `std::runtime_error` if the block does not contain
		 * exactly seven lines or if any line has invalid grammar-keyword syntax.
		 */
		DictionaryKeyWord(std::vector<std::string>& lines);

		/**
		 * \brief Destroys the keyword rule.
		 */
		virtual ~DictionaryKeyWord() = default;

		/**
		 * \brief Writes a compact keyword summary.
		 *
		 * \param[out] fout Stream receiving name, type, and short comment.
		 */
		void ShortSummary(std::ostream& fout) const;

		/**
		 * \brief Returns the keyword name.
		 *
		 * \return Constant reference to the keyword name, including leading `@`.
		 * \note This function is `[[nodiscard]]` and `noexcept`.
		 */
		[[nodiscard]] const std::string& name() const noexcept { return name_; }

		/**
		 * \brief Returns the expected keyword value type.
		 *
		 * \return Value from `DictionaryKeyWordTypes`.
		 * \note This function is `[[nodiscard]]` and `noexcept`.
		 */
		[[nodiscard]] DictionaryKeyWordTypes type() const noexcept { return type_; }

		/**
		 * \brief Returns the expected value type as an ASCII token.
		 *
		 * \return Constant reference to the type token used in grammar files.
		 * \note This function is `[[nodiscard]]` and `noexcept`.
		 */
		[[nodiscard]] const std::string& type_ascii() const noexcept { return type_ascii_; }

		/**
		 * \brief Returns the short diagnostic comment.
		 *
		 * \return Constant reference to the stored comment.
		 * \note This function is `[[nodiscard]]` and `noexcept`.
		 */
		[[nodiscard]] const std::string& comment_short() const noexcept { return comment_short_; }

		/**
		 * \brief Tests whether the keyword is compulsory.
		 *
		 * \return `true` if the keyword must be present unless satisfied by a
		 * compulsory alternative.
		 * \note This function is `[[nodiscard]]` and `noexcept`.
		 */
		[[nodiscard]] bool is_compulsory() const noexcept { return is_compulsory_; }

		/**
		 * \brief Returns compulsory alternatives for this keyword.
		 *
		 * \return Constant reference to keyword names that can replace this
		 * compulsory keyword.
		 * \note This function is `[[nodiscard]]` and `noexcept`.
		 */
		[[nodiscard]] const std::vector<std::string>& compulsory_alternatives() const noexcept { return compulsory_alternatives_; }

		/**
		 * \brief Returns keywords required when this keyword is present.
		 *
		 * \return Constant reference to required keyword names.
		 * \note This function is `[[nodiscard]]` and `noexcept`.
		 */
		[[nodiscard]] const std::vector<std::string>& required_keywords() const noexcept { return required_keywords_; }

		/**
		 * \brief Returns keywords that conflict with this keyword.
		 *
		 * \return Constant reference to mutually exclusive keyword names.
		 * \note This function is `[[nodiscard]]` and `noexcept`.
		 */
		[[nodiscard]] const std::vector<std::string>& conflicting_keywords() const noexcept { return conflicting_keywords_; }

	private:
		/** \brief Keyword name, including leading `@`. */
		std::string name_;

		/** \brief ASCII representation of `type_`. */
		std::string type_ascii_;

		/** \brief Expected value type. */
		DictionaryKeyWordTypes type_;

		/** \brief Short diagnostic comment. */
		std::string comment_short_;

		/** \brief Whether the keyword is compulsory. */
		bool is_compulsory_;

		/** \brief Alternative keywords satisfying the same compulsory rule. */
		std::vector<std::string> compulsory_alternatives_;

		/** \brief Keywords required when this keyword appears. */
		std::vector<std::string> required_keywords_;

		/** \brief Keywords mutually exclusive with this keyword. */
		std::vector<std::string> conflicting_keywords_;

	private:
		/**
		 * \brief Parses and stores the `keyword:` grammar line.
		 *
		 * \param[in] line Grammar line with exactly `keyword: <name>`.
		 * \warning Throws `std::runtime_error` on invalid syntax.
		 */
		void SetKeyword(const std::string& line);

		/**
		 * \brief Parses and stores the `type:` grammar line.
		 *
		 * \param[in] line Grammar line with exactly `type: <type-token>`.
		 * \warning Throws `std::runtime_error` on invalid or unsupported type.
		 */
		void SetType(const std::string& line);

		/**
		 * \brief Synchronizes the ASCII type token with `type_`.
		 *
		 * \warning Throws `std::runtime_error` if `type_` is not supported.
		 */
		void SetTypeASCII();

		/**
		 * \brief Parses and stores the `short_comment:` grammar line.
		 *
		 * \param[in] line Grammar line whose remaining tokens form the comment.
		 * \warning Throws `std::runtime_error` if the line does not start with
		 * `short_comment:`.
		 */
		void SetComment(const std::string& line);

		/**
		 * \brief Parses and stores the `is_compulsory:` grammar line.
		 *
		 * \param[in] line Grammar line with value `true` or `false`.
		 * \warning Throws `std::runtime_error` on invalid syntax or value.
		 */
		void SetIsCompulsory(const std::string& line);

		/**
		 * \brief Parses compulsory alternatives from a grammar line.
		 *
		 * \param[in] line Grammar line beginning with `compulsory_alternatives:`.
		 * \warning Throws `std::runtime_error` if a noncompulsory keyword
		 * declares alternatives other than `none`.
		 */
		void SetCompulsoryAlternatives(const std::string& line);

		/**
		 * \brief Parses required keywords from a grammar line.
		 *
		 * \param[in] line Grammar line beginning with `required_keywords:`.
		 * \warning Throws `std::runtime_error` on invalid syntax.
		 */
		void SetRequiredKeyWords(const std::string& line);

		/**
		 * \brief Parses conflicting keywords from a grammar line.
		 *
		 * \param[in] line Grammar line beginning with `conflicting_keywords:`.
		 * \warning Throws `std::runtime_error` on invalid syntax.
		 */
		void SetConflictingKeywords(const std::string& line);

		/**
		 * \brief Raises a keyword-rule construction error.
		 *
		 * \param[in] message Diagnostic message without keyword prefix.
		 * \warning Always throws `std::runtime_error`.
		 */
		void ErrorMessage(const std::string message) const;
	};
}

#endif	/* OpenSMOKEpp_DictionaryKeyWord_H */
