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

#include "test_framework.h"
#include "test_support.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace OpenSMOKEpp;
using testing::attach;
using testing::kw;
using testing::parse;

namespace {
// A dictionary containing only @A, used to exercise grammar coherence.
auto dictionary_with(const std::string &body) -> DictionaryManager {
  return parse("Dictionary d { " + body + " }");
}

// Captures std::cout while alive.
class CoutCapture {
public:
  CoutCapture() : previous_(std::cout.rdbuf(buffer_.rdbuf())) {}
  ~CoutCapture() { std::cout.rdbuf(previous_); }
  CoutCapture(const CoutCapture &) = delete;
  CoutCapture &operator=(const CoutCapture &) = delete;
  [[nodiscard]] auto text() const -> std::string { return buffer_.str(); }

private:
  std::ostringstream buffer_;
  std::streambuf *previous_;
};
} // namespace

// ---------------------------------------------------------------------------
// Grammar coherence (checked when the grammar is built)
// ---------------------------------------------------------------------------
TEST_CASE("grammar", "a coherent grammar is accepted") {
  auto manager = dictionary_with("@MoleFractions CH4 1; @T 300 K; @P 1 atm;");
  CHECK_NOTHROW(attach(
      manager("d"),
      {kw("@MoleFractions", VECTOR_STRING_DOUBLE, true, "@MassFractions @Moles",
          "none", "none"),
       kw("@MassFractions", VECTOR_STRING_DOUBLE, true,
          "@MoleFractions @Moles", "none", "none"),
       kw("@Moles", VECTOR_STRING_DOUBLE, true, "@MoleFractions @MassFractions",
          "none", "none"),
       kw("@T", SINGLE_MEASURE, true),
       kw("@P", SINGLE_MEASURE, false, "none", "@T", "@Rho"),
       kw("@Rho", SINGLE_MEASURE, false, "none", "none", "@P")}));
}

TEST_CASE("grammar", "names, duplicates and references") {
  auto manager = dictionary_with("@A 1;");
  auto &d = manager("d");
  CHECK_THROWS_WITH(attach(d, {kw("@A", SINGLE_INT), kw("B", SINGLE_INT)}),
                    "invalid keyword name 'B': names must start with '@'");
  CHECK_THROWS_WITH(attach(d, {kw("@A", SINGLE_INT), kw("@A", SINGLE_DOUBLE)}),
                    "the keyword @A is defined more than once");
  CHECK_THROWS_WITH(
      attach(d, {kw("@A", SINGLE_INT, false, "none", "@Missing", "none")}),
      "the keyword @A lists the undefined keyword @Missing among its required "
      "keywords");
  CHECK_THROWS_WITH(
      attach(d, {kw("@A", SINGLE_INT, false, "none", "none", "@A")}),
      "the keyword @A lists itself among its conflicting keywords");
  CHECK_THROWS_WITH(attach(d, {kw("@A", SINGLE_INT, false, "none", "@B @B",
                                  "none"),
                               kw("@B", SINGLE_INT)}),
                    "the keyword @A lists @B more than once among its required "
                    "keywords");
}

TEST_CASE("grammar", "contradictory relations") {
  auto manager = dictionary_with("@A 1;");
  auto &d = manager("d");
  CHECK_THROWS_WITH(attach(d, {kw("@A", SINGLE_INT, false, "none", "@B", "@B"),
                               kw("@B", SINGLE_INT, false, "none", "none", "@A")}),
                    "the keyword @A both requires and conflicts with @B");
  CHECK_THROWS_WITH(attach(d, {kw("@A", SINGLE_INT, true, "@B", "@B", "none"),
                               kw("@B", SINGLE_INT, true, "@A", "none", "none")}),
                    "the keyword @A requires @B, which is one of its mutually "
                    "exclusive compulsory alternatives");
  CHECK_THROWS_WITH(attach(d, {kw("@A", SINGLE_INT, false, "none", "none", "@B"),
                               kw("@B", SINGLE_INT)}),
                    "conflicts must be declared on both keywords");
  CHECK_THROWS_WITH(attach(d, {kw("@A", SINGLE_INT, true, "none", "none", "@B"),
                               kw("@B", SINGLE_INT, true, "none", "none", "@A")}),
                    "the compulsory keywords @A and @B conflict with each "
                    "other");
}

TEST_CASE("grammar", "alternative groups must be closed") {
  auto manager = dictionary_with("@A 1;");
  auto &d = manager("d");
  CHECK_THROWS_WITH(
      attach(d, {kw("@A", SINGLE_INT, true, "@B @C", "none", "none"),
                 kw("@B", SINGLE_INT, true, "@A", "none", "none"),
                 kw("@C", SINGLE_INT, true, "@A", "none", "none")}),
      "inconsistent compulsory alternatives: @A defines the group {@A, @B, "
      "@C} but @B defines {@A, @B}");
  CHECK_THROWS_WITH(
      attach(d, {kw("@A", SINGLE_INT, true, "@B", "none", "none"),
                 kw("@B", SINGLE_INT)}),
      "the keyword @A lists @B as a compulsory alternative, but @B is not "
      "compulsory");
}

TEST_CASE("grammar", "dead keywords through transitive requirements") {
  auto manager = dictionary_with("@A 1;");
  auto &d = manager("d");
  // @X -> @Y -> @Z, and @Z conflicts with @X.
  CHECK_THROWS_WITH(
      attach(d, {kw("@A", SINGLE_INT),
                 kw("@X", SINGLE_INT, false, "none", "@Y", "@Z"),
                 kw("@Y", SINGLE_INT, false, "none", "@Z", "none"),
                 kw("@Z", SINGLE_INT, false, "none", "none", "@X")}),
      "the keyword @X can never be used: it requires @Z, which conflicts with "
      "it");
  // @X requires @Y, which conflicts with the compulsory @A.
  CHECK_THROWS_WITH(
      attach(d, {kw("@A", SINGLE_INT, true, "none", "none", "@Y"),
                 kw("@X", SINGLE_INT, false, "none", "@Y", "none"),
                 kw("@Y", SINGLE_INT, false, "none", "none", "@A")}),
      "the keyword @Y can never be used: it conflicts with @A, which is "
      "always required");
  // @X requires two members of the same alternative group.
  CHECK_THROWS_WITH(
      attach(d, {kw("@A", SINGLE_INT, true, "@B", "none", "none"),
                 kw("@B", SINGLE_INT, true, "@A", "none", "none"),
                 kw("@X", SINGLE_INT, false, "none", "@A @B", "none")}),
      "the keyword @X can never be used: it requires both @A and @B, which are "
      "mutually exclusive compulsory alternatives");
}

TEST_CASE("grammar", "unsatisfiable grammars") {
  auto manager = dictionary_with("@A 1;");
  auto &d = manager("d");
  // The compulsory @A requires @B, which conflicts with the compulsory @C.
  CHECK_THROWS_WITH(
      attach(d, {kw("@A", SINGLE_INT, true, "none", "@B", "none"),
                 kw("@B", SINGLE_INT, false, "none", "none", "@C"),
                 kw("@C", SINGLE_INT, true, "none", "none", "@B")}),
      "the grammar can never be satisfied: @A is always required, but it "
      "requires @B, which conflicts with @C, which is always required");
  // Every member of an alternative group is dead.
  CHECK_THROWS_WITH(
      attach(d, {kw("@M", SINGLE_INT, true),
                 kw("@A", SINGLE_INT, true, "@B", "none", "@M"),
                 kw("@B", SINGLE_INT, true, "@A", "none", "@M"),
                 kw("@M2", SINGLE_INT, false, "none", "none", "none")}),
      "conflicts must be declared on both keywords");
  CHECK_THROWS_WITH(
      attach(d, {kw("@M", SINGLE_INT, true, "none", "none", "@A @B"),
                 kw("@A", SINGLE_INT, true, "@B", "none", "@M"),
                 kw("@B", SINGLE_INT, true, "@A", "none", "@M")}),
      "none of the compulsory alternatives {@A, @B} can be used");
}

TEST_CASE("grammar", "all problems are reported together") {
  auto manager = dictionary_with("@A 1;");
  try {
    attach(manager("d"),
           {kw("@A", SINGLE_INT, false, "none", "@Q", "@A"),
            kw("@B", SINGLE_INT, false, "none", "none", "@B")});
    CHECK(false);
  } catch (const std::exception &error) {
    const std::string what = error.what();
    CHECK(what.starts_with("Grammar of dictionary d: 3 problems found:"));
    CHECK(what.find("undefined keyword @Q") != std::string::npos);
    CHECK(what.find("@A lists itself") != std::string::npos);
    CHECK(what.find("@B lists itself") != std::string::npos);
  }
}

TEST_CASE("grammar", "the grammar object is restored after SetGrammar") {
  auto manager = dictionary_with("@A 1;");
  testing::ListGrammar grammar({kw("@A", SINGLE_INT)});
  manager("d").SetGrammar(grammar);
  CHECK(grammar.FindKeyWord("@A") == nullptr);
  CHECK(grammar.name().empty());
}

// ---------------------------------------------------------------------------
// Dictionary validation (checked when the grammar is attached)
// ---------------------------------------------------------------------------
TEST_CASE("validation", "every violation is reported, with suggestions") {
  auto manager = parse("Dictionary d\n{\n"
                       "  @Temprature 300 K;\n"
                       "  @MoleFractions CH4 1;\n"
                       "  @MassFractions CH4 1;\n"
                       "  @Fuel CH4;\n"
                       "  @Lean true;\n"
                       "  @Rich true;\n"
                       "}\n");
  const CoutCapture capture;
  try {
    attach(manager("d"),
           {kw("@Temperature", SINGLE_MEASURE, true),
            kw("@MoleFractions", VECTOR_STRING_DOUBLE, true, "@MassFractions",
               "none", "none"),
            kw("@MassFractions", VECTOR_STRING_DOUBLE, true, "@MoleFractions",
               "none", "none"),
            kw("@Fuel", SINGLE_STRING, false, "none", "@Oxidizer", "none"),
            kw("@Oxidizer", SINGLE_STRING),
            kw("@Lean", SINGLE_BOOL, false, "none", "none", "@Rich"),
            kw("@Rich", SINGLE_BOOL, false, "none", "none", "@Lean")});
    CHECK(false);
  } catch (const std::exception &error) {
    const std::string what = error.what();
    CHECK(what.find("Dictionary d defined in file test.dic: 5 errors found:") !=
          std::string::npos);
    CHECK(what.find("the keyword @Temprature is not recognized in this "
                    "dictionary (did you mean @Temperature?)") !=
          std::string::npos);
    CHECK(what.find("the compulsory keyword @Temperature is missing") !=
          std::string::npos);
    CHECK(what.find("only one of the mutually exclusive keywords "
                    "@MassFractions, @MoleFractions can be specified") !=
          std::string::npos);
    CHECK(what.find("the keyword @Fuel requires @Oxidizer, which is missing") !=
          std::string::npos);
    CHECK(what.find("the keywords @Lean and @Rich are mutually exclusive") !=
          std::string::npos);
    CHECK(what.find("List of available options in Dictionary d") !=
          std::string::npos);
  }
  CHECK(capture.text().empty());
}

TEST_CASE("validation", "missing alternative group") {
  auto manager = parse("Dictionary d { @T 300 K; }");
  CHECK_THROWS_WITH(
      attach(manager("d"),
             {kw("@T", SINGLE_MEASURE),
              kw("@X", VECTOR_STRING_DOUBLE, true, "@Y", "none", "none"),
              kw("@Y", VECTOR_STRING_DOUBLE, true, "@X", "none", "none")}),
      "one of the compulsory keywords @X, @Y must be specified");
}

TEST_CASE("validation", "@Help returns the keyword table in the exception") {
  auto manager = parse("Dictionary d { @Help; }");
  const CoutCapture capture;
  CHECK_THROWS_WITH(attach(manager("d"), {kw("@T", SINGLE_MEASURE)}),
                    "the user requested @Help");
  CHECK_THROWS_WITH(attach(manager("d"), {kw("@T", SINGLE_MEASURE)}),
                    "List of available options in Dictionary d");
  CHECK(capture.text().empty());
}
