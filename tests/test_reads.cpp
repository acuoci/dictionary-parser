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

#include <stdexcept>
#include <string>
#include <vector>

using namespace OpenSMOKEpp;
using testing::attach;
using testing::kw;
using testing::parse;

namespace {
const char *const source = "Dictionary d\n"
                           "{\n"
                           "  @Int      42;\n"
                           "  @Double   2.5e-3;\n"
                           "  @Bool     TRUE;\n"
                           "  @Off      Off;\n"
                           "  @Measure  300 K;\n"
                           "  @Ints     1 2 3;\n"
                           "  @Doubles  1 2.5 -3e2;\n"
                           "  @Pairs    CH4 0.5 O2 0.5;\n"
                           "  @Measures 1 m 2 cm;\n"
                           "  @IntNames 1 a 2 b;\n"
                           "  @Bools    on false;\n"
                           "  @Char     x;\n"
                           "}\n";

// Invalid values: reported all together when the grammar is attached.
const char *const invalid_source = "Dictionary d\n"
                                   "{\n"
                                   "  @BadInt   4.2;\n"
                                   "  @BadBool  maybe;\n"
                                   "  @TwoDoubles 1 2;\n"
                                   "  @NaN      nan;\n"
                                   "  @Odd      CH4 0.5 O2;\n"
                                   "}\n";

auto grammar() -> std::vector<DictionaryKeyWord> {
  return {kw("@Int", SINGLE_INT),          kw("@Double", SINGLE_DOUBLE),
          kw("@Bool", SINGLE_BOOL),        kw("@Off", SINGLE_BOOL),
          kw("@Measure", SINGLE_MEASURE),  kw("@Ints", VECTOR_INT),
          kw("@Doubles", VECTOR_DOUBLE),   kw("@Pairs", VECTOR_STRING_DOUBLE),
          kw("@Odd", VECTOR_STRING_DOUBLE), kw("@Measures", VECTOR_MEASURE),
          kw("@IntNames", VECTOR_INT_STRING), kw("@Bools", VECTOR_BOOL),
          kw("@Char", SINGLE_CHAR),        kw("@BadInt", SINGLE_INT),
          kw("@BadBool", SINGLE_BOOL),     kw("@TwoDoubles", SINGLE_DOUBLE),
          kw("@NaN", SINGLE_DOUBLE),       kw("@Absent", SINGLE_DOUBLE)};
}
} // namespace

TEST_CASE("reads", "typed reads") {
  auto manager = parse(source);
  auto &d = manager("d");
  attach(d, grammar());

  CHECK(d.Read<int>("@Int") == 42);
  CHECK(d.Read<double>("@Double") == 2.5e-3);
  CHECK(d.Read<bool>("@Bool"));
  CHECK(!d.Read<bool>("@Off"));
  // Without a dimension, a measure is a record (value, units).
  CHECK((d.Read<std::pair<double, std::string>>("@Measure") ==
        (std::pair<double, std::string>{300.0, "K"})));
  CHECK(d.Read<std::vector<int>>("@Ints") == (std::vector<int>{1, 2, 3}));
  CHECK(d.Read<std::vector<double>>("@Doubles") ==
        (std::vector<double>{1, 2.5, -300}));
  using Pairs = std::vector<std::pair<std::string, double>>;
  CHECK(d.Read<Pairs>("@Pairs") == (Pairs{{"CH4", 0.5}, {"O2", 0.5}}));
  using Measures = std::vector<std::pair<double, std::string>>;
  CHECK(d.Read<Measures>("@Measures") == (Measures{{1, "m"}, {2, "cm"}}));
  using IntNames = std::vector<std::pair<int, std::string>>;
  CHECK(d.Read<IntNames>("@IntNames") == (IntNames{{1, "a"}, {2, "b"}}));
  CHECK(d.Read<std::vector<bool>>("@Bools") == (std::vector<bool>{true, false}));
  CHECK(d.Read<std::string>("@Char") == "x");
  CHECK(d.CheckOption("@Int"));
  CHECK(!d.CheckOption("@Absent"));
  CHECK(d.has_grammar());
}

TEST_CASE("reads", "invalid values are reported when the grammar is attached") {
  auto manager = parse(invalid_source);
  try {
    attach(manager("d"), grammar());
    CHECK(false);
  } catch (const DictionaryError &error) {
    const std::string what = error.what();
    CHECK(error.error_count() == 5);
    CHECK(what.find("test.dic:3:13: error: keyword @BadInt: the value must be "
                    "an integer, but '4.2' was given") != std::string::npos);
    CHECK(what.find("test.dic:4:13: error: keyword @BadBool: a boolean "
                    "(true/false/on/off) is required, but 'maybe' was given") !=
          std::string::npos);
    CHECK(what.find("test.dic:5:3: error: keyword @TwoDoubles: exactly one "
                    "number (double) is required, but 2 values were given") !=
          std::string::npos);
    CHECK(what.find("keyword @NaN: the value must be a finite number, but "
                    "'nan' was given") != std::string::npos);
    CHECK(what.find("keyword @Odd: pairs of a name and a number (double) are "
                    "required") != std::string::npos);
    for (const auto &d : error.diagnostics())
      CHECK(d.code == DiagnosticCode::InvalidValue);
  }
}

TEST_CASE("reads", "programming errors are explicit") {
  auto manager = parse(source);
  auto &d = manager("d");
  CHECK_THROWS_WITH((void)d.Read<double>("@Double"),
                    "cannot read keyword @Double: no grammar is attached to "
                    "this dictionary");
  attach(d, grammar());
  CHECK_THROWS_WITH((void)d.Read<double>("@Typo"),
                    "the keyword @Typo is not declared in the grammar");
  CHECK_THROWS_WITH((void)d.Read<double>("@Measure"),
                    "the value of @Measure cannot be read as the requested C++ "
                    "type");
  CHECK_THROWS_WITH((void)d.Read<double>("@Absent"),
                    "the required keyword @Absent is not present");
}

TEST_CASE("reads", "SetDictionary validates sizes") {
  Dictionary d;
  CHECK_THROWS_AS_WITH(d.SetDictionary({"@A"}, {"1", "2"}, {1}, {1}),
                       std::invalid_argument, "same size");
  CHECK_NOTHROW(d.SetDictionary({"@A"}, {"1 2"}, {1}, {1}));
}

TEST_CASE("reads", "a failed SetGrammar leaves the dictionary without grammar") {
  auto manager = parse(source);
  auto &d = manager("d");
  CHECK_THROWS_WITH(attach(d, {kw("@Int", SINGLE_INT)}),
                    "is not recognized");
  CHECK(!d.has_grammar());
}
