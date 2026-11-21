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

#include "DictionaryParser.h"
#include "DictionaryLexer.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>

namespace OpenSMOKEpp::detail {
namespace {
using Lexer::SyntaxError;
using Lexer::Token;
using Lexer::TokenKind;

[[nodiscard]] auto is_keyword(const Token &token) noexcept -> bool {
  return token.kind == TokenKind::Word && token.text.size() > 1 &&
         token.text.front() == '@';
}

[[nodiscard]] auto is_word(const Token &token, std::string_view text) noexcept
    -> bool {
  return token.kind == TokenKind::Word && token.text == text;
}

class Parser {
public:
  Parser(const std::vector<Token> &tokens, std::string file_name)
      : tokens_(tokens), file_(std::move(file_name)) {}

  auto run() -> ParsedSource {
    while (!at_end()) {
      const Token &token = current();
      if (is_word(token, "Dictionary"))
        parse_dictionary();
      else if (is_word(token, "#include"))
        parse_include();
      else
        fail(token, "unexpected '" + token.spelling +
                        "' outside of a dictionary: expected "
                        "'Dictionary <name>' or '#include \"file\"'");
    }
    return std::move(result_);
  }

private:
  const std::vector<Token> &tokens_;
  std::string file_;
  std::size_t position_ = 0;
  ParsedSource result_;

  [[nodiscard]] auto at_end() const noexcept -> bool {
    return position_ >= tokens_.size();
  }
  [[nodiscard]] auto current() const -> const Token & {
    return tokens_[position_];
  }
  [[noreturn]] void fail(const Token &token, const std::string &message) const {
    throw SyntaxError(file_, token.line, token.column, message);
  }

  void parse_include() {
    const Token &directive = tokens_[position_++];
    if (at_end() || (current().kind != TokenKind::Word &&
                     current().kind != TokenKind::QuotedString))
      fail(directive, "'#include' must be followed by a file name, e.g. "
                      "#include \"common.dic\"");
    const Token &path = tokens_[position_++];
    if (path.text.empty())
      fail(path, "'#include' requires a non-empty file name");
    if (!at_end() && current().kind == TokenKind::Semicolon)
      ++position_;
    result_.includes.push_back(
        IncludeDirective{path.text, directive.line, directive.column});
  }

  void parse_dictionary() {
    const Token &header = tokens_[position_++];
    if (at_end())
      fail(header, "'Dictionary' must be followed by a name and a block "
                   "'{ ... }'");
    const Token &name = current();
    const bool valid_name =
        (name.kind == TokenKind::Word || name.kind == TokenKind::QuotedString) &&
        !name.text.empty() &&
        (name.kind == TokenKind::QuotedString ||
         (name.text.front() != '@' && name.text.front() != '#'));
    if (!valid_name)
      fail(name, "expected a dictionary name after 'Dictionary', found '" +
                     name.spelling + "'");
    ++position_;
    if (at_end() || current().kind != TokenKind::LeftBrace)
      fail(at_end() ? name : current(),
           "expected '{' after 'Dictionary " + name.text + "'");
    parse_block(name.text, header.line);
  }

  // Parses "{ entry+ }" starting at '{'. Returns the line of the closing '}'.
  auto parse_block(const std::string &name, const unsigned int header_line)
      -> unsigned int {
    const Token &open = tokens_[position_++];
    ParsedDictionary dictionary;
    dictionary.name = name;
    dictionary.line = header_line;

    unsigned int closing_line = 0;
    while (true) {
      if (at_end())
        fail(open, "the block of dictionary '" + name +
                       "' is not closed: missing '}'");
      const Token &token = current();
      if (token.kind == TokenKind::RightBrace) {
        closing_line = token.line;
        ++position_;
        break;
      }
      if (is_keyword(token)) {
        parse_entry(dictionary);
        continue;
      }
      if (is_word(token, "Dictionary"))
        fail(token, "a 'Dictionary' cannot be defined inside dictionary '" +
                        name + "' (missing '}'?). Use an inline block "
                               "instead: '@Keyword { ... }'");
      fail(token, "expected a keyword starting with '@' or '}' in "
                  "dictionary '" +
                      name + "', found '" + token.spelling + "'");
    }

    if (dictionary.entries.empty())
      fail(open, "dictionary '" + name + "' contains no keyword entries");

    std::unordered_map<std::string, unsigned int> first_line;
    first_line.reserve(dictionary.entries.size());
    for (const auto &entry : dictionary.entries) {
      const auto [it, inserted] =
          first_line.emplace(entry.keyword, entry.first_line);
      if (!inserted)
        throw SyntaxError(file_, entry.first_line, entry.column,
                          "the keyword " + entry.keyword +
                              " is specified twice in dictionary '" + name +
                              "' (lines " + std::to_string(it->second) +
                              " and " + std::to_string(entry.first_line) +
                              ")");
    }

    result_.dictionaries.push_back(std::move(dictionary));
    return closing_line;
  }

  void parse_entry(ParsedDictionary &dictionary) {
    const Token &keyword = tokens_[position_++];
    ParsedEntry entry;
    entry.keyword = keyword.text;
    entry.first_line = keyword.line;
    entry.last_line = keyword.line;
    entry.column = keyword.column;

    // Inline dictionary: @Keyword { ... } [;]
    if (!at_end() && current().kind == TokenKind::LeftBrace) {
      const std::string child = dictionary.name + "/" + keyword.text.substr(1);
      entry.last_line = parse_block(child, keyword.line);
      entry.values.push_back(child);
      entry.option = child;
      if (!at_end() && current().kind == TokenKind::Semicolon)
        ++position_;
      dictionary.entries.push_back(std::move(entry));
      return;
    }

    while (true) {
      if (at_end())
        fail(keyword, "missing ';' at the end of keyword " + keyword.text);
      const Token &token = current();
      switch (token.kind) {
      case TokenKind::Semicolon:
        entry.last_line = token.line;
        ++position_;
        dictionary.entries.push_back(std::move(entry));
        return;
      case TokenKind::RightBrace:
        fail(token, "missing ';' at the end of keyword " + keyword.text +
                        " (line " + std::to_string(keyword.line) +
                        ") before '}'");
      case TokenKind::LeftBrace:
        fail(token, "'{' can only directly follow a keyword to open an "
                    "inline dictionary, e.g. '" +
                        keyword.text + " { ... }'");
      case TokenKind::Word:
        if (token.text.front() == '@')
          fail(token, "unexpected keyword " + token.text +
                          ": missing ';' at the end of keyword " +
                          keyword.text + " (line " +
                          std::to_string(keyword.line) + ")");
        [[fallthrough]];
      case TokenKind::QuotedString:
        entry.values.push_back(token.text);
        if (!entry.option.empty())
          entry.option += ' ';
        entry.option += token.spelling;
        ++position_;
        break;
      }
    }
  }
};
} // namespace

auto parse_source(const std::string_view source, const std::string_view file_name)
    -> ParsedSource {
  const auto tokens = Lexer::tokenize(source, file_name);
  Parser parser(tokens, std::string(file_name));
  return parser.run();
}
} // namespace OpenSMOKEpp::detail
