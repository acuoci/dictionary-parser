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
value is that name, so existing code reading it with `ReadDictionary` works
unchanged. Inline blocks can be nested, and a `;` after the closing brace is
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

Here `ReadDictionary("@InitialStatus", name)` returns
`BatchReactor/InitialStatus`, which is retrieved with `manager(name)`.

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
- Errors raised while reading (`ReadInt`, `ReadMeasure`, ...) have the same
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
  `missing-keyword`, `wrong-dimension`, `grammar-incoherent`.
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
| `SetDefault("1800 kg/m3")` | any non-compulsory keyword | value returned by `Read*` when the keyword is absent (`CheckOption` still reports only the file) |
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
  in `dictionary.warnings()` after `SetGrammar`.
- `dictionary.CheckUnusedKeywords()` (or `manager.CheckUnusedKeywords()` for
  all dictionaries), called after reading, returns a warning for each keyword
  written in the file but never read by a `Read*` call: typically the calling
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
double velocity, temperature;            // 0.195 m/s, 298.15 K
std::vector<double> times;               // 60, 120, 300 s
dictionary.ReadMeasure("@FuelVelocity", velocity);
dictionary.ReadMeasure("@Temperature", temperature);
dictionary.ReadMeasures("@SnapshotTimes", times);
```

- `ReadMeasure(name, value, dimension)` and `ReadMeasures(name, values,
  dimension)` take the expected dimension explicitly, for grammars that do
  not declare it. If the grammar does declare one, both must agree.
- The legacy `ReadMeasure(name, value, units)` is unchanged and returns the
  raw magnitude and unit string.
- Lists of measures accept pairs (`1 m 20 cm`) or numbers followed by one
  shared unit (`1000 1100 1200 K`).
- Grammar files declare a dimension with an optional eighth line after the
  seven keyword lines: `dimension: m/s` (or `dimension: K difference`).
- `units::to_si(value, "cm/s", dim::Velocity)` and `units::from_si` are
  available for units found elsewhere (e.g. a separate `@XUnits` keyword).
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
  dictionary validation, units, measures, diagnostics, rules and keyword
  semantics (`unit_tests [suite...]` runs selected suites).
- `Test_Example_*`: parse the input files in `examples/`, including a
  grammar-aware test for `input3.dic`.
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
