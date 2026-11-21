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
#include "test_framework.h"

#include <string>
#include <vector>

using OpenSMOKEpp::Lexer::strip_comment;
using OpenSMOKEpp::Lexer::SyntaxError;
using OpenSMOKEpp::Lexer::Token;
using OpenSMOKEpp::Lexer::TokenKind;
using OpenSMOKEpp::Lexer::tokenize;

namespace {
auto texts(const std::vector<Token> &tokens) -> std::vector<std::string> {
  std::vector<std::string> result;
  for (const auto &token : tokens)
    result.push_back(token.text);
  return result;
}
} // namespace

TEST_CASE("lexer", "words and delimiters") {
  const auto tokens = tokenize("@Moles O2 0.21\n   N2 0.79;", "t");
  CHECK(texts(tokens) ==
        (std::vector<std::string>{"@Moles", "O2", "0.21", "N2", "0.79", ";"}));
  REQUIRE(tokens.size() == 6);
  CHECK(tokens[5].kind == TokenKind::Semicolon);
  CHECK(tokens[3].line == 2);
  CHECK(tokens[3].column == 4);
}

TEST_CASE("lexer", "braces are tokens even when glued") {
  const auto tokens = tokenize("Dictionary a{@X 1;}", "t");
  CHECK(texts(tokens) == (std::vector<std::string>{"Dictionary", "a", "{",
                                                   "@X", "1", ";", "}"}));
  CHECK(tokens[2].kind == TokenKind::LeftBrace);
  CHECK(tokens[6].kind == TokenKind::RightBrace);
}

TEST_CASE("lexer", "comments start only at the start of a token") {
  CHECK(texts(tokenize("@X 1; // comment", "t")) ==
        (std::vector<std::string>{"@X", "1", ";"}));
  CHECK(texts(tokenize("@X 1;// comment", "t")) ==
        (std::vector<std::string>{"@X", "1", ";"}));
  CHECK(texts(tokenize("// whole line\n@X 1;", "t")) ==
        (std::vector<std::string>{"@X", "1", ";"}));
  CHECK(texts(tokenize("@Url http://www.example.org/a//b;", "t")) ==
        (std::vector<std::string>{"@Url", "http://www.example.org/a//b",
                                  ";"}));
}

TEST_CASE("lexer", "quoted strings") {
  const auto tokens =
      tokenize(R"(@Title "a b; @c { } // d" next;)", "t");
  REQUIRE(tokens.size() == 4);
  CHECK(tokens[1].kind == TokenKind::QuotedString);
  CHECK(tokens[1].text == "a b; @c { } // d");
  CHECK(tokens[1].spelling == R"("a b; @c { } // d")");
  CHECK(tokens[2].text == "next");
}

TEST_CASE("lexer", "escape sequences") {
  const auto tokens = tokenize(R"("say \"hi\" \\ C:\dir")", "t");
  REQUIRE(tokens.size() == 1);
  CHECK(tokens[0].text == R"(say "hi" \ C:\dir)");
}

TEST_CASE("lexer", "empty quoted string and delimiters after quotes") {
  CHECK(texts(tokenize(R"(@X "";)", "t")) ==
        (std::vector<std::string>{"@X", "", ";"}));
  CHECK(texts(tokenize(R"(@X "a"// c)", "t")) ==
        (std::vector<std::string>{"@X", "a"}));
}

TEST_CASE("lexer", "unterminated string reports its position") {
  try {
    (void)tokenize("@A 1;\n@X   \"abc\n;", "file.dic");
    CHECK(false);
  } catch (const SyntaxError &error) {
    CHECK(error.line() == 2);
    CHECK(error.column() == 6);
    CHECK(error.file_name() == "file.dic");
    CHECK(std::string(error.what()).starts_with("file.dic:2:6: error:"));
    CHECK(error.message().find("unterminated") != std::string::npos);
  }
}

TEST_CASE("lexer", "quote inside a word and glued characters are errors") {
  CHECK_THROWS_AS_WITH((void)tokenize(R"(@X ab"c";)", "t"), SyntaxError,
                       "t:1:6: error: unexpected '\"' inside the word 'ab'");
  CHECK_THROWS_AS_WITH((void)tokenize(R"(@X "a"b;)", "t"), SyntaxError,
                       "right after a quoted string");
}

TEST_CASE("lexer", "strip_comment follows the same rule") {
  CHECK(strip_comment("@X http://a/b // c") == "@X http://a/b ");
  CHECK(strip_comment("@X 1;//c") == "@X 1;");
  CHECK(strip_comment(R"(@X "a // b" // c)") == R"(@X "a // b" )");
  CHECK(strip_comment("// all") == "");
  CHECK(strip_comment("no comment") == "no comment");
}
