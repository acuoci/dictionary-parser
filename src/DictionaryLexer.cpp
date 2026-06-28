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

#include "DictionaryLexer.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace OpenSMOKEpp::Lexer {
namespace {
[[nodiscard]] auto is_space(const char value) noexcept -> bool {
  return value == ' ' || value == '\t' || value == '\n' || value == '\r' ||
         value == '\f' || value == '\v';
}

[[nodiscard]] auto ascii_lower(const char value) noexcept -> char {
  if (value >= 'A' && value <= 'Z')
    return static_cast<char>(value - 'A' + 'a');
  return value;
}

[[nodiscard]] auto ascii_equal_case_insensitive(const char lhs,
                                                const char rhs) noexcept
    -> bool {
  return ascii_lower(lhs) == ascii_lower(rhs);
}
} // namespace

auto trim(const std::string_view text) noexcept -> std::string_view {
  std::size_t first = 0;
  while (first < text.size() && is_space(text[first]))
    ++first;

  std::size_t last = text.size();
  while (last > first && is_space(text[last - 1]))
    --last;

  return text.substr(first, last - first);
}

auto trim_copy(const std::string_view text) -> std::string {
  return std::string(trim(text));
}

auto strip_comment(const std::string_view text) noexcept -> std::string_view {
  const std::size_t marker = text.find("//");
  if (marker == std::string_view::npos)
    return text;
  return text.substr(0, marker);
}

auto is_blank(const std::string_view text) noexcept -> bool {
  return trim(text).empty();
}

auto split_whitespace_view(const std::string_view text)
    -> std::vector<std::string_view> {
  std::size_t token_count = 0;
  bool in_token = false;
  for (const char value : text) {
    const bool space = is_space(value);
    if (!space && !in_token)
      ++token_count;
    in_token = !space;
  }

  std::vector<std::string_view> tokens;
  tokens.reserve(token_count);
  std::size_t cursor = 0;

  while (cursor < text.size()) {
    while (cursor < text.size() && is_space(text[cursor]))
      ++cursor;

    const std::size_t token_begin = cursor;
    while (cursor < text.size() && !is_space(text[cursor]))
      ++cursor;

    if (token_begin != cursor)
      tokens.emplace_back(text.substr(token_begin, cursor - token_begin));
  }

  return tokens;
}

auto split_whitespace_copy(const std::string_view text)
    -> std::vector<std::string> {
  const auto views = split_whitespace_view(text);
  std::vector<std::string> tokens;
  tokens.reserve(views.size());
  std::transform(
      views.begin(), views.end(), std::back_inserter(tokens),
      [](const std::string_view token) { return std::string(token); });
  return tokens;
}

auto split_whitespace(const std::string_view text)
    -> std::vector<std::string_view> {
  return split_whitespace_view(text);
}

auto count_char(const std::string_view text, const char value) noexcept
    -> std::size_t {
  return static_cast<std::size_t>(std::count(text.begin(), text.end(), value));
}

auto count_substring(const std::string_view text,
                     const std::string_view needle) noexcept -> std::size_t {
  if (needle.empty())
    return 0;

  std::size_t count = 0;
  std::size_t cursor = text.find(needle);
  while (cursor != std::string_view::npos) {
    ++count;
    cursor = text.find(needle, cursor + needle.size());
  }

  return count;
}

auto count_substring_case_insensitive(const std::string_view text,
                                      const std::string_view needle) noexcept
    -> std::size_t {
  if (needle.empty() || needle.size() > text.size())
    return 0;

  std::size_t count = 0;
  for (std::size_t i = 0; i <= text.size() - needle.size(); ++i) {
    bool matched = true;
    for (std::size_t j = 0; j < needle.size(); ++j) {
      if (!ascii_equal_case_insensitive(text[i + j], needle[j])) {
        matched = false;
        break;
      }
    }
    if (matched)
      ++count;
  }

  return count;
}

auto contains_substring(const std::string_view text,
                        const std::string_view needle) noexcept -> bool {
  return text.find(needle) != std::string_view::npos;
}

auto replace_tabs_with_spaces(std::string &text) noexcept -> void {
  std::replace(text.begin(), text.end(), '\t', ' ');
}

auto read_and_preprocess_file(const std::filesystem::path &file_name)
    -> PreprocessedFile {
  std::ifstream input(file_name);
  if (!input.is_open())
    throw std::runtime_error("Unable to open dictionary file: " +
                             file_name.string());

  PreprocessedFile result;
  result.file_name = file_name;

  int line_number = 1;
  std::string line;
  while (std::getline(input, line)) {
    const std::size_t comment_marker = line.find("//");
    if (comment_marker != std::string::npos)
      line.erase(comment_marker);
    replace_tabs_with_spaces(line);

    if (is_blank(line)) {
      result.indices_of_blank_lines.push_back(line_number);
      result.blank_lines.push_back(line);
    } else {
      result.indices_of_good_lines.push_back(line_number);
      result.good_lines.push_back(line);
    }

    result.clean_lines.push_back(line);
    ++line_number;
  }

  if (input.bad())
    throw std::runtime_error("I/O error while reading dictionary file: " +
                             file_name.string());

  return result;
}
} // namespace OpenSMOKEpp::Lexer
