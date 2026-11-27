# OpenSMOKE++ Style Dictionary Parser

**A standalone, high-performance, modern C++ library for parsing and processing structured input files according to rigorous dictionary syntax rules.**

OpenSMOKE++ Dictionary Parser provides a compact parser for `Dictionary ... { ... }` input files with keyword-based options, multiline entries, comments, source-line diagnostics, and optional grammar validation. The parser core is fully decoupled from Boost, Eigen, and the wider OpenSMOKE++ framework.

## Highlights

| Area | Description |
| --- | --- |
| Language standard | Pure modern C++ with C++20/C++23-ready interfaces |
| Dependencies | Zero external runtime or parser dependencies |
| Architecture | Public headers in `include/`, private compiled implementation in `src/` |
| Parsing model | Dictionary blocks, `@Keyword` entries, semicolon terminators, multiline values |
| Syntax | Inline dictionaries, `#include`, quoted strings, flexible brace placement |
| Units | Dimensions declared in the grammar; units in the input checked and converted to SI automatically |
| Option formats | Records, lists, tables with units, enumerations with aliases, choices; `Read<T>` into C++ types |
| Typed front end | Keys declared once as `constexpr` objects; reading an undeclared key or into the wrong type does not compile; grammar coherence checked at compile time; typed references between dictionaries |
| Validation | Grammar rules (counting, requirements, conflicts, conditional rules), defaults, allowed values, ranges, sizes and sums; every value checked at load time; coherence analysis of the grammar itself |
| Error handling | `DictionaryError` with structured diagnostics; all problems reported together, compiler-style messages with source excerpts, "did you mean" suggestions; nothing printed by the library |
| Tests | Unit tests, integration tests over `examples/`, optional corpus test |
| Documentation | Public headers documented with Doxygen comments |
| License | GNU General Public License v3 |

## Repository Layout

```text
.
├── include/                 # Public API headers
├── src/                     # Private implementation files
├── examples/                # Example dictionary input files
├── tests/                   # Unit, integration and corpus tests
├── .github/workflows/       # Continuous integration (GCC, Clang, sanitizers)
├── CMakeLists.txt           # CMake build and test configuration
├── LICENSE                  # GNU GPL v3 license text
└── README.md
```

The public API is intentionally separated from the implementation. Consumers include headers from `include/` and link against the compiled `dictionary_parser` library.

## Dictionary Syntax

Input files are organized into one or more named dictionary blocks:

```text
Dictionary dae-parameters
{
    @DaeSolver            OpenSMOKE++;
    @RelativeTolerance    1e-6;
    @AbsoluteTolerance    1e-10;
}
```

Core syntax rules:

- `Dictionary <name>` begins a top-level block.
- `{` and `}` delimit the dictionary body. They may be placed on their own
  lines or on the same line as the name or the entries
  (`Dictionary grid { @Points 100; }`).
- Options begin with `@`.
- Every option terminates with `;`.
- Values are whitespace-tokenized.
- `//` starts a comment running to the end of the line when it appears at the
  start of a token (at the beginning of a line, after a blank, or right after
  `;`, `{`, `}`). Inside a value, as in `http://host/x` or `dir//file`, it is
  part of the value.
- Keyword entries may span multiple physical lines until the terminating `;`.

### Quoted strings

A value enclosed in double quotes may contain blanks and the characters
`;`, `@`, `{`, `}` and `//`. The closing quote must be on the same line;
`\"` and `\\` are the only escape sequences.

```text
@Output   "Output files/run 01";
@Title    "C2H4/air flame @ 1 atm; strain rate 80 1/s";
```

### Inline dictionaries

A keyword followed directly by a block defines a nested dictionary. It is
stored under the name `<parent>/<Keyword>` (without `@`), and the keyword's
value is that name, so it is read like any reference to a dictionary. Inline blocks can be nested, and a `;` after the closing brace is
optional.

```text
Dictionary BatchReactor
{
    @InitialStatus
    {
        @Temperature  1000 K;
        @Pressure     1 atm;
        @MoleFractions H2 0.3 O2 0.7;
    }
    @Options { @Verbose true; }
}
```

Here `Read<std::string>("@InitialStatus")` returns
`BatchReactor/InitialStatus`, which is retrieved with `manager(name)` (a
typed `Reference` key follows it directly).

### Including files

`#include "file.dic"` (outside dictionary blocks) reads the dictionaries of
another file. Relative paths are resolved against the directory of the file
containing the directive. A file included more than once is read only once;
circular includes are reported as errors. Diagnostics refer to the file where
the problem is.

```text
#include "common/solver-parameters.dic"
```

See [examples/syntax-extensions](examples/syntax-extensions) for a complete
example.

### Diagnostics

Every error about an input file, a grammar or a read is thrown as an
`OpenSMOKEpp::DictionaryError` (derived from `std::runtime_error`). It carries
a list of structured `Diagnostic`s (severity, code, location, message, hint,
related notes), and `what()` renders them in compiler style, with the source
line and a marker under the offending token:

```text
Dictionary flame defined in file flame.dic: 3 errors found
flame.dic:3:2: error: the keyword @Temprature is not recognized in this dictionary
    3 |  @Temprature  300 K;
      |  ^~~~~~~~~~~
      = hint: did you mean @Temperature?
flame.dic:6:2: error: the keywords @Lean and @Rich are mutually exclusive and cannot be used together
    6 |  @Rich  true;
      |  ^~~~~
flame.dic:5:2: note: @Lean is specified here
    5 |  @Lean  true;
      |  ^~~~~
flame.dic:4:20: error: keyword @Velocity: units 'K' (temperature [K]) are not compatible with the expected velocity [m/s]
    4 |  @Velocity    19.5 K;
      |                    ^
```

- Validation reports **all** problems at once; a missing keyword points at
  the dictionary name, a conflict points at the later keyword with a note on
  the earlier one, value and unit errors point at the exact token.
- Errors raised while reading (`Read<T>`, `ReadValue`) have the same
  form, because each dictionary keeps a shared copy of its source text.
- Programs can inspect the diagnostics instead of parsing text:

```cpp
try {
    dictionary.SetGrammar(grammar);
} catch (const OpenSMOKEpp::DictionaryError& error) {
    for (const auto& d : error.diagnostics())
        if (d.code == OpenSMOKEpp::DiagnosticCode::UnknownKeyword)
            std::cerr << d.location.line << ": " << d.message << " (" << d.hint << ")\n";
}
```

- Codes: `syntax`, `include`, `duplicate-dictionary`, `unknown-keyword`,
  `missing-compulsory`, `alternative-group`, `missing-required`, `conflict`,
  `help-requested`, `invalid-value`, `invalid-unit`, and, for mistakes in the
  calling code, `no-grammar`, `undeclared-keyword`, `wrong-type`,
  `missing-keyword`, `grammar-incoherent`.
- Syntax errors are `Lexer::SyntaxError`, a `DictionaryError` that also
  exposes `file_name()`, `line()`, `column()` and `message()`.
- `Dictionary::entries()` gives the parsed entries (keyword, values, token
  positions) and `location_of(keyword)` the position of a keyword.

## Grammar Rules and Keyword Properties

### Rules

Besides the per-keyword lists of the original format (compulsory
alternatives, required keywords, conflicting keywords), a grammar can declare
structural rules with `AddRule` and the factory functions of
`OpenSMOKEpp::rules` (`DictionaryRules.h`):

```cpp
void DefineRules() override
{
    AddKeyWord(DictionaryKeyWord("@Type", SINGLE_STRING, "Reactor type", false)
                   .SetAllowedValues({"Isothermal", "UserDefinedVolume"})
                   .SetDefault("Isothermal"));
    // ... other keywords ...

    AddRule(rules::Exactly(2, {"@Temperature", "@Pressure", "@Density"}));
    AddRule(rules::ExactlyOneOf({"@MoleFractions", "@MassFractions", "@Moles"}));
    AddRule(rules::Conflicts("@Lean", {"@Rich"}));        // symmetric
    AddRule(rules::RequiresAnyOf("@Fuel", {"@Oxidizer", "@Air"}));
    AddRule(rules::When(rules::Equals("@Type", "UserDefinedVolume"),
                        rules::Required("@VolumeProfile")));
}
```

| Rule | Meaning |
| --- | --- |
| `Required(k)` / `Forbidden(k)` | `k` must be present / absent |
| `Exactly(n, ks)`, `AtLeast(n, ks)`, `AtMost(n, ks)` | counting rules on a group |
| `ExactlyOneOf`, `AtLeastOneOf`, `AtMostOneOf` | the same with `n = 1` |
| `Requires(k, ks)` | if `k` is present, all of `ks` must be |
| `RequiresAnyOf(k, ks)` | if `k` is present, at least one of `ks` must be |
| `Conflicts(k, ks)` | `k` cannot be used with any of `ks` (declared once) |
| `When(condition, rule)` | the rule applies only if the condition holds |

Conditions: `Present(k)`, `Absent(k)`, `Equals(k, value)`, `OneOf(k, values)`.
`Equals` and `OneOf` use the effective value of a single-token keyword (the
file value or its default); booleans are compared by value (`on` = `true`).
The legacy declarations are translated into the same rules
(`grammar.rules()` lists them all), so old and new grammars behave alike.

### Keyword Properties

Chainable setters on `DictionaryKeyWord` (next to `SetDimension`):

| Setter | Applies to | Effect |
| --- | --- | --- |
| `SetDefault("1800 kg/m3")` | any non-compulsory keyword | value returned by `Read<T>` when the keyword is absent (`CheckOption` still reports only the file) |
| `SetAllowedValues({"A", "B"})` | `single_string`, `vector_string` | other values are rejected, with a suggestion |
| `SetMinimum(x, inclusive)`, `SetMaximum(x, inclusive)` | integers, doubles, measures (in SI, dimension required), numbers of pair lists | bounds |
| `SetSize(n)`, `SetSize(min, max)` | lists (pairs count as one element) | number of elements |
| `SetSum(1.0, 1e-6)` | `vector_double`, `vector_string_double` | required sum (e.g. fractions) |
| `SetDeprecated("use @New")` | any | using the keyword produces a warning |

**All values are validated when the grammar is attached**: type, number of
values, units, allowed values, ranges, sizes and sums of every keyword written
in the file, reported together with the rule violations:

```text
flame.dic:7:22: error: keyword @PlanckCoefficient: 'Smoke' is not an allowed value (allowed: Smooke, Kent, Sazhin, none)
    7 |   @PlanckCoefficient Smoke;
      |                      ^~~~~
      = hint: did you mean 'Smooke'?
flame.dic:9:16: error: keyword @Temperature: the value 5 C (= 278.15 K) must be >= 300 K
```

### Warnings and Unused Keywords

- Warnings (deprecated keywords) do not stop the program: they are available
  in `dictionary.warnings()` after `SetGrammar`, and returned by
  `manager.validate`. `manager.SetWarningStream(&std::cout)` (or
  `Dictionary::SetWarningStream`) prints them as soon as they are found, each
  one once, in the format of the errors; when the manager is destroyed, it
  also reports the dictionaries the program never used (never validated,
  viewed, looked up or named by a validated dictionary).
- `OpenSMOKEpp::InstallErrorHandler(options)` reports a `DictionaryError` that
  no code catches (title, then every diagnostic, on `options.stream`), waits
  for Enter if `options.wait_for_enter`, and exits with `options.exit_code`,
  instead of the platform's abort message (or none).
- `dictionary.CheckUnusedKeywords()` (or `manager.CheckUnusedKeywords()` for
  all dictionaries), called after reading, returns a warning for each keyword
  written in the file but never read by `Read<T>`/`ReadValue`: typically the calling
  code reads a misspelled keyword. `UnusedKeywords()` returns just the names.
- `SetWarningsAsErrors(true)` on a dictionary or on the manager turns
  warnings into errors (`SetGrammar` and `CheckUnusedKeywords` then throw).

### Documentation

`grammar.Describe("Title")` returns a Markdown table of the keywords (type, SI
units, default, required, constraints, description) followed by the rules.
The `@Help` table also shows defaults, allowed values and rules.

### Grammar Files

The seven-line keyword blocks may be followed by optional property lines, and
rules can be written anywhere with `rule:` lines:

```text
keyword: @Type
type: single_string
short_comment: Reactor type
is_compulsory: false
compulsory_alternatives: none
required_keywords: none
conflicting_keywords: none
allowed_values: Isothermal UserDefinedVolume
default: Isothermal

rule: exactly 2 @Temperature @Pressure @Density
rule: required @VolumeProfile when @Type = UserDefinedVolume
rule: conflicts @Lean @Rich
```

Property lines: `dimension:`, `default:`, `allowed_values:`, `minimum: <x>
[exclusive]`, `maximum: <x> [exclusive]`, `size: <n> [<max>]`,
`sum: <target> [<tolerance>]`, `deprecated: <message>`. Rule kinds:
`required`, `forbidden`, `exactly N`, `at_least N`, `at_most N`,
`exactly_one_of`, `at_least_one_of`, `at_most_one_of`, `requires`,
`requires_any`, `conflicts`; conditions: `when K present`, `when K absent`,
`when K = V`, `when K in V1 V2 ...`.

### Grammar Coherence

When a grammar is built, its rules are checked before any dictionary is
validated. The grammar is rejected (with every problem listed) if, for example:

- a keyword name does not start with `@` or is defined twice;
- a rule references an undefined keyword or the keyword itself, has an
  impossible count, or has a condition on an undefined or multi-token keyword
  or with a value that is not valid for that keyword;
- a keyword both requires and conflicts with another keyword;
- legacy compulsory alternatives do not form a closed group, or a legacy
  conflict is not declared on both keywords;
- a property does not fit the keyword type (allowed values on a number, a
  range on a string or on a measure without dimension, an empty range, a size
  on a scalar), or a default is invalid or set on a compulsory keyword;
- following the requirements transitively, a keyword would force two
  conflicting keywords, two members of the same exactly-one group, or a
  forbidden keyword, so it can never be used;
- no input could ever satisfy the grammar (an always-required keyword that
  can never be used, a counting rule without enough usable keywords, or with
  more always-required keywords than it allows).

## Structured Options and Typed Reads

### Formats

Options that do not fit a fixed type (records, tables, lists of tuples,
special values) are declared with a **format** (`DictionaryFormats.h`)
built from a few composable pieces:

```cpp
using namespace OpenSMOKEpp::formats;
namespace dim = OpenSMOKEpp::units::dim;

// @InternalConnections  3 7 250.3 kg/s   4 8 1.2 kg/s;
AddKeyWord(DictionaryKeyWord("@InternalConnections",
    List(Record({{"from", Integer().Minimum(1)},
                 {"to",   Integer().Minimum(1)},
                 {"flow", Quantity(dim::MassFlowRate)}})),
    "Connections between reactors", false));

// @Profile  s K   0 300   1 500   2 800;      (units header, then rows)
AddKeyWord(DictionaryKeyWord("@Profile",
    Table({{"time", dim::Time}, {"temperature", dim::Temperature}}),
    "Temperature profile", false));

// @OutputSpecies ALL;   or   @OutputSpecies CH4 O2 CO2;
AddKeyWord(DictionaryKeyWord("@OutputSpecies",
    Choice({Enumeration({"ALL", "NONE"}), List(Word())}),
    "Species written in output", false).SetDefault("ALL"));

// @OdeSolver CVODE;   (aliases, and values disabled in this build)
AddKeyWord(DictionaryKeyWord("@OdeSolver",
    Enumeration({"OpenSMOKE++", "CVODE", "DASPK"})
        .Alias("OpenSMOKE", "OpenSMOKE++")
        .Alias("Cvode", "CVODE")
        .Disable("DASPK", "OpenSMOKE++ was built without DASPK"),
    "ODE solver", false));
```

| Format | Tokens |
| --- | --- |
| `Integer()`, `Real()` | one number; `.Minimum(x)`, `.Maximum(x)` |
| `Boolean()` | `true`/`false`/`on`/`off` |
| `Word()` | one token (possibly quoted) |
| `Text()` | all remaining tokens, joined by blanks |
| `Enumeration({...})` | one of the values; `.Alias(alias, value)`, `.Disable(value, reason)`; the code of a value is its index |
| `Quantity(dim)` | number + units, converted to SI; bounds in SI |
| `Record({{"name", format}, ...})` | fixed sequence of fields (only the last may have a variable length) |
| `List(element, min, max)` | repetition of a fixed-width element; `.Size(n)` |
| `QuantityList(dim)` | `1 m 20 cm` or `1 2 3 m`, converted to SI |
| `Table({{"name", dim}, ...})` | one unit per column (`-` if dimensionless), then rows; SI values |
| `Choice({...})` | the first alternative that matches the whole value |
| `Composition()` | name/number pairs (`CH4 0.5 O2 0.5`) |
| `Composition(dim)` | name/number pairs followed by their units (`OH 1e-5 HO2 2e-5 mol/cm3`), converted to SI |
| `Quantity()`, `QuantityList()` | measures whose dimension is given when the value is read (`ValueFormat::WithDimension`) |
| `Unit()`, `Unit(dim)` | one units token (`cm`), checked to be valid units (of `dim`) |

Formats are validated when the grammar is built (for example a list of
variable-length elements, or a bound on a word, is rejected), values are
validated when the grammar is attached, and errors name the element and the
field: `keyword @InternalConnections: element 2, field 'flow': units 'K'
(temperature [K]) are not compatible with the expected mass flow rate
[kg/s]`. Defaults and conditional rules work as for any keyword (a
condition on an enumeration also accepts its aliases).

### Typed Reads

`Read<T>` reads **any** keyword (legacy types included) directly as a C++
value; `TryRead<T>` is the non-throwing version and `ReadValue` returns the
parsed value tree:

```cpp
auto n       = dict.Read<int>("@NumberOfPoints");
auto T       = dict.Read<double>("@Temperature");           // SI if dimensioned
auto x       = dict.Read<std::vector<std::pair<std::string, double>>>("@MoleFractions");
auto links   = dict.Read<std::vector<std::tuple<int, int, double>>>("@InternalConnections");
auto profile = dict.Read<std::vector<std::array<double, 2>>>("@Profile");
auto solver  = dict.Read<OdeSolver>("@OdeSolver");          // enum from the code
auto species = dict.Read<std::variant<std::string, std::vector<std::string>>>("@OutputSpecies");

if (auto r = dict.TryRead<double>("@Tolerance"); r)
    use(r.value());
```

Supported types: `bool`, integers (range-checked), floating-point types,
`std::string`, enums, and `std::vector`, `std::array`, `std::pair`,
`std::tuple`, `std::variant` (one alternative per `Choice` alternative) of
supported types. A value that does not fit the requested type is an error
with code `wrong-type`.

## Typed Grammars (Compile-Time Checks)

`DictionaryTyped.h` lets you declare each keyword once, as a `constexpr` key
whose type carries the keyword name and the C++ type of its value. A typed
grammar lists its keys and rules, and a *view* of a dictionary reads keys with
the type deduced from their declaration:

```cpp
#include "DictionaryTyped.h"

namespace gas {
using namespace OpenSMOKEpp::typed;
namespace dim = OpenSMOKEpp::units::dim;

inline constexpr auto T   = Key<"@Temperature">(Quantity(dim::Temperature).Minimum(0, false), "Temperature");
inline constexpr auto P   = Key<"@Pressure">(Quantity(dim::Pressure), "Pressure");
inline constexpr auto Rho = Key<"@Density">(Quantity(dim::Density), "Density");
inline constexpr auto X   = Key<"@MoleFractions">(Composition().Sum(1.0), "Mole fractions");
inline constexpr auto Y   = Key<"@MassFractions">(Composition().Sum(1.0), "Mass fractions");

struct Status : Grammar<Status> {
  static constexpr std::string_view display_name = "gas status";   // optional
  static constexpr auto keys        = Keys(T, P, Rho, X, Y);
  static constexpr auto constraints = Rules(Exactly(2, T, P, Rho), ExactlyOneOf(X, Y));
};
} // namespace gas

namespace reactor {
using namespace OpenSMOKEpp::typed;
namespace dim = OpenSMOKEpp::units::dim;
enum class Solver { OpenSMOKE, CVODE, DASPK };
inline constexpr auto EndTime = Key<"@EndTime">(Quantity(dim::Time), "End time").Compulsory();
inline constexpr auto Ode     = Key<"@OdeSolver">(Enumeration<Solver>("OpenSMOKE++", "CVODE", "DASPK")
                                                      .Alias("Cvode", "CVODE")
                                                      .Disable("DASPK", "not available in this build"),
                                                  "ODE solver").Default("OpenSMOKE++");
inline constexpr auto Initial = Key<"@InitialStatus">(Reference<gas::Status>(), "Initial status").Compulsory();
struct Batch : Grammar<Batch> {
  static constexpr auto keys = Keys(EndTime, Ode, Initial);
};
} // namespace reactor

auto batch   = manager.view<reactor::Batch>("batch");     // validated once
double t_end = batch.get(reactor::EndTime);                // SI seconds
auto solver  = batch.get(reactor::Ode);                    // reactor::Solver
auto mix     = batch.get(reactor::Initial);                // View<gas::Status>, validated
double T     = mix.get(gas::T);                            // SI kelvin
auto P       = mix.get_or(gas::P, 101325.0);
```

**Rejected at compile time:**

| Mistake | Message |
| --- | --- |
| `mix.get(reactor::EndTime)`: the key is not in the grammar | `View::get: the key is not declared in this grammar` |
| a key with a grammar keyword's name but another value type | `... declares this keyword with a different value type` |
| `float t; batch.read(reactor::EndTime, t);` | `View::read: the variable does not have the type declared by the key` |
| `Key<"Temperature">(...)` | `a keyword name starts with '@' ...` |
| an incoherent grammar | `typed grammar: ...` with the keys in the template arguments, e.g. `Report<Issue::dead_keyword, "@Temperature", "@Pressure">` |

The compile-time coherence analysis covers duplicate keys, compulsory keys
with defaults, rules on undeclared keys, keys listed twice, invalid counts,
conditions on undeclared or multi-token keys, condition values a key can never
have (including disabled enumeration values), keys that require keys they
conflict with, conflicting compulsory keys, unusable ("dead") keys and
unsatisfiable rules. It is the same analysis as `DictionaryGrammar::Check`,
restricted to what is known at compile time. `typed::check<G>()` returns
the first problem found, so it can also be tested with `static_assert`.

**Value specs and C++ types:**

| Spec | C++ type |
| --- | --- |
| `Integer()`, `Real()`, `Boolean()` | `int`, `double`, `bool` (bounds with `Minimum`/`Maximum`) |
| `Word()`, `Text()`, `Path()` | `std::string`, `std::string`, `std::filesystem::path` |
| `Quantity(dim)`, `QuantityList(dim)` | `double`, `std::vector<double>` (SI) |
| `Composition()` | `std::vector<std::pair<std::string, double>>` (`Sum`, `Size`, bounds) |
| `Composition(dim)` | `std::vector<std::pair<std::string, double>>` (SI; the units are compulsory) |
| `Quantity()`, `QuantityList()` | `double`, `std::vector<double>` (SI), read with `get(key, dimension)` |
| `Unit()` | `typed::Units` (`to_si(value, dimension)`) |
| `Enumeration<E>(names...)` | `E` (by position), or `std::string`; `Alias`, `Disable` |
| `List(spec)` | `std::vector<spec type>` (`Size`) |
| `Record(Field("name", spec)...)` | `std::tuple<field types...>` |
| `Table(Column("t", dim::Time), Column("x"))` | `std::vector<std::array<double, N>>` (SI) |
| `Choice(spec...)` | `std::variant<spec types...>` |
| `Reference<G>()`, `ReferenceList<G>()` | `View<G>`, `std::vector<View<G>>` |
| `DictionaryName()` | `std::string` (a dictionary with an untyped grammar) |

Keys are refined with `Compulsory()`, `Default("text as in a file")` and
`Deprecated("advice")`. Rules use the same names as `OpenSMOKEpp::rules`, with
keys instead of strings: `Required`, `Forbidden`, `Exactly`, `AtLeast`,
`AtMost`, `ExactlyOneOf`, `AtLeastOneOf`, `AtMostOneOf`, `Requires`,
`RequiresAnyOf`, `Conflicts`, and `When(Present|Absent|Equals|OneOf(...), rule)`.

**Views.** `get(key)` returns the value (or the default); `get_if(key)`
returns `std::nullopt` when the key is absent and has no default;
`get_or(key, fallback)` returns a fallback instead; `has(key)` tells whether
the key is written in the file; `read(key, out)` requires `out` to have the
exact type of the key; `read_if(key, out)` assigns `out` only when the key is
present (or has a default) and returns whether it did, so the value already in
`out` acts as the default (the old `if (CheckOption(k)) Read(k, out);`).
`dictionary()` gives access to the underlying `Dictionary` (warnings,
locations).

**References and the manager.** `manager.view<G>(name)` attaches `G` to a
dictionary once. Later views with the same grammar do not validate again;
a view with a different typed grammar is an error (`grammar-mismatch`).
`manager.validate<Root>(name)` walks every typed reference from the root,
validates each dictionary with its grammar and collects all the errors in one
`DictionaryError`: invalid dictionaries, references to undefined dictionaries
(`unknown-dictionary`, located at the reference, with a suggestion) and cycles
(`reference-cycle`). A `DictionaryName()` key must name an existing
dictionary too (same error); that dictionary is not validated (its grammar is
not typed), and the dictionaries named among its values count as reached.
`validate` returns the warnings, including dictionaries not reached from the
root (`unreferenced-dictionary`).

`typed::attach<G>(dictionary)` creates a view without a manager (references
cannot be followed), and `typed::runtime_grammar<G>()` returns the equivalent
`DictionaryGrammar`, for example for `Describe()`.

A typed grammar is translated into an ordinary `DictionaryGrammar`, so
everything checked at run time for other grammars (values, units, rules,
`@Help`, unused keywords) applies unchanged.

## Units of Measure

Measure keywords can declare their physical dimension in the grammar. The
units written in the input file are then validated when the grammar is
attached (all errors reported together), and the values are converted to SI
when read: no unit handling is needed in user code.

```cpp
#include "DictionaryUnits.h"
using namespace OpenSMOKEpp;
namespace dim = OpenSMOKEpp::units::dim;

// Grammar
AddKeyWord(DictionaryKeyWord("@FuelVelocity", SINGLE_MEASURE, "Fuel velocity", true)
               .SetDimension(dim::Velocity));
AddKeyWord(DictionaryKeyWord("@Temperature", SINGLE_MEASURE, "Inlet temperature", true)
               .SetDimension(dim::Temperature));
AddKeyWord(DictionaryKeyWord("@SnapshotTimes", VECTOR_MEASURE, "Snapshot times", false)
               .SetDimension(dim::Time));

// Input:  @FuelVelocity 19.5 cm/s;  @Temperature 25 C;  @SnapshotTimes 1 2 5 min;
// (values are SI; amounts of substance in kmol, see below)
const auto velocity    = dictionary.Read<double>("@FuelVelocity");            // 0.195 m/s
const auto temperature = dictionary.Read<double>("@Temperature");             // 298.15 K
const auto times       = dictionary.Read<std::vector<double>>("@SnapshotTimes"); // 60, 120, 300 s
```

- A measure keyword without a dimension is a record: read it as
  `Read<std::pair<double, std::string>>` to get the raw magnitude and unit
  string, and convert it with `units::to_si`.
- Lists of measures accept pairs (`1 m 20 cm`) or numbers followed by one
  shared unit (`1000 1100 1200 K`).
- Grammar files declare a dimension with an optional eighth line after the
  seven keyword lines: `dimension: m/s` (or `dimension: K difference`).
- When the dimension is known only at run time (it depends on another
  keyword, e.g. the type of a parametric analysis or the variable of a
  profile), declare the key with `Quantity()` or `QuantityList()` (also
  inside a list, record or choice) and read it with
  `view.get(key, dimension)`. The grammar checks that a number is followed by
  valid units; the read checks them against `dimension` (errors point at the
  units) and converts to SI. `get(key)` on such a key does not compile.
- Units written in their own keyword (`@XUnits cm;`) are declared with
  `Unit()`: `view.get(key)` returns a `typed::Units` whose
  `to_si(value, dimension)` converts values (an error names the keyword).
- `Composition(dim)` reads pairs followed by their units
  (`OH 1e-5 HO2 2e-5 mol/cm3`): the units are compulsory.
- `units::to_si(value, "cm/s", dim::Velocity)` and `units::from_si` are
  available for units found outside dictionaries (e.g. in a CSV file).
- Mismatches are explicit: `line 12, keyword @FuelVelocity: units 'K'
  (temperature [K]) are not compatible with the expected velocity [m/s]`;
  misspelled symbols get a suggestion (`'Kg' (did you mean 'kg'?)`).

### Dimensions

`units::dim` provides the base dimensions (`Length`, `Mass`, `Time`,
`Temperature`, `Amount`, `Current`, `LuminousIntensity`), `Dimensionless`,
and common derived ones: `Area`, `Volume`, `Velocity`, `Acceleration`,
`Frequency`, `Force`, `Pressure`, `Energy`, `Power`, `Density`,
`Concentration`, `MolarMass`, `MassFlowRate`, `MolarFlowRate`,
`VolumetricFlowRate`, `MassFlux`, `HeatFlux`, `HeatTransferCoefficient`,
`ThermalConductivity`, `DynamicViscosity`, `KinematicViscosity`,
`Diffusivity`, `SurfaceTension`, `SpecificEnergy`, `MolarEnergy`,
`HeatCapacity`, `SpecificHeatCapacity`, `MolarHeatCapacity`, `HeatingRate`,
and more. Others are composed at compile time:
`dim::Mass / dim::Volume / dim::Time.pow({2, 1})`. Exponents are rational
(`m2/s^0.5/kg`, `sqrt(K)`).

**Temperatures.** `dim::Temperature` is an absolute temperature: `25 C` →
298.15 K, `32 F` → 273.15 K. `dim::TemperatureDifference` is an interval:
`10 C` → 10 K, `18 F` → 10 K. Inside compound units (`W/m2/C`, `C/min`) the
Celsius and Fahrenheit symbols always denote intervals.

**Amount of substance.** Following the convention of the whole OpenSMOKE++
framework, amounts are expressed in **kilomoles** by default: `30 kcal/mol` →
1.2552e8 J/kmol, `28 g/mol` → 28 kg/kmol, `1 mol/cm3` → 1000 kmol/m3. All
other quantities are plain SI. Strict SI (mole) is available with
`manager.SetUnitSystem(units::si)` (all dictionaries, including those loaded
later) or `dictionary.SetUnitSystem(units::si)`; `units::to_si`,
`units::from_si`, `units::si_unit` and `units::describe` take the system as an
optional last argument (default `units::si_kmol`).

**Exact factors.** Conversion factors are kept exact, as an integer ratio
times a power of ten (`g/cm3` is exactly 10³, `ms` exactly 10⁻³, `km/h`
exactly 10/36), and the converted value is rounded only once. A value in
`cm3` therefore gives the same `double` as `value / 1.e6`, in `min` the same
as `value * 60.`, in `g/cm3` the same as `value * 1.e3`: reading the input
through units reproduces the usual hand-written conversions bit for bit.
`units::parse_unit` returns the rounded `factor` and its exact form
(`numerator`, `denominator`, `exponent10`).

### Unit Syntax

- Products: `.`, `*` or `·` (`Pa.s`, `kg*m`); each `/` divides by the next
  factor only, so `kg/m3/s2` is kg·m⁻³·s⁻² and `W/m2/K` is W·m⁻²·K⁻¹;
  parentheses group: `W/(m2.K)`.
- Exponents: glued (`m3`, `s-1`, `s0.5`) or with `^` (`m^3`, `s^-1`,
  `s^(1/2)`); `sqrt(...)` is the power ½. A number is a pure factor (`1/s`).
- Symbols are **case-sensitive** (`Pa`, not `pa`; `kg`, not `Kg`).

### Unit Symbols

| Quantity | Symbols (`*` = accepts SI prefixes) |
| --- | --- |
| Length | `m*`, `micron`, `angstrom`, `in`, `ft` |
| Mass | `g*` (so `kg`, `mg`), `lb` (0.45359237 kg), `oz` (avoirdupois) |
| Time | `s*`, `min`, `h`, `hr` |
| Temperature | `K*`, `C`, `°C`, `degC`, `F`, `°F`, `degF`, `R` (Rankine) |
| Amount | `mol*` (so `kmol`) |
| Current, luminous intensity | `A*`, `cd*` |
| Volume | `L*`, `l*` (litre), or any length cubed (`cm3`, `dm3`) |
| Frequency | `Hz*`, `rpm` (2π/60 rad/s) |
| Force | `N*`, `dyn` |
| Pressure | `Pa*`, `bar*`, `atm`, `torr` (atm/760), `mmHg`, `psi` |
| Energy | `J*`, `Wh*` (so `kWh`), `cal*` (thermochemical, 4.184 J), `eV*`, `erg`, `BTU` (IT) |
| Power | `W*` |
| Angle | `rad`, `deg`, `°` (dimensionless) |
| Dimensionless | `-`, `dimensionless`, `#`, `%`, `ppm` |

SI prefixes: `Y Z E P T G M k h da d c m u µ n p f a`. An exact symbol always
takes precedence over a prefixed reading (`min` is the minute, `Pa` the
pascal, `h` the hour while `hPa` is the hectopascal). Not supported on
purpose, because they are ambiguous: `M` (molar), gallons, coulomb, farad.

## Build

### Requirements

- CMake 3.30 or newer
- A C++20-capable compiler (GCC 11+, Clang 16+, Apple Clang 15+)
- Standard C++ library with `<filesystem>` and `<charconv>` support

### Build Options

| Option | Default | Description |
| --- | --- | --- |
| `DICTIONARY_PARSER_BUILD_TESTS` | `ON` when top-level | Build the tests |
| `DICTIONARY_PARSER_WARNINGS_AS_ERRORS` | `OFF` | Treat warnings as errors |
| `DICTIONARY_PARSER_SANITIZE` | `OFF` | AddressSanitizer + UndefinedBehaviorSanitizer (GCC/Clang) |
| `DICTIONARY_PARSER_CORPUS_DIR` | `../OpenSMOKEpp2` if present | Directory scanned recursively for `*.dic` files by the corpus test |

### Configure and Compile

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

This builds the static library target:

```text
dictionary_parser
```

The target is also available inside CMake as:

```cmake
DictionaryParser::dictionary_parser
```

## Run Tests

The repository includes a dependency-free unit-test harness and CTest
integration. From the build directory:

```bash
ctest --output-on-failure
```

- `Unit_*`: lexer, syntax, `#include`, typed reads, grammar coherence,
  dictionary validation, units, measures, diagnostics, rules, keyword
  semantics, formats, typed reads and typed grammars (`unit_tests [suite...]`
  runs selected suites).
- `Test_Example_*`: parse the input files in `examples/`, including a
  grammar-aware test for `input3.dic`.
- `CompileFail_*`: one per file in `tests/compile_fail/`. Each file is
  compiled twice: as it is (it must compile; part of the normal build) and
  with `DICTIONARY_PARSER_SHOULD_FAIL` defined, which must fail with the error
  given on its `// expected-error:` line (`ctest -L compile-fail`).
- `Test_Corpus`: parses every `*.dic` file below `DICTIONARY_PARSER_CORPUS_DIR`
  (enabled only when the directory exists). `test_corpus <dir> --dump` prints
  the parsed content, which is useful to compare two parser versions.

## Using the Library from CMake

When the project is added as a CMake subdirectory, link against the exported alias target:

```cmake
add_subdirectory(path/to/dictionary-parser)

target_link_libraries(my_application
    PRIVATE
        DictionaryParser::dictionary_parser
)
```

Then include public headers:

```cpp
#include "DictionaryManager.h"
```

## Public API Overview

| Header | Purpose |
| --- | --- |
| `DictionaryManager.h` | Reads files or strings (with includes) and owns parsed dictionaries |
| `Dictionary.h` | Provides keyword lookup, grammar validation, and typed reads |
| `DictionaryGrammar.h` | Base class for user-defined grammar rules and coherence checks |
| `DictionaryKeyWord.h` | Keyword metadata, value types, dimensions, defaults and constraints |
| `DictionaryRules.h` | Structural rules and conditions (`rules::Exactly`, `rules::When`, ...) |
| `DictionaryTyped.h` | Typed keys, typed grammars checked at compile time, views and typed references |
| `DictionaryFormats.h` | Value formats (`formats::Record`, `List`, `Table`, `Choice`, ...) and the `Value` tree |
| `DictionaryUnits.h` | Dimensions, unit parsing and conversion to SI |
| `DictionaryInputFile.h` | Preprocessed input-file representation |
| `DictionaryDiagnostics.h` | `Diagnostic`, `DictionaryError`, source locations and rendering |
| `DictionaryLexer.h` | Tokenizer, `SyntaxError`, and low-level lexical utilities |

## Examples

Example input files are available in:

```text
examples/
├── input.1.dic
├── input.2.dic
├── input3.dic
└── syntax-extensions/   # inline dictionaries, #include, quoted strings
```

These files cover representative dictionary layouts, comments, scalar options, measured values, and grammar-validated model configuration.

## Documentation

The public headers in `include/` are documented using Doxygen-compatible comments. Generate API documentation with your preferred Doxygen configuration by pointing `INPUT` to:

```text
include/
```

## License

This project is licensed under the **GNU General Public License v3.0**.

See the root [LICENSE](LICENSE) file for the full license text.

## Copyright

Copyright (C) 2026 Alberto Cuoci.

OpenSMOKEpp is distributed in the hope that it will be useful, but without any warranty; without even the implied warranty of merchantability or fitness for a particular purpose.
