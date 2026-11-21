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
#include <string>
#include <utility>

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

[[nodiscard]] auto is_delimiter(const char value) noexcept -> bool {
  return value == ';' || value == '{' || value == '}';
}

[[nodiscard]] auto starts_comment(const std::string_view text,
                                  const std::size_t i) noexcept -> bool {
  return i + 1 < text.size() && text[i] == '/' && text[i + 1] == '/';
}

// Position of the first `//` that starts a comment, honoring the token-start
// rule and double-quoted strings. Returns npos if there is none.
[[nodiscard]] auto find_comment_start(const std::string_view text) noexcept
    -> std::size_t {
  bool at_token_start = true;
  bool in_string = false;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (in_string) {
      if (c == '\\' && i + 1 < text.size() &&
          (text[i + 1] == '"' || text[i + 1] == '\\'))
        ++i;
      else if (c == '"') {
        in_string = false;
        at_token_start = true;
      }
      continue;
    }
    if (is_space(c) || is_delimiter(c)) {
      at_token_start = true;
      continue;
    }
    if (at_token_start && starts_comment(text, i))
      return i;
    if (at_token_start && c == '"') {
      in_string = true;
      continue;
    }
    at_token_start = false;
  }
  return std::string_view::npos;
}
} // namespace

SyntaxError::SyntaxError(std::string file_name, const unsigned int line,
                         const unsigned int column, const std::string &message)
    : std::runtime_error(file_name + ":" + std::to_string(line) + ":" +
                         std::to_string(column) + ": error: " + message),
      file_name_(std::move(file_name)), line_(line), column_(column),
      message_(message) {}

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
  const std::size_t marker = find_comment_start(text);
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
    const std::size_t comment_marker = find_comment_start(line);
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
auto tokenize(const std::string_view source, const std::string_view file_name)
    -> std::vector<Token> {
  const std::string file(file_name);
  std::vector<Token> tokens;
  tokens.reserve(source.size() / 4);

  std::size_t i = 0;
  unsigned int line = 1;
  unsigned int column = 1;

  const auto advance = [&]() noexcept {
    if (source[i] == '\n') {
      ++line;
      column = 1;
    } else
      ++column;
    ++i;
  };

  const auto describe = [](const char c) -> std::string {
    if (static_cast<unsigned char>(c) < 32U || c == 127)
      return "character code " + std::to_string(static_cast<int>(c));
    return std::string("'") + c + "'";
  };

  while (i < source.size()) {
    const char c = source[i];

    if (is_space(c)) {
      advance();
      continue;
    }

    // Comment: only reachable at the start of a token.
    if (starts_comment(source, i)) {
      while (i < source.size() && source[i] != '\n')
        advance();
      continue;
    }

    const unsigned int token_line = line;
    const unsigned int token_column = column;

    if (is_delimiter(c)) {
      const TokenKind kind = c == ';'   ? TokenKind::Semicolon
                             : c == '{' ? TokenKind::LeftBrace
                                        : TokenKind::RightBrace;
      tokens.push_back(
          Token{kind, std::string(1, c), std::string(1, c), token_line,
                token_column});
      advance();
      continue;
    }

    if (c == '"') {
      std::string text;
      std::string spelling(1, '"');
      advance();
      bool closed = false;
      while (i < source.size()) {
        const char d = source[i];
        if (d == '\n' || d == '\r')
          break;
        if (d == '\\' && i + 1 < source.size() &&
            (source[i + 1] == '"' || source[i + 1] == '\\')) {
          spelling += d;
          spelling += source[i + 1];
          text += source[i + 1];
          advance();
          advance();
          continue;
        }
        spelling += d;
        advance();
        if (d == '"') {
          closed = true;
          break;
        }
        text += d;
      }
      if (!closed)
        throw SyntaxError(file, token_line, token_column,
                          "unterminated quoted string: the closing '\"' must "
                          "be on the same line");
      if (i < source.size() && !is_space(source[i]) &&
          !is_delimiter(source[i]) && !starts_comment(source, i))
        throw SyntaxError(file, line, column,
                          "unexpected " + describe(source[i]) +
                              " right after a quoted string: separate the "
                              "values with a blank");
      tokens.push_back(Token{TokenKind::QuotedString, std::move(text),
                             std::move(spelling), token_line, token_column});
      continue;
    }

    std::string word;
    while (i < source.size() && !is_space(source[i]) &&
           !is_delimiter(source[i])) {
      if (source[i] == '"')
        throw SyntaxError(file, line, column,
                          "unexpected '\"' inside the word '" + word +
                              "': a quoted string must start at the "
                              "beginning of a value");
      word += source[i];
      advance();
    }
    tokens.push_back(
        Token{TokenKind::Word, word, word, token_line, token_column});
  }

  return tokens;
}
} // namespace OpenSMOKEpp::Lexer
