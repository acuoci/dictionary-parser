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

namespace OpenSMOKEpp::Lexer
{
	struct PreprocessedFile
	{
		std::filesystem::path file_name;
		std::vector<int> indices_of_good_lines;
		std::vector<int> indices_of_blank_lines;
		std::vector<std::string> good_lines;
		std::vector<std::string> blank_lines;
		std::vector<std::string> clean_lines;
	};

	[[nodiscard]] auto trim(std::string_view text) noexcept -> std::string_view;
	[[nodiscard]] auto trim_copy(std::string_view text) -> std::string;
	[[nodiscard]] auto strip_comment(std::string_view text) noexcept -> std::string_view;
	[[nodiscard]] auto is_blank(std::string_view text) noexcept -> bool;
	[[nodiscard]] auto split_whitespace(std::string_view text) -> std::vector<std::string_view>;
	[[nodiscard]] auto count_char(std::string_view text, char value) noexcept -> std::size_t;
	[[nodiscard]] auto count_substring(std::string_view text, std::string_view needle) noexcept -> std::size_t;
	[[nodiscard]] auto count_substring_case_insensitive(std::string_view text, std::string_view needle) noexcept -> std::size_t;
	[[nodiscard]] auto contains_substring(std::string_view text, std::string_view needle) noexcept -> bool;

	auto replace_tabs_with_spaces(std::string& text) noexcept -> void;
	[[nodiscard]] auto read_and_preprocess_file(const std::filesystem::path& file_name) -> PreprocessedFile;
}

#endif	// OpenSMOKEpp_DictionaryLexer_H
