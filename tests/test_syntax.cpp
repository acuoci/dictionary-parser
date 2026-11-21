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
#include "test_support.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

using namespace OpenSMOKEpp;
using testing::attach;
using testing::kw;
using testing::parse;

namespace {
auto summary(const Dictionary &dictionary) -> std::string {
  std::ostringstream out;
  dictionary.Summary(out);
  const std::string text = out.str();
  return text.substr(text.find('\n') + 1); // drop the header line
}
} // namespace

// ---------------------------------------------------------------------------
// Legacy layout and backward compatibility
// ---------------------------------------------------------------------------
TEST_CASE("syntax", "legacy layout: entries, options and line ranges") {
  auto manager = parse("// header\n"
                       "Dictionary Initial-Composition\n"
                       "{\n"
                       "\t\t@Temperature   \t833.    K ; \n"
                       "\t\t@Moles  \tN2 0.79\n"
                       "\t\t\t\tO2 0.21;\n"
                       "\t\t@Help;\n"
                       "}\n");
  CHECK(summary(manager("Initial-Composition")) ==
        "@Temperature*833. K*4-4\n"
        "@Moles*N2 0.79 O2 0.21*5-6\n"
        "@Help**7-7\n");
}

TEST_CASE("syntax", "several dictionaries and names()") {
  auto manager = parse("Dictionary b\n{\n@X 1;\n}\nDictionary a\n{\n@Y 2;\n}\n");
  CHECK(manager.names() == (std::vector<std::string>{"a", "b"}));
  CHECK(manager.contains("a"));
  CHECK(!manager.contains("c"));
  CHECK_THROWS_WITH((void)manager("c"), "The c dictionary was not defined");
}

// ---------------------------------------------------------------------------
// (a) comments only at the start of a token
// ---------------------------------------------------------------------------
TEST_CASE("syntax", "URLs and double slashes inside values") {
  auto manager = parse("Dictionary d\n{\n"
                       "  @Url  http://www.opensmoke.polimi.it/; // comment\n"
                       "  @Path ../kinetics//POLIMI.CKI;\n"
                       "}\n");
  auto &d = manager("d");
  attach(d, {kw("@Url", SINGLE_STRING), kw("@Path", SINGLE_PATH)});
  std::string url;
  d.ReadString("@Url", url);
  CHECK(url == "http://www.opensmoke.polimi.it/");
  std::filesystem::path path;
  d.ReadPath("@Path", path);
  CHECK(path == std::filesystem::path("../kinetics//POLIMI.CKI"));
}

// ---------------------------------------------------------------------------
// (b) quoted strings
// ---------------------------------------------------------------------------
TEST_CASE("syntax", "quoted strings with blanks and special characters") {
  auto manager = parse("Dictionary d\n{\n"
                       "  @Title    \"Flame #1; @ 1 atm { lean }\";\n"
                       "  @Output   \"my results/run  01\";\n"
                       "  @Species  \"A B\" C;\n"
                       "  @Comments first \"second item\" third;\n"
                       "}\n");
  auto &d = manager("d");
  attach(d, {kw("@Title", SINGLE_STRING), kw("@Output", SINGLE_PATH),
             kw("@Species", VECTOR_STRING), kw("@Comments", SEQUENCE_STRING)});
  std::string title;
  d.ReadString("@Title", title);
  CHECK(title == "Flame #1; @ 1 atm { lean }");
  std::filesystem::path output;
  d.ReadPath("@Output", output);
  CHECK(output.string() == "my results/run  01");
  std::vector<std::string> species;
  d.ReadOption("@Species", species);
  CHECK(species == (std::vector<std::string>{"A B", "C"}));
  std::string sequence;
  d.ReadSequence("@Comments", sequence);
  CHECK(sequence == "first \"second item\" third");
}

TEST_CASE("syntax", "unquoted multi-token path keeps the legacy behavior") {
  auto manager = parse("Dictionary d\n{\n@Path my   output dir;\n}\n");
  auto &d = manager("d");
  attach(d, {kw("@Path", SINGLE_PATH)});
  std::filesystem::path path;
  d.ReadPath("@Path", path);
  CHECK(path.string() == "my output dir");
}

// ---------------------------------------------------------------------------
// (c) relaxed brace placement
// ---------------------------------------------------------------------------
TEST_CASE("syntax", "braces on the same line") {
  auto manager = parse("Dictionary a { @X 1; @Y 2; }\n"
                       "Dictionary b {\n  @Z 3;\n}\n"
                       "Dictionary c\n{ @W 4;\n  @V 5; }\n");
  CHECK(summary(manager("a")) == "@X*1*1-1\n@Y*2*1-1\n");
  CHECK(summary(manager("b")) == "@Z*3*3-3\n");
  CHECK(summary(manager("c")) == "@W*4*6-6\n@V*5*7-7\n");
}

// ---------------------------------------------------------------------------
// (d) inline dictionaries
// ---------------------------------------------------------------------------
TEST_CASE("syntax", "inline dictionaries are named parent/Keyword") {
  auto manager = parse("Dictionary BatchReactor\n{\n"
                       "  @EndTime 1 s;\n"
                       "  @InitialStatus\n"
                       "  {\n"
                       "    @Temperature 1000 K;\n"
                       "    @Moles { @Inner 1; };\n"
                       "  }\n"
                       "  @Options { @Verbose true; }\n"
                       "}\n");
  CHECK(manager.names() ==
        (std::vector<std::string>{"BatchReactor", "BatchReactor/InitialStatus",
                                  "BatchReactor/InitialStatus/Moles",
                                  "BatchReactor/Options"}));
  CHECK(summary(manager("BatchReactor")) ==
        "@EndTime*1 s*3-3\n"
        "@InitialStatus*BatchReactor/InitialStatus*4-8\n"
        "@Options*BatchReactor/Options*9-9\n");

  auto &main = manager("BatchReactor");
  attach(main, {kw("@EndTime", SINGLE_MEASURE),
                kw("@InitialStatus", SINGLE_DICTIONARY, true),
                kw("@Options", SINGLE_DICTIONARY)});
  std::string name;
  main.ReadDictionary("@InitialStatus", name);
  CHECK(name == "BatchReactor/InitialStatus");
  auto &status = manager(name);
  attach(status, {kw("@Temperature", SINGLE_MEASURE),
                  kw("@Moles", SINGLE_DICTIONARY)});
  double value = 0;
  std::string units;
  status.ReadMeasure("@Temperature", value, units);
  CHECK(value == 1000.0);
  CHECK(units == "K");
}

TEST_CASE("syntax", "inline and explicit dictionary with the same name") {
  CHECK_THROWS_WITH(parse("Dictionary a { @B { @X 1; } }\n"
                          "Dictionary a/B { @Y 2; }\n"),
                    "The dictionary 'a/B' is defined more than once: at "
                    "test.dic:1 and at test.dic:2");
}

// ---------------------------------------------------------------------------
// Syntax errors
// ---------------------------------------------------------------------------
TEST_CASE("syntax", "missing semicolon before the next keyword") {
  try {
    (void)parse("Dictionary d\n{\n  @X 1\n  @Y 2;\n}\n");
    CHECK(false);
  } catch (const Lexer::SyntaxError &error) {
    CHECK(error.line() == 4);
    CHECK(error.column() == 3);
    CHECK(std::string(error.what()) ==
          "test.dic:4:3: error: unexpected keyword @Y: missing ';' at the end "
          "of keyword @X (line 3)");
  }
}

TEST_CASE("syntax", "structural errors") {
  CHECK_THROWS_WITH(parse("Dictionary d\n{\n@X 1;\n"),
                    "test.dic:2:1: error: the block of dictionary 'd' is not "
                    "closed: missing '}'");
  CHECK_THROWS_WITH(parse("Dictionary d\n{\n@X 1\n}\n"),
                    "missing ';' at the end of keyword @X (line 3) before '}'");
  CHECK_THROWS_WITH(parse("Dictionary d { @X 1; }\nstray\n"),
                    "test.dic:2:1: error: unexpected 'stray' outside of a "
                    "dictionary");
  CHECK_THROWS_WITH(parse("Dictonary d { @X 1; }\n"),
                    "unexpected 'Dictonary' outside of a dictionary");
  CHECK_THROWS_WITH(parse("Dictionary d { @X 1; Dictionary e { @Y 1; } }"),
                    "a 'Dictionary' cannot be defined inside dictionary 'd'");
  CHECK_THROWS_WITH(parse("Dictionary d { @X 1 { @Y 2; } }"),
                    "'{' can only directly follow a keyword");
  CHECK_THROWS_WITH(parse("Dictionary d { }"),
                    "dictionary 'd' contains no keyword entries");
  CHECK_THROWS_WITH(parse("Dictionary d { X 1; }"),
                    "expected a keyword starting with '@' or '}' in "
                    "dictionary 'd', found 'X'");
  CHECK_THROWS_WITH(parse("Dictionary { @X 1; }"),
                    "expected a dictionary name after 'Dictionary'");
  CHECK_THROWS_WITH(parse("Dictionary d @X 1;"), "expected '{' after");
  CHECK_THROWS_WITH(parse("Dictionary d { @X 1;"), "missing '}'");
  CHECK_THROWS_WITH(parse("// only comments\n"), "No dictionaries are specified");
}

TEST_CASE("syntax", "duplicate keywords and dictionaries") {
  CHECK_THROWS_WITH(parse("Dictionary d\n{\n@X 1;\n@X 2;\n}\n"),
                    "test.dic:4:1: error: the keyword @X is specified twice "
                    "in dictionary 'd' (lines 3 and 4)");
  CHECK_THROWS_WITH(parse("Dictionary d { @X 1; }\nDictionary d { @X 1; }\n"),
                    "The dictionary 'd' is defined more than once");
}

// ---------------------------------------------------------------------------
// #include
// ---------------------------------------------------------------------------
TEST_CASE("include", "relative includes, nesting and include-once") {
  testing::TemporaryDirectory dir;
  dir.write("common/solvers.dic",
            "Dictionary ode-parameters { @RelativeTolerance 1e-6; }\n"
            "#include \"../shared.dic\"\n");
  dir.write("shared.dic", "Dictionary shared { @Value 1; }\n");
  const auto main = dir.write("main.dic",
                              "#include \"common/solvers.dic\"\n"
                              "#include \"shared.dic\";\n"
                              "Dictionary main { @OdeParameters ode-parameters; }\n");
  DictionaryManager manager;
  manager.ReadDictionariesFromFile(main.string());
  CHECK(manager.names() ==
        (std::vector<std::string>{"main", "ode-parameters", "shared"}));
}

TEST_CASE("include", "errors in included files report the included file") {
  testing::TemporaryDirectory dir;
  dir.write("bad.dic", "Dictionary bad\n{\n  @X 1\n}\n");
  const auto main = dir.write("main.dic", "Dictionary main { @Y 1; }\n"
                                          "#include \"bad.dic\"\n");
  DictionaryManager manager;
  CHECK_THROWS_WITH(manager.ReadDictionariesFromFile(main.string()),
                    "bad.dic:4:1: error: missing ';'");
}

TEST_CASE("include", "missing file and circular includes") {
  testing::TemporaryDirectory dir;
  const auto missing =
      dir.write("missing.dic", "Dictionary m { @X 1; }\n\n  #include \"nope.dic\"\n");
  DictionaryManager first;
  CHECK_THROWS_WITH(first.ReadDictionariesFromFile(missing.string()),
                    "missing.dic:3:3: error: cannot open included file "
                    "'nope.dic'");

  dir.write("a.dic", "Dictionary a { @X 1; }\n#include \"b.dic\"\n");
  dir.write("b.dic", "Dictionary b { @X 1; }\n#include \"a.dic\"\n");
  DictionaryManager second;
  CHECK_THROWS_WITH(
      second.ReadDictionariesFromFile((dir.path() / "a.dic").string()),
      "circular #include: a.dic -> b.dic -> a.dic");
}

TEST_CASE("include", "include from a string uses the given directory") {
  testing::TemporaryDirectory dir;
  dir.write("common.dic", "Dictionary common { @X 1; }\n");
  DictionaryManager manager;
  manager.ReadDictionariesFromString("#include \"common.dic\"\n"
                                     "Dictionary main { @Y 1; }\n",
                                     "inline.dic", dir.path());
  CHECK(manager.contains("common"));
  CHECK_THROWS_WITH(parse("Dictionary d { #include \"x.dic\" }"),
                    "expected a keyword starting with '@' or '}'");
  CHECK_THROWS_WITH(parse("#include\n"), "'#include' must be followed by a file name");
}

TEST_CASE("include", "the syntax-extensions example parses") {
  const std::filesystem::path examples(DICTIONARY_PARSER_EXAMPLES_DIR);
  DictionaryManager manager;
  manager.ReadDictionariesFromFile(
      (examples / "syntax-extensions" / "main.dic")
          .string());
  CHECK(manager.contains("CounterFlowFlame1D"));
  CHECK(manager.contains("CounterFlowFlame1D/FuelStream"));
  CHECK(manager.contains("dae-parameters"));
}
