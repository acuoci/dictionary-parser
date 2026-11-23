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

#include "DictionaryFormats.h"
#include "DictionaryRules.h"
#include "DictionaryUnits.h"
#include "test_framework.h"
#include "test_support.h"

#include <array>
#include <cmath>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

using namespace OpenSMOKEpp;
using namespace OpenSMOKEpp::formats;
namespace dim = OpenSMOKEpp::units::dim;
using testing::attach;
using testing::kw;
using testing::parse;

namespace {
template <typename F> auto catch_error(F &&f) -> DictionaryError {
  try {
    f();
  } catch (const DictionaryError &error) {
    return error;
  }
  throw std::runtime_error("no DictionaryError was thrown");
}

auto close(double a, double b) -> bool {
  return std::abs(a - b) <= 1e-12 * std::max(1.0, std::abs(b));
}

auto connections() -> ValueFormat {
  return List(Record({{"from", Integer().Minimum(1)},
                      {"to", Integer().Minimum(1)},
                      {"flow", Quantity(dim::MassFlowRate)}}));
}

auto profile() -> ValueFormat {
  return Table({{"time", dim::Time}, {"temperature", dim::Temperature}});
}

auto species() -> ValueFormat {
  return Choice({Enumeration({"ALL", "NONE"}), List(Word())});
}

enum class Solver { OpenSMOKE, CVODE, DASPK };

auto solver() -> ValueFormat {
  return Enumeration({"OpenSMOKE++", "CVODE", "DASPK"})
      .Alias("OpenSMOKE", "OpenSMOKE++")
      .Alias("Cvode", "CVODE")
      .Disable("DASPK", "OpenSMOKE++ was built without DASPK");
}
} // namespace

// ---------------------------------------------------------------------------
// Structured values
// ---------------------------------------------------------------------------
TEST_CASE("formats", "lists of records with quantities") {
  auto manager = parse("Dictionary d { @Connections 3 7 250.3 kg/s  4 8 1.2 g/s; }");
  auto &d = manager("d");
  attach(d, {DictionaryKeyWord("@Connections", connections(), "links", true)});
  const auto links = d.Read<std::vector<std::tuple<int, int, double>>>("@Connections");
  REQUIRE(links.size() == 2);
  CHECK(std::get<0>(links[0]) == 3);
  CHECK(std::get<1>(links[0]) == 7);
  CHECK(close(std::get<2>(links[0]), 250.3));
  CHECK(close(std::get<2>(links[1]), 1.2e-3));
}

TEST_CASE("formats", "tables with a units header") {
  auto manager = parse("Dictionary d { @Profile min C  0 25  1 100; "
                       "@Plain 1 2 3 4; @Mixed - K  0.5 300; }");
  auto &d = manager("d");
  attach(d, {DictionaryKeyWord("@Profile", profile(), "T(t)", false),
             DictionaryKeyWord("@Plain", Table({{"x", {}}, {"y", {}}}), "", false),
             DictionaryKeyWord("@Mixed",
                               Table({{"z", {}}, {"temperature", dim::Temperature}}),
                               "", false)});
  const auto rows = d.Read<std::vector<std::array<double, 2>>>("@Profile");
  REQUIRE(rows.size() == 2);
  CHECK(close(rows[0][0], 0.0));
  CHECK(close(rows[0][1], 298.15));
  CHECK(close(rows[1][0], 60.0));
  CHECK(close(rows[1][1], 373.15));
  const auto plain = d.Read<std::vector<std::pair<double, double>>>("@Plain");
  CHECK(plain == (std::vector<std::pair<double, double>>{{1, 2}, {3, 4}}));
  const auto mixed = d.Read<std::vector<std::vector<double>>>("@Mixed");
  CHECK(mixed == (std::vector<std::vector<double>>{{0.5, 300}}));
}

TEST_CASE("formats", "choices map to std::variant") {
  auto manager = parse("Dictionary d { @A ALL; @B CH4 O2 CO2; }");
  auto &d = manager("d");
  attach(d, {DictionaryKeyWord("@A", species(), "", false),
             DictionaryKeyWord("@B", species(), "", false)});
  using Species = std::variant<std::string, std::vector<std::string>>;
  const auto a = d.Read<Species>("@A");
  REQUIRE(a.index() == 0);
  CHECK(std::get<0>(a) == "ALL");
  const auto b = d.Read<Species>("@B");
  REQUIRE(b.index() == 1);
  CHECK(std::get<1>(b) == (std::vector<std::string>{"CH4", "O2", "CO2"}));
  CHECK(d.ReadValue("@B").alternative == 1);
}

TEST_CASE("formats", "enumerations with aliases and disabled values") {
  auto manager = parse("Dictionary d { @A Cvode; @B OpenSMOKE; }");
  auto &d = manager("d");
  attach(d, {DictionaryKeyWord("@A", solver(), "", false),
             DictionaryKeyWord("@B", solver(), "", false)});
  CHECK(d.Read<Solver>("@A") == Solver::CVODE);
  CHECK(d.Read<std::string>("@A") == "CVODE"); // canonical spelling
  CHECK(d.Read<int>("@B") == 0);

  auto bad = parse("Dictionary d\n{\n@A DASPK;\n@B Cvod;\n}\n");
  const auto error = catch_error([&] {
    attach(bad("d"), {DictionaryKeyWord("@A", solver(), "", false),
                      DictionaryKeyWord("@B", solver(), "", false)});
  });
  REQUIRE(error.diagnostics().size() == 2);
  CHECK(error.diagnostics()[0].code == DiagnosticCode::NotAllowed);
  CHECK(error.diagnostics()[0].message ==
        "keyword @A: 'DASPK' is not available: OpenSMOKE++ was built without "
        "DASPK");
  CHECK(error.diagnostics()[1].message ==
        "keyword @B: 'Cvod' is not an allowed value (allowed: OpenSMOKE++, "
        "CVODE, DASPK)");
  CHECK(error.diagnostics()[1].hint == "did you mean 'Cvode'?");
  CHECK(error.diagnostics()[1].location.line == 4);
  CHECK(error.diagnostics()[1].location.column == 4);
}

TEST_CASE("formats", "errors name the element and the field") {
  auto manager = parse("Dictionary d\n{\n@Connections 3 7 1 kg/s  0 8 250 K;\n}\n");
  const auto error = catch_error([&] {
    attach(manager("d"),
           {DictionaryKeyWord("@Connections", connections(), "links", true)});
  });
  REQUIRE(error.diagnostics().size() == 2);
  CHECK(error.diagnostics()[0].message ==
        "keyword @Connections: element 2, field 'from': the value 0 must be >= 1");
  CHECK(error.diagnostics()[0].code == DiagnosticCode::OutOfRange);
  CHECK(error.diagnostics()[0].location.column == 26);
  CHECK(error.diagnostics()[1].message.starts_with(
      "keyword @Connections: element 2, field 'flow': units 'K' (temperature "
      "[K]) are not compatible with the expected mass flow rate [kg/s]"));
  CHECK(error.diagnostics()[1].code == DiagnosticCode::InvalidUnit);
  CHECK(error.diagnostics()[1].location.column == 34);
}

TEST_CASE("formats", "sizes, groups and quantity lists") {
  auto manager = parse("Dictionary d { @Ratios 1; @Pairs CH4 0.5 O2; "
                       "@Times 1 2 -3 s; @Rows s K 1 300 2; }");
  const auto error = catch_error([&] {
    attach(manager("d"),
           {DictionaryKeyWord("@Ratios", List(Real(), 2, 3), "", false),
            DictionaryKeyWord("@Pairs", Composition(), "", false),
            DictionaryKeyWord("@Times", QuantityList(dim::Time).Minimum(0), "", false),
            DictionaryKeyWord("@Rows", profile(), "", false)});
  });
  REQUIRE(error.diagnostics().size() == 4);
  CHECK(error.diagnostics()[0].message ==
        "keyword @Ratios: 1 element was given, but between 2 and 3 are required");
  CHECK(error.diagnostics()[1].message ==
        "keyword @Pairs: expected groups of 2 values ((name: word, value: "
        "number)), but 3 values were given");
  CHECK(error.diagnostics()[2].message ==
        "keyword @Times: the value -3 s (= -3 s) must be >= 0 s");
  CHECK(error.diagnostics()[3].message ==
        "keyword @Rows: expected one unit per column (time, temperature) "
        "followed by rows of 2 numbers, but 5 values were given");
}

TEST_CASE("formats", "records ending with free text") {
  auto manager = parse("Dictionary d { @Note 3 \"first part\" and more; }");
  auto &d = manager("d");
  attach(d, {DictionaryKeyWord(
                "@Note", Record({{"level", Integer()}, {"text", Text()}}), "", false)});
  const auto note = d.Read<std::pair<int, std::string>>("@Note");
  CHECK(note.first == 3);
  CHECK(note.second == "first part and more");
}

TEST_CASE("formats", "invalid formats are rejected with the grammar") {
  auto manager = parse("Dictionary d { @X 1; }");
  auto &d = manager("d");
  const auto rejected = [&](const ValueFormat &format) {
    return catch_error([&] {
             attach(d, {kw("@X", SINGLE_INT),
                        DictionaryKeyWord("@F", format, "", false)});
           })
        .diagnostics()
        .front()
        .message;
  };
  CHECK(rejected(List(Text())) ==
        "the format of @F is not valid: the elements of a list must have a "
        "fixed number of values, but text does not");
  CHECK(rejected(Record({{"a", Text()}, {"b", Integer()}})) ==
        "the format of @F is not valid: the field 'a' has a variable number "
        "of values, so it must be the last field of the record");
  CHECK(rejected(Word().Minimum(0)) ==
        "the format of @F is not valid: a minimum cannot be applied to a word");
  CHECK(rejected(Enumeration({})) ==
        "the format of @F is not valid: an enumeration needs at least one value");
  CHECK(rejected(Enumeration({"A", "A"})) ==
        "the format of @F is not valid: the enumeration has an empty or "
        "repeated value 'A'");
  CHECK(rejected(Enumeration({"A"}).Alias("B", "C")) ==
        "the format of @F is not valid: the alias 'B' refers to the unknown "
        "value 'C'");
  CHECK(rejected(Choice({Word()})) ==
        "the format of @F is not valid: a choice needs at least two alternatives");
  CHECK(rejected(Real().Minimum(2).Maximum(1)) ==
        "the format of @F is not valid: the range is empty");
  CHECK(rejected(Record({{"a", Word()}, {"a", Word()}})) ==
        "the format of @F is not valid: the record has an empty or repeated "
        "field name 'a'");
  CHECK_THROWS_WITH(
      attach(d, {kw("@X", SINGLE_INT),
                 DictionaryKeyWord("@F", Real(), "", false).SetMinimum(0)}),
      "use the format modifiers instead");
}

TEST_CASE("formats", "descriptions") {
  CHECK(connections().describe() ==
        "list of (from: integer >= 1, to: integer >= 1, flow: mass flow rate "
        "[kg/s])");
  CHECK(profile().describe() == "table of (time [s], temperature [K])");
  CHECK(species().describe() == "one of ALL | NONE, or list of word");
  CHECK(solver().describe() == "one of OpenSMOKE++ | CVODE | DASPK");
  CHECK(List(Real(), 2, 2).describe() == "list (2) of number");
  CHECK(QuantityList(dim::Concentration).describe() ==
        "list of concentration [kmol/m3]");
  CHECK(connections().width() == std::nullopt);
  CHECK(Record({{"a", Integer()}, {"b", Quantity(dim::Length)}}).width() == 3u);
}

TEST_CASE("formats", "defaults, conditions and help for structured keywords") {
  const std::vector<DictionaryKeyWord> keywords = {
      DictionaryKeyWord("@Species", species(), "output species", false)
          .SetDefault("ALL"),
      DictionaryKeyWord("@Solver", solver(), "ODE solver", false)
          .SetDefault("OpenSMOKE++"),
      kw("@Tolerance", SINGLE_DOUBLE)};
  const std::vector<Rule> rules = {
      rules::When(rules::Equals("@Solver", "CVODE"), rules::Required("@Tolerance"))};

  auto empty = parse("Dictionary d { @Tolerance 1e-6; }");
  auto &d = empty("d");
  attach(d, keywords, rules);
  CHECK(std::get<0>(d.Read<std::variant<std::string, std::vector<std::string>>>(
            "@Species")) == "ALL");
  CHECK(d.Read<Solver>("@Solver") == Solver::OpenSMOKE);

  auto alias = parse("Dictionary d { @Solver Cvode; }"); // alias of CVODE
  const auto error = catch_error([&] { attach(alias("d"), keywords, rules); });
  CHECK(error.diagnostics().front().message ==
        "the compulsory keyword @Tolerance is missing (when @Solver is CVODE)");

  auto bad_default = parse("Dictionary d { @Tolerance 1e-6; }");
  CHECK_THROWS_WITH(
      attach(bad_default("d"),
             {DictionaryKeyWord("@Solver", solver(), "", false).SetDefault("RK4"),
              kw("@Tolerance", SINGLE_DOUBLE)}),
      "the default value 'RK4' of @Solver is not valid: 'RK4' is not an "
      "allowed value");

  auto help = parse("Dictionary d { @Help; }");
  CHECK_THROWS_WITH(attach(help("d"), keywords, rules),
                    "(format: one of OpenSMOKE++ | CVODE | DASPK)");
}

// ---------------------------------------------------------------------------
// Typed reads for every keyword type
// ---------------------------------------------------------------------------
TEST_CASE("typed", "Read<T> on legacy keyword types") {
  auto manager = parse("Dictionary d\n{\n"
                       "@N 42; @X 2.5; @Flag on; @Name CH4; @Path \"a b/c.txt\";\n"
                       "@Values 1 2 3; @Moles N2 0.79 O2 0.21; @Raw 300 K;\n"
                       "@T 25 C; @Times 1 2 min; @Indexed 1 a 2 b; @Negative -1;\n"
                       "}\n");
  auto &d = manager("d");
  attach(d, {kw("@N", SINGLE_INT), kw("@X", SINGLE_DOUBLE), kw("@Flag", SINGLE_BOOL),
             kw("@Name", SINGLE_STRING), kw("@Path", SINGLE_PATH),
             kw("@Values", VECTOR_DOUBLE), kw("@Moles", VECTOR_STRING_DOUBLE),
             kw("@Raw", SINGLE_MEASURE),
             kw("@T", SINGLE_MEASURE).SetDimension(dim::Temperature),
             kw("@Times", VECTOR_MEASURE).SetDimension(dim::Time),
             kw("@Indexed", VECTOR_INT_STRING), kw("@Negative", SINGLE_INT),
             kw("@Default", SINGLE_INT).SetDefault("7")});
  CHECK(d.Read<int>("@N") == 42);
  CHECK(d.Read<long long>("@N") == 42);
  CHECK(d.Read<double>("@N") == 42.0); // integers convert to floating point
  CHECK(d.Read<float>("@X") == 2.5f);
  CHECK(d.Read<bool>("@Flag"));
  CHECK(d.Read<std::string>("@Name") == "CH4");
  CHECK(d.Read<std::string>("@Path") == "a b/c.txt");
  CHECK(d.Read<std::vector<double>>("@Values") == (std::vector<double>{1, 2, 3}));
  const auto moles = d.Read<std::vector<std::pair<std::string, double>>>("@Moles");
  CHECK(moles == (std::vector<std::pair<std::string, double>>{{"N2", 0.79},
                                                              {"O2", 0.21}}));
  const auto raw = d.Read<std::pair<double, std::string>>("@Raw");
  CHECK(raw.first == 300.0);
  CHECK(raw.second == "K");
  CHECK(close(d.Read<double>("@T"), 298.15));
  CHECK(d.Read<std::vector<double>>("@Times") == (std::vector<double>{60, 120}));
  const auto indexed = d.Read<std::vector<std::pair<int, std::string>>>("@Indexed");
  CHECK(indexed.at(1).second == "b");
  CHECK(d.Read<int>("@Default") == 7);
}

TEST_CASE("typed", "shape mismatches are explicit") {
  auto manager = parse("Dictionary d { @Name CH4; @Negative -1; @Big 3000000000; }");
  auto &d = manager("d");
  attach(d, {kw("@Name", SINGLE_STRING), kw("@Negative", SINGLE_INT),
             DictionaryKeyWord("@Big", Integer(), "", false)});
  const auto error = catch_error([&] { (void)d.Read<int>("@Name"); });
  CHECK(error.has(DiagnosticCode::WrongType));
  CHECK(error.diagnostics().front().message ==
        "the value of @Name cannot be read as the requested C++ type: expected "
        "an integer, but the value is a text (the value is word)");
  CHECK_THROWS_WITH((void)d.Read<unsigned int>("@Negative"),
                    "the integer -1 does not fit the requested type");
  CHECK_THROWS_WITH((void)d.Read<int>("@Big"),
                    "the integer 3000000000 does not fit the requested type");
  CHECK(d.Read<long long>("@Big") == 3000000000LL);
  CHECK_THROWS_WITH((void)d.Read<std::vector<int>>("@Name"),
                    "expected a sequence, but the value is a text");
  using Two = std::array<int, 2>;
  CHECK_THROWS_WITH((void)d.Read<Two>("@Negative"),
                    "expected a sequence of 2 items");
}

TEST_CASE("typed", "TryRead<T> error codes") {
  auto manager = parse("Dictionary d { @N 1; @S CVODE; }");
  auto &d = manager("d");
  CHECK(d.TryRead<int>("@N").error().code == Dictionary::ParseErrorCode::NoGrammar);
  attach(d, {kw("@N", SINGLE_INT), kw("@M", SINGLE_INT),
             DictionaryKeyWord("@S", solver(), "", false)});
  CHECK(d.TryRead<int>("@N").value() == 1);
  CHECK(d.TryRead<int>("@Q").error().code ==
        Dictionary::ParseErrorCode::UndeclaredKeyword);
  CHECK(d.TryRead<int>("@M").error().code ==
        Dictionary::ParseErrorCode::MissingKeyword);
  CHECK(d.TryRead<std::string>("@N").error().code ==
        Dictionary::ParseErrorCode::WrongType);
}

TEST_CASE("typed", "ReadValue and usage tracking") {
  auto manager = parse("Dictionary d { @S Cvode; @Unread 1; }");
  auto &d = manager("d");
  attach(d, {DictionaryKeyWord("@S", solver(), "", false), kw("@Unread", SINGLE_INT)});
  const Value value = d.ReadValue("@S");
  CHECK(value.enumeration);
  CHECK(value.integer == 1);
  CHECK(value.text == "CVODE");
  CHECK(d.UnusedKeywords() == (std::vector<std::string>{"@Unread"}));
}
