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

#include "DictionaryRules.h"
#include "DictionaryUnits.h"
#include "test_framework.h"
#include "test_support.h"

#include <cmath>
#include <string>
#include <vector>

using namespace OpenSMOKEpp;
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

auto first_with(const DictionaryError &error, DiagnosticCode code)
    -> const Diagnostic * {
  for (const auto &d : error.diagnostics())
    if (d.code == code)
      return &d;
  return nullptr;
}

auto state_grammar() -> std::vector<DictionaryKeyWord> {
  return {kw("@T", SINGLE_MEASURE), kw("@P", SINGLE_MEASURE),
          kw("@Rho", SINGLE_MEASURE)};
}
} // namespace

// ---------------------------------------------------------------------------
// Rule evaluation
// ---------------------------------------------------------------------------
TEST_CASE("rules", "exactly n of a group") {
  const std::vector<Rule> rules = {rules::Exactly(2, {"@T", "@P", "@Rho"})};
  {
    auto manager = parse("Dictionary d { @T 300 K; @P 1 atm; }");
    CHECK_NOTHROW(attach(manager("d"), state_grammar(), rules));
  }
  {
    auto manager = parse("Dictionary d { @T 300 K; }");
    const auto error =
        catch_error([&] { attach(manager("d"), state_grammar(), rules); });
    REQUIRE(error.diagnostics().size() == 1);
    const Diagnostic &d = error.diagnostics().front();
    CHECK(d.code == DiagnosticCode::Cardinality);
    CHECK(d.message == "exactly 2 of @T, @P, @Rho must be specified, but "
                       "only 1 is present (@T)");
    CHECK(d.location.column == 12); // the dictionary name
  }
  {
    auto manager =
        parse("Dictionary d\n{\n@T 300 K;\n@P 1 atm;\n@Rho 1 kg/m3;\n}\n");
    const auto error =
        catch_error([&] { attach(manager("d"), state_grammar(), rules); });
    const Diagnostic &d = error.diagnostics().front();
    CHECK(d.message == "exactly 2 of @T, @P, @Rho must be specified, but 3 "
                       "are present (@T, @P, @Rho)");
    CHECK(d.location.line == 5);
    CHECK(d.notes.size() == 2);
  }
}

TEST_CASE("rules", "at least and at most") {
  auto none = parse("Dictionary d { @X 1; }");
  const auto too_few = catch_error([&] {
    attach(none("d"), {kw("@X", SINGLE_INT), kw("@A", SINGLE_INT), kw("@B", SINGLE_INT)},
           {rules::AtLeastOneOf({"@A", "@B"})});
  });
  CHECK(too_few.diagnostics().front().message ==
        "at least one of @A, @B must be specified, but none is present");

  auto both = parse("Dictionary d { @A 1; @B 2; }");
  const auto too_many = catch_error([&] {
    attach(both("d"), {kw("@A", SINGLE_INT), kw("@B", SINGLE_INT)},
           {rules::AtMostOneOf({"@A", "@B"})});
  });
  CHECK(too_many.diagnostics().front().message ==
        "at most one of @A, @B can be specified, but 2 are present (@A, @B)");
  CHECK(too_many.diagnostics().front().code == DiagnosticCode::Cardinality);
}

TEST_CASE("rules", "conflicts are declared once and are symmetric") {
  auto manager = parse("Dictionary d\n{\n@Rich true;\n@Lean true;\n}\n");
  const auto error = catch_error([&] {
    attach(manager("d"), {kw("@Lean", SINGLE_BOOL), kw("@Rich", SINGLE_BOOL)},
           {rules::Conflicts("@Lean", {"@Rich"})});
  });
  const Diagnostic &d = error.diagnostics().front();
  CHECK(d.code == DiagnosticCode::Conflict);
  CHECK(d.message == "the keywords @Lean and @Rich are mutually exclusive and "
                     "cannot be used together");
  CHECK(d.location.line == 4); // the later one
  REQUIRE(d.notes.size() == 1);
  CHECK(d.notes[0].location.line == 3);
}

TEST_CASE("rules", "requires any of") {
  auto manager = parse("Dictionary d { @Fuel CH4; }");
  const auto error = catch_error([&] {
    attach(manager("d"),
           {kw("@Fuel", SINGLE_STRING), kw("@Oxidizer", SINGLE_STRING),
            kw("@Air", SINGLE_BOOL)},
           {rules::RequiresAnyOf("@Fuel", {"@Oxidizer", "@Air"})});
  });
  CHECK(error.diagnostics().front().message ==
        "the keyword @Fuel requires one of @Oxidizer, @Air, but none is "
        "present");
  CHECK(error.diagnostics().front().code == DiagnosticCode::MissingRequired);
}

TEST_CASE("rules", "conditional rules on values") {
  const std::vector<DictionaryKeyWord> keywords = {
      kw("@Type", SINGLE_STRING).SetAllowedValues({"Constant", "Variable"}),
      kw("@Profile", VECTOR_DOUBLE), kw("@Value", SINGLE_DOUBLE)};
  const std::vector<Rule> rules = {
      rules::When(rules::Equals("@Type", "Variable"), rules::Required("@Profile")),
      rules::When(rules::Equals("@Type", "Variable"), rules::Forbidden("@Value"))};

  auto ok = parse("Dictionary d { @Type Constant; @Value 1; }");
  CHECK_NOTHROW(attach(ok("d"), keywords, rules));

  auto bad = parse("Dictionary d\n{\n@Type Variable;\n@Value 1;\n}\n");
  const auto error = catch_error([&] { attach(bad("d"), keywords, rules); });
  REQUIRE(error.diagnostics().size() == 2);
  const Diagnostic *missing = first_with(error, DiagnosticCode::MissingCompulsory);
  REQUIRE(missing != nullptr);
  CHECK(missing->message ==
        "the compulsory keyword @Profile is missing (when @Type is Variable)");
  REQUIRE(missing->notes.size() == 1);
  CHECK(missing->notes[0].message == "@Type is specified here");
  CHECK(missing->notes[0].location.line == 3);
  const Diagnostic *forbidden = first_with(error, DiagnosticCode::Forbidden);
  REQUIRE(forbidden != nullptr);
  CHECK(forbidden->message ==
        "the keyword @Value cannot be used (when @Type is Variable)");
  CHECK(forbidden->location.line == 4);
}

TEST_CASE("rules", "conditions use defaults and normalize booleans") {
  const std::vector<DictionaryKeyWord> keywords = {
      kw("@Type", SINGLE_STRING).SetDefault("Variable"),
      kw("@Profile", VECTOR_DOUBLE), kw("@Debug", SINGLE_BOOL),
      kw("@Level", SINGLE_INT)};
  const std::vector<Rule> rules = {
      rules::When(rules::Equals("@Type", "Variable"), rules::Required("@Profile")),
      rules::When(rules::Equals("@Debug", "on"), rules::Required("@Level")),
      rules::When(rules::OneOf("@Type", {"A", "B"}), rules::Forbidden("@Profile")),
      rules::When(rules::Absent("@Profile"), rules::Forbidden("@Level")),
      rules::When(rules::Present("@Level"), rules::Required("@Debug"))};

  auto missing = parse("Dictionary d { @Debug true; }");
  const auto error = catch_error([&] { attach(missing("d"), keywords, rules); });
  CHECK(error.error_count() == 2);
  CHECK(std::string(error.what()).find("@Profile is missing (when @Type is "
                                       "Variable)") != std::string::npos);
  CHECK(std::string(error.what()).find("@Level is missing (when @Debug is "
                                       "on)") != std::string::npos);

  auto fine = parse("Dictionary d { @Debug TRUE; @Level 2; @Profile 1 2; }");
  CHECK_NOTHROW(attach(fine("d"), keywords, rules));

  auto absent = parse("Dictionary d { @Type A; @Level 2; @Debug on; }");
  const auto forbidden = catch_error([&] { attach(absent("d"), keywords, rules); });
  CHECK(forbidden.diagnostics().front().message ==
        "the keyword @Level cannot be used (when @Profile is not specified)");
}

TEST_CASE("rules", "legacy declarations and new rules work together") {
  auto manager = parse("Dictionary d { @A 1; @C 3; }");
  const auto error = catch_error([&] {
    attach(manager("d"),
           {kw("@A", SINGLE_INT, true, "@B", "none", "none"),
            kw("@B", SINGLE_INT, true, "@A", "none", "none"),
            kw("@C", SINGLE_INT), kw("@D", SINGLE_INT)},
           {rules::Requires("@C", {"@D"})});
  });
  CHECK(error.diagnostics().size() == 1);
  CHECK(error.diagnostics().front().message ==
        "the keyword @C requires @D, which is missing");
}

TEST_CASE("rules", "rules() lists legacy and new rules") {
  struct Exposed : testing::RuleGrammar {
    using RuleGrammar::RuleGrammar;
    const std::vector<Rule> &build() {
      UserDefined();
      return rules();
    }
  };
  Exposed grammar(
      {kw("@A", SINGLE_INT, true),
       kw("@B", SINGLE_INT, false, "none", "none", "@C"),
       kw("@C", SINGLE_INT, false, "none", "none", "@B")},
      {rules::Requires("@B", {"@A"})});
  const auto &all = grammar.build();
  REQUIRE(all.size() == 3);
  CHECK(all[0].describe() == "@A is required");          // legacy compulsory
  CHECK(all[1].describe() == "@B cannot be used with @C"); // legacy, deduplicated
  CHECK(all[2].describe() == "@B requires @A");          // AddRule

  CHECK(rules::When(rules::OneOf("@T", {"x", "y"}), rules::Forbidden("@Z"))
            .describe() == "@Z is not allowed when @T is x or y");
  CHECK(rules::Exactly(2, {"@A", "@B", "@C"}).describe() ==
        "exactly 2 of @A, @B, @C");
  CHECK(rules::Conflicts("@A", {"@B", "@C"}).describe() ==
        "@A cannot be used with @B or @C");
  CHECK(rules::When(rules::Absent("@Q"), rules::Required("@R")).describe() ==
        "@R is required when @Q is not specified");
}

// ---------------------------------------------------------------------------
// Coherence of rules
// ---------------------------------------------------------------------------
TEST_CASE("rules", "incoherent rules are rejected") {
  auto manager = parse("Dictionary d { @A 1; }");
  auto &d = manager("d");
  const std::vector<DictionaryKeyWord> three = {
      kw("@A", SINGLE_INT), kw("@B", SINGLE_INT), kw("@C", VECTOR_INT)};
  CHECK_THROWS_WITH(attach(d, three, {rules::Requires("@A", {"@Z"})}),
                    "the rule '@A requires @Z' refers to the undefined keyword @Z");
  CHECK_THROWS_WITH(attach(d, three, {rules::Exactly(4, {"@A", "@B", "@C"})}),
                    "can never be satisfied: the count must be between 1 and 3");
  CHECK_THROWS_WITH(attach(d, three, {rules::AtMost(3, {"@A", "@B", "@C"})}),
                    "has no effect or forbids every keyword");
  CHECK_THROWS_WITH(
      attach(d, three, {rules::When(rules::Present("@Z"), rules::Required("@B"))}),
      "has a condition on the undefined keyword @Z");
  CHECK_THROWS_WITH(
      attach(d, three, {rules::When(rules::Equals("@C", "1"), rules::Required("@B"))}),
      "compares the value of @C, whose type (vector_int) is not a single");
  CHECK_THROWS_WITH(
      attach(d,
             {kw("@A", SINGLE_INT),
              kw("@T", SINGLE_STRING).SetAllowedValues({"X", "Y"}),
              kw("@B", SINGLE_INT)},
             {rules::When(rules::Equals("@T", "Z"), rules::Required("@B"))}),
      "compares @T with 'Z', which is not a valid value: 'Z' is not an "
      "allowed value (allowed: X, Y)");
  CHECK_THROWS_WITH(attach(d, three, {rules::Requires("@A", {"@A", "@B"})}),
                    "lists @A more than once");
}

TEST_CASE("rules", "unsatisfiable rule sets are rejected") {
  auto manager = parse("Dictionary d { @A 1; }");
  auto &d = manager("d");
  const std::vector<DictionaryKeyWord> three = {
      kw("@A", SINGLE_INT), kw("@B", SINGLE_INT), kw("@C", SINGLE_INT)};
  CHECK_THROWS_WITH(attach(d, three, {rules::Required("@A"), rules::Forbidden("@A")}),
                    "the grammar can never be satisfied: @A is always "
                    "required, but a rule forbids it");
  CHECK_THROWS_WITH(
      attach(d, three,
             {rules::Required("@A"), rules::Conflicts("@A", {"@B", "@C"}),
              rules::Exactly(2, {"@A", "@B", "@C"})}),
      "the rule 'exactly 2 of @A, @B, @C' needs 2 keywords, but only 1 of "
      "them can be used");
  CHECK_THROWS_WITH(
      attach(d, three,
             {rules::Required("@A"), rules::Required("@B"), rules::Required("@C"),
              rules::AtMost(2, {"@A", "@B", "@C"})}),
      "the rule 'at most 2 of @A, @B, @C' allows 2, but 3 of its keywords are "
      "always required");
  CHECK_THROWS_WITH(
      attach(d, three,
             {rules::Requires("@B", {"@C"}), rules::Conflicts("@C", {"@B"})}),
      "the keyword @B can never be used: it requires @C, which conflicts with "
      "it");
}

// ---------------------------------------------------------------------------
// Keyword semantics: defaults, allowed values, ranges, sizes, sums
// ---------------------------------------------------------------------------
TEST_CASE("semantics", "defaults are returned when the keyword is absent") {
  auto manager = parse("Dictionary d { @Schmidt 40; }");
  auto &d = manager("d");
  attach(d, {kw("@Schmidt", SINGLE_DOUBLE).SetDefault("50"),
             kw("@Density", SINGLE_MEASURE)
                 .SetDimension(units::dim::Density)
                 .SetDefault("1.8 g/cm3"),
             kw("@Debug", SINGLE_BOOL).SetDefault("false"),
             kw("@Model", SINGLE_STRING).SetDefault("HMOM"),
             kw("@Moles", VECTOR_STRING_DOUBLE).SetDefault("N2 0.79 O2 0.21"),
             kw("@Title", SINGLE_STRING).SetDefault("\"my flame\"")});
  CHECK(d.Read<double>("@Schmidt") == 40.0); // file value wins
  CHECK(std::abs(d.Read<double>("@Density") - 1800.0) < 1e-9);
  CHECK(!d.Read<bool>("@Debug"));
  CHECK(d.Read<std::string>("@Model") == "HMOM");
  CHECK(d.Read<std::string>("@Title") == "my flame");
  using Pairs = std::vector<std::pair<std::string, double>>;
  CHECK(d.Read<Pairs>("@Moles") == (Pairs{{"N2", 0.79}, {"O2", 0.21}}));
  CHECK(!d.CheckOption("@Density")); // only what is written in the file
  CHECK(d.CheckOption("@Schmidt"));
  CHECK(d.entries().back().from_default);
}

TEST_CASE("semantics", "invalid defaults are rejected with the grammar") {
  auto manager = parse("Dictionary d { @X 1; }");
  auto &d = manager("d");
  CHECK_THROWS_WITH(attach(d, {kw("@X", SINGLE_INT), kw("@N", SINGLE_INT).SetDefault("abc")}),
                    "the default value 'abc' of @N is not valid: the value "
                    "must be an integer");
  CHECK_THROWS_WITH(
      attach(d, {kw("@X", SINGLE_INT),
                 kw("@V", SINGLE_MEASURE)
                     .SetDimension(units::dim::Velocity)
                     .SetDefault("5 K")}),
      "the default value '5 K' of @V is not valid: units 'K'");
  CHECK_THROWS_WITH(
      attach(d, {kw("@X", SINGLE_INT), kw("@P", SINGLE_DOUBLE).SetMinimum(0).SetDefault("-1")}),
      "the default value '-1' of @P is not valid: the value -1 must be >= 0");
  CHECK_THROWS_WITH(attach(d, {kw("@X", SINGLE_INT, true).SetDefault("1")}),
                    "the keyword @X is compulsory, so it cannot have a default "
                    "value");
  CHECK_THROWS_WITH(
      attach(d, {kw("@X", SINGLE_INT).SetDefault("1")}, {rules::Required("@X")}),
      "the keyword @X is compulsory, so it cannot have a default value");
}

TEST_CASE("semantics", "allowed values") {
  auto manager = parse("Dictionary d\n{\n@Planck Smoke;\n@Species CH4 XX;\n}\n");
  const auto error = catch_error([&] {
    attach(manager("d"),
           {kw("@Planck", SINGLE_STRING).SetAllowedValues({"Smooke", "Kent", "none"}),
            kw("@Species", VECTOR_STRING).SetAllowedValues({"CH4", "O2"})});
  });
  REQUIRE(error.diagnostics().size() == 2);
  const Diagnostic &planck = error.diagnostics()[0];
  CHECK(planck.code == DiagnosticCode::NotAllowed);
  CHECK(planck.message == "keyword @Planck: 'Smoke' is not an allowed value "
                          "(allowed: Smooke, Kent, none)");
  CHECK(planck.hint == "did you mean 'Smooke'?");
  CHECK(planck.location.column == 9);
  CHECK(error.diagnostics()[1].location.column == 14);

  auto other = parse("Dictionary d { @N 1; }");
  CHECK_THROWS_WITH(
      attach(other("d"), {kw("@N", SINGLE_INT).SetAllowedValues({"1", "2"})}),
      "declares allowed values, but only single_string and vector_string");
}

TEST_CASE("semantics", "numeric ranges, also in SI for measures") {
  auto manager = parse("Dictionary d\n{\n@Eps 0;\n@T 5 C;\n@Points 3 0 7;\n}\n");
  const auto error = catch_error([&] {
    attach(manager("d"),
           {kw("@Eps", SINGLE_DOUBLE).SetMinimum(0, false),
            kw("@T", SINGLE_MEASURE)
                .SetDimension(units::dim::Temperature)
                .SetMinimum(300),
            kw("@Points", VECTOR_INT).SetMinimum(1).SetMaximum(5)});
  });
  REQUIRE(error.diagnostics().size() == 4);
  CHECK(error.diagnostics()[0].message == "keyword @Eps: the value 0 must be > 0");
  CHECK(error.diagnostics()[3].message ==
        "keyword @Points: the value 7 must be <= 5");
  CHECK(error.diagnostics()[1].message ==
        "keyword @T: the value 5 C (= 278.15 K) must be >= 300 K");
  CHECK(error.diagnostics()[1].location.column == 4);
  CHECK(error.diagnostics()[2].message ==
        "keyword @Points: the value 0 must be >= 1");
  CHECK(error.diagnostics()[2].location.column == 11);
  for (const auto &d : error.diagnostics())
    CHECK(d.code == DiagnosticCode::OutOfRange);

  auto other = parse("Dictionary d { @X 1; }");
  CHECK_THROWS_WITH(attach(other("d"), {kw("@X", SINGLE_INT),
                                        kw("@M", SINGLE_MEASURE).SetMinimum(0)}),
                    "which requires a dimension");
  CHECK_THROWS_WITH(attach(other("d"), {kw("@X", SINGLE_INT),
                                        kw("@S", SINGLE_STRING).SetMinimum(0)}),
                    "declares a numeric range, but its type (single_string)");
  CHECK_THROWS_WITH(attach(other("d"), {kw("@X", SINGLE_INT).SetMinimum(2).SetMaximum(1)}),
                    "has an empty range");
}

TEST_CASE("semantics", "list sizes and sums") {
  auto manager = parse("Dictionary d { @Ratio 1 2; @Moles CH4 0.5 O2 0.4; }");
  const auto error = catch_error([&] {
    attach(manager("d"),
           {kw("@Ratio", VECTOR_DOUBLE).SetSize(1),
            kw("@Moles", VECTOR_STRING_DOUBLE).SetSize(1, 10).SetSum(1.0, 1e-6)});
  });
  REQUIRE(error.diagnostics().size() == 2);
  CHECK(error.diagnostics()[0].message ==
        "keyword @Ratio: 2 elements were given, but exactly 1 are required");
  CHECK(error.diagnostics()[1].message ==
        "keyword @Moles: the values add up to 0.9, but they must add up to 1 "
        "(tolerance 1e-06)");

  auto other = parse("Dictionary d { @X 1; }");
  CHECK_THROWS_WITH(attach(other("d"), {kw("@X", SINGLE_INT).SetSize(1)}),
                    "declares a list size, but its type (single_int) is not a list");
  CHECK_THROWS_WITH(attach(other("d"), {kw("@X", SINGLE_INT),
                                        kw("@V", VECTOR_INT).SetSum(1)}),
                    "declares a sum, but only vector_double and "
                    "vector_string_double");
}

TEST_CASE("semantics", "deprecated keywords produce warnings") {
  auto manager = parse("Dictionary d { @OldName 1; }");
  auto &d = manager("d");
  attach(d, {kw("@OldName", SINGLE_INT).SetDeprecated("use @NewName"),
             kw("@NewName", SINGLE_INT)});
  REQUIRE(d.warnings().size() == 1);
  CHECK(d.warnings()[0].severity == Severity::Warning);
  CHECK(d.warnings()[0].code == DiagnosticCode::Deprecated);
  CHECK(d.warnings()[0].message == "the keyword @OldName is deprecated");
  CHECK(d.warnings()[0].hint == "use @NewName");
  CHECK(format_diagnostic(d.warnings()[0]).starts_with(
      "test.dic:1:16: warning: the keyword @OldName is deprecated"));

  manager.SetWarningsAsErrors(true);
  const auto error = catch_error([&] {
    attach(d, {kw("@OldName", SINGLE_INT).SetDeprecated("use @NewName"),
               kw("@NewName", SINGLE_INT)});
  });
  CHECK(error.has(DiagnosticCode::Deprecated));
  CHECK(error.error_count() == 1);
}

TEST_CASE("semantics", "keywords never read are reported") {
  auto manager = parse("Dictionary d\n{\n@A 1;\n@B 2;\n@C 3;\n}\n"
                       "Dictionary e { @X 1; }");
  auto &d = manager("d");
  attach(d, {kw("@A", SINGLE_INT), kw("@B", SINGLE_INT), kw("@C", SINGLE_INT),
             kw("@D", SINGLE_INT).SetDefault("4")});
  attach(manager("e"), {kw("@X", SINGLE_INT)});
  (void)d.Read<int>("@A");
  (void)d.Read<int>("@D"); // default: never "unused"
  (void)d.CheckOption("@B"); // a presence test is not a read
  CHECK(d.UnusedKeywords() == (std::vector<std::string>{"@B", "@C"}));

  const auto warnings = d.CheckUnusedKeywords();
  REQUIRE(warnings.size() == 2);
  CHECK(warnings[0].code == DiagnosticCode::UnusedKeyword);
  CHECK(warnings[0].severity == Severity::Warning);
  CHECK(warnings[0].message == "the keyword @B is never read by the program");
  CHECK(warnings[0].location.line == 4);

  CHECK(manager.CheckUnusedKeywords().size() == 3); // includes e/@X

  manager.SetWarningsAsErrors(true);
  CHECK_THROWS_WITH((void)d.CheckUnusedKeywords(), "2 errors found");
  CHECK_THROWS_WITH((void)manager.CheckUnusedKeywords(), "3 errors found");
}

TEST_CASE("semantics", "the @Help table shows defaults, allowed values and rules") {
  testing::RuleGrammar grammar(
      {kw("@T", SINGLE_MEASURE, true).SetDimension(units::dim::Temperature),
       kw("@Model", SINGLE_STRING).SetAllowedValues({"A", "B"}).SetDefault("A"),
       kw("@Eps", SINGLE_DOUBLE).SetDeprecated("use @Epsilon"),
       kw("@X", VECTOR_DOUBLE)},
      {rules::When(rules::Equals("@Model", "B"), rules::Required("@X"))});
  auto manager = parse("Dictionary flame { @T 300 K; @Help; }");
  const auto error = catch_error([&] { manager("flame").SetGrammar(grammar); });
  const std::string what = error.what();
  CHECK(error.has(DiagnosticCode::HelpRequested));
  CHECK(what.find("(default: A) (allowed: A B)") != std::string::npos);
  CHECK(what.find("(deprecated)") != std::string::npos);
  CHECK(what.find("Rules\n  - @T is required\n  - @X is required when @Model "
                  "is B") != std::string::npos);
}

TEST_CASE("semantics", "Describe output") {
  class Documented : public DictionaryGrammar {
  public:
    std::string run() {
      UserDefined();
      return Describe("Flame");
    }

  protected:
    void DefineRules() override {
      AddKeyWord(DictionaryKeyWord("@T", SINGLE_MEASURE, "Temperature", true)
                     .SetDimension(units::dim::Temperature));
      AddKeyWord(DictionaryKeyWord("@Model", SINGLE_STRING, "Model | variant", false)
                     .SetAllowedValues({"A", "B"})
                     .SetDefault("A"));
      AddKeyWord(DictionaryKeyWord("@X", VECTOR_DOUBLE, "Fractions", false)
                     .SetSize(2, 4)
                     .SetSum(1));
      AddRule(rules::When(rules::Equals("@Model", "B"), rules::Required("@X")));
    }
  };
  Documented grammar;
  const std::string text = grammar.run();
  CHECK(text.starts_with("## Flame\n\n| Keyword | Type | Units (SI) | Default"));
  CHECK(text.find("| `@T` | single_measure | K |  | yes |  | Temperature |") !=
        std::string::npos);
  CHECK(text.find("| `@Model` | single_string |  | `A` |  | one of: A, B | "
                  "Model \\| variant |") != std::string::npos);
  CHECK(text.find("| `@X` | vector_double |  |  | when @Model is B | 2 to 4 "
                  "elements, sum = 1 | Fractions |") != std::string::npos);
  CHECK(text.find("**Rules**\n\n- @X is required when @Model is B\n") !=
        std::string::npos);
}

TEST_CASE("semantics", "grammar files declare properties and rules") {
  testing::TemporaryDirectory dir;
  const auto file = dir.write(
      "flame.grammar",
      "keyword: @Model\ntype: single_string\nshort_comment: model\n"
      "is_compulsory: false\ncompulsory_alternatives: none\n"
      "required_keywords: none\nconflicting_keywords: none\n"
      "allowed_values: Constant Variable\ndefault: Constant\n\n"
      "keyword: @Profile\ntype: vector_double\nshort_comment: profile\n"
      "is_compulsory: false\ncompulsory_alternatives: none\n"
      "required_keywords: none\nconflicting_keywords: none\n"
      "minimum: 0 exclusive\nsize: 2 100\nsum: 1 1e-3\n\n"
      "keyword: @Old\ntype: single_int\nshort_comment: old\n"
      "is_compulsory: false\ncompulsory_alternatives: none\n"
      "required_keywords: none\nconflicting_keywords: none\n"
      "deprecated: use @Profile\n\n"
      "rule: required @Profile when @Model = Variable\n"
      "rule: at_most_one_of @Profile @Old\n");
  auto manager = parse("Dictionary d { @Model Variable; @Old 1; }");
  auto &d = manager("d");
  const auto error = catch_error([&] { d.SetGrammar(file.string()); });
  CHECK(error.error_count() == 1);
  CHECK(std::string(error.what()).find("the compulsory keyword @Profile is "
                                       "missing (when @Model is Variable)") !=
        std::string::npos);
  CHECK(error.has(DiagnosticCode::Deprecated)); // listed as a warning

  auto good = parse("Dictionary d { @Profile 0.5 0.5; }");
  good("d").SetGrammar(file.string());
  CHECK(good("d").Read<std::string>("@Model") == "Constant");

  auto bad = parse("Dictionary d { @Profile 0.6 0; }");
  const auto range = catch_error([&] { bad("d").SetGrammar(file.string()); });
  CHECK(range.error_count() == 2); // value 0 and sum 0.6

  const auto wrong = dir.write("wrong.grammar",
                               "keyword: @A\ntype: single_int\nshort_comment: a\n"
                               "is_compulsory: false\ncompulsory_alternatives: none\n"
                               "required_keywords: none\nconflicting_keywords: none\n"
                               "rule: sometimes @A\n");
  CHECK_THROWS_WITH(good("d").SetGrammar(wrong.string()),
                    "invalid rule line 'rule: sometimes @A'");
}
