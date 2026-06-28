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

#ifndef OpenSMOKEpp_DictionaryLexer_H
#define	OpenSMOKEpp_DictionaryLexer_H

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

/**
 * \brief Low-level lexical utilities for dictionary input files.
 *
 * The namespace contains comment stripping, whitespace normalization, token
 * splitting, and source-line bookkeeping used by higher-level dictionary
 * parsers. Functions do not interpret keyword types or grammar rules.
 */
namespace OpenSMOKEpp::Lexer
{
	/**
	 * \brief Stores preprocessed file lines and their original line indices.
	 *
	 * `clean_lines` contains every physical line after comment stripping and
	 * tab normalization. `good_lines` contains nonblank clean lines.
	 * Line indices are 1-based and refer to the original input file.
	 */
	struct PreprocessedFile
	{
		/** \brief Full input file path used to read the data. */
		std::filesystem::path file_name;

		/** \brief 1-based original line numbers for entries in `good_lines`. */
		std::vector<int> indices_of_good_lines;

		/** \brief 1-based original line numbers for entries in `blank_lines`. */
		std::vector<int> indices_of_blank_lines;

		/** \brief Nonblank lines after comment stripping and tab normalization. */
		std::vector<std::string> good_lines;

		/** \brief Blank or comment-only lines after preprocessing. */
		std::vector<std::string> blank_lines;

		/** \brief All lines after preprocessing, preserving physical file order. */
		std::vector<std::string> clean_lines;
	};

	/**
	 * \brief Returns a view with leading and trailing ASCII whitespace removed.
	 *
	 * \param[in] text Input character sequence.
	 * \return A `std::string_view` into `text`, never owning storage.
	 * \note This function is `[[nodiscard]]` and `noexcept`.
	 */
	[[nodiscard]] auto trim(std::string_view text) noexcept -> std::string_view;

	/**
	 * \brief Returns a trimmed copy of a character sequence.
	 *
	 * \param[in] text Input character sequence.
	 * \return A new `std::string` containing `text` without leading or trailing
	 * ASCII whitespace.
	 * \note This function is `[[nodiscard]]`.
	 */
	[[nodiscard]] auto trim_copy(std::string_view text) -> std::string;

	/**
	 * \brief Removes a dictionary line comment from a character sequence.
	 *
	 * \param[in] text Input line.
	 * \return A view ending before the first `//` marker, or the full input view
	 * if no marker exists.
	 * \warning The dictionary syntax has no string-literal escape rule; `//`
	 * always starts a comment.
	 * \note This function is `[[nodiscard]]` and `noexcept`.
	 */
	[[nodiscard]] auto strip_comment(std::string_view text) noexcept -> std::string_view;

	/**
	 * \brief Tests whether a character sequence is blank after trimming.
	 *
	 * \param[in] text Input character sequence.
	 * \return `true` if `text` contains only ASCII whitespace, otherwise `false`.
	 * \note This function is `[[nodiscard]]` and `noexcept`.
	 */
	[[nodiscard]] auto is_blank(std::string_view text) noexcept -> bool;

	/**
	 * \brief Splits a character sequence into whitespace-delimited token views.
	 *
	 * \param[in] text Input character sequence.
	 * \return Token views pointing into `text`.
	 * \warning The returned views are invalidated when `text` storage is
	 * destroyed or modified.
	 * \note This function is `[[nodiscard]]`.
	 */
	[[nodiscard]] auto split_whitespace(std::string_view text) -> std::vector<std::string_view>;

	/**
	 * \brief Counts occurrences of a character.
	 *
	 * \param[in] text Input character sequence.
	 * \param[in] value Character to count.
	 * \return Number of exact character matches.
	 * \note This function is `[[nodiscard]]` and `noexcept`.
	 */
	[[nodiscard]] auto count_char(std::string_view text, char value) noexcept -> std::size_t;

	/**
	 * \brief Counts non-overlapping occurrences of a substring.
	 *
	 * \param[in] text Input character sequence.
	 * \param[in] needle Substring to count.
	 * \return Number of non-overlapping exact substring matches.
	 * \note An empty `needle` returns 0.
	 * \note This function is `[[nodiscard]]` and `noexcept`.
	 */
	[[nodiscard]] auto count_substring(std::string_view text, std::string_view needle) noexcept -> std::size_t;

	/**
	 * \brief Counts case-insensitive ASCII occurrences of a substring.
	 *
	 * \param[in] text Input character sequence.
	 * \param[in] needle Substring to count using ASCII case folding.
	 * \return Number of case-insensitive substring matches.
	 * \note This function is intended for dictionary grammar keywords, which are
	 * ASCII by specification.
	 * \note This function is `[[nodiscard]]` and `noexcept`.
	 */
	[[nodiscard]] auto count_substring_case_insensitive(std::string_view text, std::string_view needle) noexcept -> std::size_t;

	/**
	 * \brief Tests whether a substring occurs in a character sequence.
	 *
	 * \param[in] text Input character sequence.
	 * \param[in] needle Substring to search.
	 * \return `true` if `needle` occurs in `text`, otherwise `false`.
	 * \note This function is `[[nodiscard]]` and `noexcept`.
	 */
	[[nodiscard]] auto contains_substring(std::string_view text, std::string_view needle) noexcept -> bool;

	/**
	 * \brief Replaces tab characters with single spaces in place.
	 *
	 * \param[in,out] text Mutable string to normalize.
	 * \note This function is `noexcept`.
	 */
	auto replace_tabs_with_spaces(std::string& text) noexcept -> void;

	/**
	 * \brief Reads an input file and applies dictionary lexical preprocessing.
	 *
	 * \param[in] file_name Path to the dictionary input file.
	 * \return Preprocessed file lines and line-index metadata.
	 * \warning Throws `std::runtime_error` if the file cannot be opened or an
	 * I/O error occurs while reading.
	 * \note This function strips `//` comments and replaces tabs with spaces,
	 * but does not parse dictionary blocks or keyword values.
	 * \note This function is `[[nodiscard]]`.
	 */
	[[nodiscard]] auto read_and_preprocess_file(const std::filesystem::path& file_name) -> PreprocessedFile;
}

#endif	// OpenSMOKEpp_DictionaryLexer_H
