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


#include "DictionaryTyped.h"
#include "test_framework.h"
#include "test_support.h"
#include "typed_grammars.h"

#include <cmath>
#include <filesystem>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

using namespace OpenSMOKEpp;
using namespace OpenSMOKEpp::typed;
using testing::parse;

namespace {
template <typename F> auto catch_error(F &&f) -> DictionaryError {
  try {
    f();
  } catch (const DictionaryError &error) {
    return error;
  }
  throw std::runtime_error("expected a DictionaryError");
}

auto near(double a, double b) -> bool {
  return std::abs(a - b) <= 1e-9 * std::max(1.0, std::abs(b));
}

constexpr const char *reactor_input = R"(
Dictionary batch
{
  @Type           NonIsothermal;
  @EndTime        2 ms;
  @OdeSolver      Cvode;
  @InitialStatus  mixture;
  @Inlets         air fuel;
  @Profile        s K   0 300   1 500;
  @Links          1 2 0.5 kg/s   2 3 0.25 g/s;
  @OutputSpecies  CH4 O2;
}
Dictionary mixture
{
  @Temperature    1000 K;
  @Pressure       1 atm;
  @MoleFractions  CH4 0.5 O2 0.5;
}
Dictionary air  { @Temperature 300 K; @Pressure 1 bar; @MoleFractions O2 0.21 N2 0.79; }
Dictionary fuel { @Temperature 300 K; @Density 0.7 kg/m3; @MassFractions CH4 1; }
)";
} // namespace

// ---------------------------------------------------------------------------
// Typed keys
// ---------------------------------------------------------------------------

// The value type of a key is deduced from its spec.
static_assert(std::is_same_v<decltype(gas::T)::value_type, double>);
static_assert(std::is_same_v<decltype(reactor::Steps)::value_type, int>);
static_assert(std::is_same_v<decltype(reactor::OdeSolver)::value_type, reactor::Solver>);
static_assert(std::is_same_v<decltype(reactor::Type)::value_type, std::string>);
static_assert(std::is_same_v<decltype(reactor::Output)::value_type, std::filesystem::path>);
static_assert(std::is_same_v<decltype(gas::X)::value_type,
                             std::vector<std::pair<std::string, double>>>);
static_assert(std::is_same_v<decltype(reactor::Profile)::value_type,
                             std::vector<std::array<double, 2>>>);
static_assert(std::is_same_v<decltype(reactor::Links)::value_type,
                             std::vector<std::tuple<int, int, double>>>);
static_assert(std::is_same_v<decltype(reactor::Species)::value_type,
                             std::variant<std::string, std::vector<std::string>>>);
static_assert(std::is_same_v<decltype(reactor::InitialStatus)::value_type, View<gas::Status>>);
static_assert(decltype(gas::T)::name() == "@Temperature");
static_assert(reactor::Steps.default_.has_value() && *reactor::Steps.default_ == "100");

// The test grammars are coherent.
static_assert(check<gas::Status>().issue == Issue::none);
static_assert(check<reactor::Batch>().issue == Issue::none);
static_assert(check<chain::Node>().issue == Issue::none);

// ---------------------------------------------------------------------------
// Compile-time coherence: each problem is detected (and names the key)
// ---------------------------------------------------------------------------
namespace incoherent {
using namespace gas;
inline constexpr auto A = Key<"@A">(Integer(), "a");
inline constexpr auto B = Key<"@B">(Integer(), "b");
inline constexpr auto C = Key<"@C">(Integer(), "c");
inline constexpr auto Mode = Key<"@Mode">(Enumeration("Fast", "Slow").Disable("Slow", "no"), "mode");

struct Duplicate : Grammar<Duplicate> {
  static constexpr auto keys = Keys(T, P, T);
};
struct CompulsoryDefault : Grammar<CompulsoryDefault> {
  static constexpr auto keys = Keys(A.Compulsory().Default("1"));
};
struct UndeclaredInRule : Grammar<UndeclaredInRule> {
  static constexpr auto keys = Keys(A, B);
  static constexpr auto constraints = Rules(Requires(A, C));
};
struct ListedTwice : Grammar<ListedTwice> {
  static constexpr auto keys = Keys(A, B);
  static constexpr auto constraints = Rules(Conflicts(A, B, A));
};
struct CountTooLarge : Grammar<CountTooLarge> {
  static constexpr auto keys = Keys(A, B);
  static constexpr auto constraints = Rules(Exactly(3, A, B));
};
struct AtMostAll : Grammar<AtMostAll> {
  static constexpr auto keys = Keys(A, B);
  static constexpr auto constraints = Rules(AtMost(2, A, B));
};
struct OneKeyGroup : Grammar<OneKeyGroup> {
  static constexpr auto keys = Keys(A, B);
  static constexpr auto constraints = Rules(ExactlyOneOf(A));
};
struct ConditionUndeclared : Grammar<ConditionUndeclared> {
  static constexpr auto keys = Keys(A, B);
  static constexpr auto constraints = Rules(When(Present(C), Required(A)));
};
struct ConditionOnQuantity : Grammar<ConditionOnQuantity> {
  static constexpr auto keys = Keys(T, A);
  static constexpr auto constraints = Rules(When(Equals(T, "300"), Required(A)));
};
struct ConditionBadValue : Grammar<ConditionBadValue> {
  static constexpr auto keys = Keys(Mode, A);
  static constexpr auto constraints = Rules(When(Equals(Mode, "Fats"), Required(A)));
};
struct ConditionDisabledValue : Grammar<ConditionDisabledValue> {
  static constexpr auto keys = Keys(Mode, A);
  static constexpr auto constraints = Rules(When(OneOf(Mode, "Fast", "Slow"), Required(A)));
};
struct ConditionBadInteger : Grammar<ConditionBadInteger> {
  static constexpr auto keys = Keys(A, B);
  static constexpr auto constraints = Rules(When(Equals(A, "one"), Required(B)));
};
struct RequiresConflicting : Grammar<RequiresConflicting> {
  static constexpr auto keys = Keys(A, B);
  static constexpr auto constraints = Rules(Requires(A, B), Conflicts(B, A));
};
struct CompulsoryConflict : Grammar<CompulsoryConflict> {
  static constexpr auto keys = Keys(A.Compulsory(), B);
  static constexpr auto constraints = Rules(Required(B), Conflicts(A, B));
};
struct Dead : Grammar<Dead> {
  // @A requires @B and @C, which conflict with each other.
  static constexpr auto keys = Keys(A, B, C);
  static constexpr auto constraints = Rules(Requires(A, B, C), Conflicts(B, C));
};
struct DeadByForbidden : Grammar<DeadByForbidden> {
  static constexpr auto keys = Keys(A, B);
  static constexpr auto constraints = Rules(Requires(A, B), Forbidden(B));
};
struct DeadByAlwaysPresent : Grammar<DeadByAlwaysPresent> {
  // @C is always present and conflicts with @B, which @A requires.
  static constexpr auto keys = Keys(A, B, C.Compulsory());
  static constexpr auto constraints = Rules(Requires(A, B), Conflicts(B, C));
};
struct UnsatisfiableKeyword : Grammar<UnsatisfiableKeyword> {
  static constexpr auto keys = Keys(A, B, C);
  static constexpr auto constraints =
      Rules(Required(A), Requires(A, B, C), ExactlyOneOf(B, C));
};
struct UnsatisfiableGroup : Grammar<UnsatisfiableGroup> {
  // Both members of the group are dead: nothing can satisfy it.
  static constexpr auto keys = Keys(A, B, C);
  static constexpr auto constraints =
      Rules(AtLeastOneOf(A, B), Forbidden(A), Forbidden(B));
};
struct TooManyAlways : Grammar<TooManyAlways> {
  static constexpr auto keys = Keys(A.Compulsory(), B.Compulsory(), C.Compulsory());
  static constexpr auto constraints = Rules(AtMost(2, A, B, C));
};
struct ConditionalIsFine : Grammar<ConditionalIsFine> {
  // Conditional rules do not take part in the dead-key analysis.
  static constexpr auto keys = Keys(Mode, A, B);
  static constexpr auto constraints =
      Rules(When(Equals(Mode, "Fast"), Forbidden(B)), Requires(A, B));
};
struct Empty : Grammar<Empty> {
  static constexpr auto keys = Keys();
};

template <class G> constexpr auto issue() { return check<G>().issue; }
template <class G> constexpr auto keyword_is(std::string_view name) {
  return check<G>().keyword.view() == name;
}
template <class G> constexpr auto other_is(std::string_view name) {
  return check<G>().other.view() == name;
}

static_assert(issue<Duplicate>() == Issue::duplicate_keyword &&
              keyword_is<Duplicate>("@Temperature"));
static_assert(issue<CompulsoryDefault>() == Issue::compulsory_keyword_with_default);
static_assert(issue<UndeclaredInRule>() == Issue::rule_on_undeclared_keyword &&
              keyword_is<UndeclaredInRule>("@C"));
static_assert(issue<ListedTwice>() == Issue::keyword_listed_twice_in_rule &&
              keyword_is<ListedTwice>("@A"));
static_assert(issue<CountTooLarge>() == Issue::invalid_rule_count);
static_assert(issue<AtMostAll>() == Issue::invalid_rule_count);
static_assert(issue<OneKeyGroup>() == Issue::rule_needs_two_keywords);
static_assert(issue<ConditionUndeclared>() == Issue::condition_on_undeclared_keyword &&
              keyword_is<ConditionUndeclared>("@C"));
static_assert(issue<ConditionOnQuantity>() == Issue::condition_on_multi_token_value);
static_assert(issue<ConditionBadValue>() == Issue::invalid_condition_value &&
              keyword_is<ConditionBadValue>("@Mode") && other_is<ConditionBadValue>("Fats"));
static_assert(issue<ConditionDisabledValue>() == Issue::invalid_condition_value &&
              other_is<ConditionDisabledValue>("Slow"));
static_assert(issue<ConditionBadInteger>() == Issue::invalid_condition_value);
static_assert(issue<RequiresConflicting>() == Issue::requires_and_conflicts &&
              keyword_is<RequiresConflicting>("@A") && other_is<RequiresConflicting>("@B"));
static_assert(issue<CompulsoryConflict>() == Issue::compulsory_keywords_conflict);
static_assert(issue<Dead>() == Issue::dead_keyword && keyword_is<Dead>("@A") &&
              other_is<Dead>("@B"));
static_assert(issue<DeadByForbidden>() == Issue::dead_keyword &&
              keyword_is<DeadByForbidden>("@A") && other_is<DeadByForbidden>("@B"));
static_assert(issue<DeadByAlwaysPresent>() == Issue::dead_keyword &&
              keyword_is<DeadByAlwaysPresent>("@A") && other_is<DeadByAlwaysPresent>("@B"));
static_assert(issue<UnsatisfiableKeyword>() == Issue::unsatisfiable_keyword &&
              keyword_is<UnsatisfiableKeyword>("@A"));
static_assert(issue<UnsatisfiableGroup>() == Issue::unsatisfiable_rule);
static_assert(issue<TooManyAlways>() == Issue::unsatisfiable_rule);
static_assert(issue<ConditionalIsFine>() == Issue::none);
static_assert(issue<Empty>() == Issue::none);
} // namespace incoherent

TEST_CASE("frontend", "the compile-time analysis agrees with the runtime check") {
  // A coherent typed grammar passes the runtime check too; the incoherent
  // ones above are never instantiated (they would not compile).
  CHECK_NOTHROW((void)runtime_grammar<reactor::Batch>());
  CHECK_NOTHROW((void)runtime_grammar<incoherent::ConditionalIsFine>());
  const DictionaryGrammar grammar = runtime_grammar<gas::Status>();
  CHECK(grammar.keywords().size() == 5);
  CHECK(grammar.rules().size() == 2);
  CHECK(grammar.Describe().find("exactly 2 of @Temperature, @Pressure, @Density") !=
        std::string::npos);
}

// ---------------------------------------------------------------------------
// Views
// ---------------------------------------------------------------------------
TEST_CASE("frontend", "a view reads every key with its declared type") {
  auto manager = parse(reactor_input);
  const auto batch = manager.view<reactor::Batch>("batch");

  CHECK(batch.get(reactor::Type) == "NonIsothermal");
  CHECK(near(batch.get(reactor::EndTime), 2e-3));
  CHECK(batch.get(reactor::OdeSolver) == reactor::Solver::CVODE); // alias
  CHECK(batch.get(reactor::Steps) == 100);                        // default
  CHECK(batch.get(reactor::Verbose) == false);                    // default
  CHECK(batch.get(reactor::Output) == std::filesystem::path("Output"));

  const auto profile = batch.get(reactor::Profile);
  REQUIRE(profile.size() == 2);
  CHECK(near(profile[1][0], 1.0) && near(profile[1][1], 500.0));

  const auto links = batch.get(reactor::Links);
  REQUIRE(links.size() == 2);
  CHECK(std::get<0>(links[1]) == 2 && std::get<1>(links[1]) == 3);
  CHECK(near(std::get<2>(links[1]), 2.5e-4));

  const auto species = batch.get(reactor::Species);
  REQUIRE(species.index() == 1);
  CHECK((std::get<1>(species) == std::vector<std::string>{"CH4", "O2"}));
}

TEST_CASE("frontend", "get_if, get_or, has and read") {
  auto manager = parse(reactor_input);
  const auto batch = manager.view<reactor::Batch>("batch");

  CHECK(!batch.has(reactor::Volume));
  CHECK(!batch.get_if(reactor::Volume).has_value());
  CHECK(near(batch.get_or(reactor::Volume, 1.5), 1.5));
  CHECK(batch.get_if(reactor::Steps).value_or(0) == 100); // default counts
  CHECK(!batch.has(reactor::Steps));                       // but is not written
  CHECK(batch.has(reactor::EndTime));
  CHECK_THROWS_WITH((void)batch.get(reactor::Volume), "@Volume");

  double end_time = 0.0;
  batch.read(reactor::EndTime, end_time);
  CHECK(near(end_time, 2e-3));
  reactor::Solver solver{};
  batch.read(reactor::OdeSolver, solver);
  CHECK(solver == reactor::Solver::CVODE);

  // read_if: assigns only when present (or defaulted), else keeps the value
  double volume = 7.0;
  CHECK(!batch.read_if(reactor::Volume, volume));
  CHECK(near(volume, 7.0));
  double end = 0.0;
  CHECK(batch.read_if(reactor::EndTime, end));
  CHECK(near(end, 2e-3));
  int steps = 0;
  CHECK(batch.read_if(reactor::Steps, steps)); // default counts
  CHECK(steps == 100);
}

TEST_CASE("frontend", "references are followed with their own grammar") {
  auto manager = parse(reactor_input);
  const auto batch = manager.view<reactor::Batch>("batch");

  const View<gas::Status> mixture = batch.get(reactor::InitialStatus);
  CHECK(mixture.name() == "mixture");
  CHECK(near(mixture.get(gas::T), 1000.0));
  CHECK(near(mixture.get(gas::P), 101325.0));
  CHECK(mixture.get(gas::X).size() == 2);
  CHECK(!mixture.get_if(gas::Rho).has_value());

  const auto inlets = batch.get(reactor::Inlets);
  REQUIRE(inlets.size() == 2);
  CHECK(near(inlets[1].get(gas::Rho), 0.7));
  CHECK(inlets[1].get(gas::Y).front().first == "CH4");
}

TEST_CASE("frontend", "invalid dictionaries are reported when viewed") {
  auto manager = parse(R"(
Dictionary batch { @Type Isothermal; @EndTime 1 s; @OdeSolver DASPK; @InitialStatus mixture; @Links 1 2 1 kg/s; }
Dictionary mixture { @Temperature 300 K; @MoleFractions O2 1; }
)");
  const auto error = catch_error([&] { (void)manager.view<reactor::Batch>("batch"); });
  CHECK(error.has(DiagnosticCode::NotAllowed));      // DASPK disabled
  CHECK(error.has(DiagnosticCode::MissingRequired)); // @Links requires @Inlets
  CHECK(std::string(error.what()).find("not available in this build") != std::string::npos);

  // The referenced dictionary is validated when the reference is read.
  auto manager2 = parse(R"(
Dictionary batch { @Type Isothermal; @EndTime 1 s; @InitialStatus mixture; }
Dictionary mixture { @Temperature 300 K; @MoleFractions O2 1; }
)");
  const auto batch = manager2.view<reactor::Batch>("batch");
  const auto error2 = catch_error([&] { (void)batch.get(reactor::InitialStatus); });
  CHECK(error2.has(DiagnosticCode::Cardinality)); // exactly 2 of T, P, Rho
}

TEST_CASE("frontend", "conditional rules of a typed grammar") {
  auto manager = parse(R"(
Dictionary batch { @Type NonIsothermal; @EndTime 1 s; @InitialStatus mixture; }
Dictionary mixture { @Temperature 300 K; @Pressure 1 bar; @MoleFractions O2 1; }
)");
  const auto error = catch_error([&] { (void)manager.view<reactor::Batch>("batch"); });
  CHECK(error.has(DiagnosticCode::MissingCompulsory));
  CHECK(std::string(error.what()).find("@Profile is missing (when @Type is NonIsothermal)") !=
        std::string::npos);
}

TEST_CASE("frontend", "deprecated keys produce warnings") {
  auto manager = parse(R"(
Dictionary batch { @Type Isothermal; @EndTime 1 s; @InitialStatus m; @OldVolume 1 m3; }
Dictionary m { @Temperature 300 K; @Pressure 1 bar; @MoleFractions O2 1; }
)");
  const auto batch = manager.view<reactor::Batch>("batch");
  REQUIRE(batch.dictionary().warnings().size() == 1);
  CHECK(batch.dictionary().warnings().front().code == DiagnosticCode::Deprecated);
}

TEST_CASE("frontend", "a dictionary is attached once per typed grammar") {
  auto manager = parse(reactor_input);
  const auto a = manager.view<gas::Status>("mixture");
  const auto b = manager.view<gas::Status>("mixture"); // no new validation
  CHECK(&a.dictionary() == &b.dictionary());

  const auto error = catch_error([&] { (void)manager.view<reactor::Batch>("mixture"); });
  CHECK(error.has(DiagnosticCode::GrammarMismatch));
  CHECK(std::string(error.what()).find("'gas status'") != std::string::npos);
  CHECK(std::string(error.what()).find("'batch reactor'") != std::string::npos);
}

TEST_CASE("frontend", "attach without a manager") {
  auto manager = parse(reactor_input);
  const auto batch = attach<reactor::Batch>(manager("batch"));
  CHECK(near(batch.get(reactor::EndTime), 2e-3));
  CHECK_THROWS_WITH((void)batch.get(reactor::InitialStatus),
                    "cannot follow references");
}

// ---------------------------------------------------------------------------
// Validation of the reference graph
// ---------------------------------------------------------------------------
TEST_CASE("frontend", "validate walks every reference") {
  auto manager = parse(std::string(reactor_input) +
                       "Dictionary orphan { @Temperature 300 K; }\n");
  const auto warnings = manager.validate<reactor::Batch>("batch");
  REQUIRE(warnings.size() == 1);
  CHECK(warnings.front().code == DiagnosticCode::UnreferencedDictionary);
  CHECK(warnings.front().message.find("'orphan'") != std::string::npos);
  // Every referenced dictionary now has its grammar.
  CHECK(manager("air").has_grammar() && manager("fuel").has_grammar());
}

TEST_CASE("frontend", "validate collects every error of the graph") {
  auto manager = parse(R"(
Dictionary batch
{
  @Type           Isothermal;
  @EndTime        1 s;
  @InitialStatus  mixture;
  @Inlets         air fuell;
}
Dictionary mixture { @Temperature 300 K; @MoleFractions O2 1; }
Dictionary air     { @Temperature 300 K; @Pressure 1 bar; @MoleFractions O2 0.5; }
Dictionary fuel    { @Temperature 300 K; @Pressure 1 bar; @MassFractions CH4 1; }
)");
  const auto error = catch_error([&] { (void)manager.validate<reactor::Batch>("batch"); });
  CHECK(error.has(DiagnosticCode::Cardinality));       // mixture: only T
  CHECK(error.has(DiagnosticCode::OutOfRange));        // air: sum is 0.5
  CHECK(error.has(DiagnosticCode::UnknownDictionary)); // fuell
  const std::string text = error.what();
  CHECK(text.find("did you mean 'fuel'?") != std::string::npos);
  CHECK(text.find("test.dic:7:") != std::string::npos); // located at @Inlets
}

TEST_CASE("frontend", "validate reports cycles") {
  auto manager = parse(R"(
Dictionary a { @Next b; }
Dictionary b { @Next c; }
Dictionary c { @Next a; }
)");
  const auto error = catch_error([&] { (void)manager.validate<chain::Node>("a"); });
  CHECK(error.has(DiagnosticCode::ReferenceCycle));
  CHECK(std::string(error.what()).find("a -> b -> c -> a") != std::string::npos);
}

TEST_CASE("frontend", "validate reports a dictionary used with two grammars") {
  auto manager = parse(R"(
Dictionary batch { @Type Isothermal; @EndTime 1 s; @InitialStatus shared; @Inlets shared; }
Dictionary shared { @Temperature 300 K; @Pressure 1 bar; @MoleFractions O2 1; }
)");
  CHECK_NOTHROW((void)manager.validate<reactor::Batch>("batch")); // same grammar twice
  CHECK_THROWS_WITH((void)manager.view<chain::Node>("shared"), "already used as 'gas status'");
}
