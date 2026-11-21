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
                           "  @Odd      CH4 0.5 O2;\n"
                           "  @Measures 1 m 2 cm;\n"
                           "  @IntNames 1 a 2 b;\n"
                           "  @Bools    on false;\n"
                           "  @Char     x;\n"
                           "  @BadInt   4.2;\n"
                           "  @BadBool  maybe;\n"
                           "  @TwoDoubles 1 2;\n"
                           "  @NaN      nan;\n"
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

  int i = 0;
  d.ReadInt("@Int", i);
  CHECK(i == 42);
  double x = 0;
  d.ReadDouble("@Double", x);
  CHECK(x == 2.5e-3);
  bool flag = false;
  d.ReadBool("@Bool", flag);
  CHECK(flag);
  d.ReadBool("@Off", flag);
  CHECK(!flag);
  std::string units;
  d.ReadMeasure("@Measure", x, units);
  CHECK(x == 300.0);
  CHECK(units == "K");
  std::vector<int> ints;
  d.ReadOption("@Ints", ints);
  CHECK(ints == (std::vector<int>{1, 2, 3}));
  std::vector<double> doubles;
  d.ReadOption("@Doubles", doubles);
  CHECK(doubles == (std::vector<double>{1, 2.5, -300}));
  std::vector<std::string> names;
  d.ReadOption("@Pairs", names, doubles);
  CHECK(names == (std::vector<std::string>{"CH4", "O2"}));
  CHECK(doubles == (std::vector<double>{0.5, 0.5}));
  d.ReadOption("@Measures", doubles, names);
  CHECK(doubles == (std::vector<double>{1, 2}));
  CHECK(names == (std::vector<std::string>{"m", "cm"}));
  d.ReadOption("@IntNames", ints, names);
  CHECK(ints == (std::vector<int>{1, 2}));
  CHECK(names == (std::vector<std::string>{"a", "b"}));
  std::vector<bool> bools;
  d.ReadOption("@Bools", bools);
  CHECK(bools == (std::vector<bool>{true, false}));
  char c = 0;
  d.ReadChar("@Char", c);
  CHECK(c == 'x');
  CHECK(d.CheckOption("@Int"));
  CHECK(!d.CheckOption("@Absent"));
  CHECK(d.has_grammar());
}

TEST_CASE("reads", "value errors name the line and the keyword") {
  auto manager = parse(source);
  auto &d = manager("d");
  attach(d, grammar());
  int i = 0;
  bool flag = false;
  double x = 0;
  std::vector<std::string> names;
  std::vector<double> values;
  CHECK_THROWS_WITH(d.ReadInt("@BadInt", i),
                    "line 16, keyword @BadInt: an integer is required, but "
                    "'4.2' was given");
  CHECK_THROWS_WITH(d.ReadBool("@BadBool", flag),
                    "a boolean (true/false/on/off) is required, but 'maybe'");
  CHECK_THROWS_WITH(d.ReadDouble("@TwoDoubles", x),
                    "line 18: the keyword @TwoDoubles requires exactly one "
                    "number (double), but 2 values were given");
  CHECK_THROWS_WITH(d.ReadDouble("@NaN", x), "requires a finite number");
  CHECK_THROWS_WITH(d.ReadOption("@Odd", names, values),
                    "pairs of a name and a number (double) are required");
}

TEST_CASE("reads", "programming errors are explicit") {
  auto manager = parse(source);
  auto &d = manager("d");
  double x = 0;
  CHECK_THROWS_WITH(d.ReadDouble("@Double", x),
                    "cannot read keyword @Double: no grammar is attached to "
                    "this dictionary");
  attach(d, grammar());
  CHECK_THROWS_WITH(d.ReadDouble("@Typo", x),
                    "the keyword @Typo is not declared in the grammar");
  CHECK_THROWS_WITH(d.ReadDouble("@Measure", x),
                    "the keyword @Measure is declared as 'single_measure' in "
                    "the grammar (read it with ReadMeasure), but it was read "
                    "as 'single_double'");
  CHECK_THROWS_WITH(d.ReadDouble("@Absent", x),
                    "the required keyword @Absent is not present");
}

TEST_CASE("reads", "TryReadDouble error codes") {
  auto manager = parse(source);
  auto &d = manager("d");
  CHECK(d.TryReadDouble("@Double").error().code ==
        Dictionary::ParseErrorCode::NoGrammar);
  attach(d, grammar());
  CHECK(d.TryReadDouble("@Double").value() == 2.5e-3);
  CHECK(d.TryReadDouble("@Typo").error().code ==
        Dictionary::ParseErrorCode::UndeclaredKeyword);
  CHECK(d.TryReadDouble("@Int").error().code ==
        Dictionary::ParseErrorCode::WrongType);
  CHECK(d.TryReadDouble("@Absent").error().code ==
        Dictionary::ParseErrorCode::MissingKeyword);
  CHECK(d.TryReadDouble("@TwoDoubles").error().code ==
        Dictionary::ParseErrorCode::BadTokenCount);
  CHECK(d.TryReadDouble("@TwoDoubles").error().line == 18);
  CHECK(d.TryReadDouble("@NaN").error().code ==
        Dictionary::ParseErrorCode::BadNumber);
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
