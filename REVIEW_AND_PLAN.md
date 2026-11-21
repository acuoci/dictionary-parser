# dictionary-parser: architecture review and modernization plan

Status: analysis only. No code was changed. Written 2026-11-21.

Scope of the study:
- every source file in this repository (`include/`, `src/`, `tests/`, `examples/`);
- a build of the library plus its 3 CTest tests (all pass);
- a throwaway probe program, kept outside the repo, that exercises edge cases of the grammar and the lexer;
- a survey of how OpenSMOKE++2 uses the same library: about 99 grammar classes, 1,484 keyword declarations and 144 `.dic` files.

---

## 0. Progress log

### Phase 5 (units): done (branch `feature/units`, 2026-11-21)

This phase was brought forward, ahead of Phases 2–4, at the user's request.

Delivered:
- **New public header** `DictionaryUnits.h`:
  - compile-time dimension algebra with rational exponents;
  - about 40 predefined dimensions;
  - a run-time parser for unit expressions (OpenSMOKE++ conventions, parentheses, `^`, `sqrt`);
  - case-sensitive symbols with SI prefixes and spelling suggestions;
  - absolute temperatures versus temperature differences;
  - a selectable amount base. The default is the **kilomole** (`units::si_kmol`), the convention of the whole OpenSMOKE++ framework, chosen by the user; strict SI (`units::si`, mole) is opt-in.
  - the thermochemical calorie (4.184 J), chosen by the user.
- **Grammar integration:**
  - `DictionaryKeyWord::SetDimension(dim::...)`, or an optional `dimension:` line in grammar files;
  - the grammar check rejects a dimension on non-measure keywords;
  - declared units are validated when the grammar is attached, together with all the other errors;
  - the `@Help` table shows the SI unit.
- **New reads:**
  - `Dictionary::ReadMeasure(name, value)` and `ReadMeasures(name, values)` convert to SI;
  - variants take an explicit dimension;
  - `units::to_si` / `from_si` / `check_unit`;
  - `SetUnitSystem` on the dictionary and on the manager.
- **Lists of measures** accept pairs or a shared trailing unit (`1000 1100 K`), including in the legacy `ReadOption(values, units)`.

Verification:
- 18 new test cases. They include a check that every conversion factor follows its exact definition (NIST SP 811 values, temperature fixed points, and the gas constant expressed in six different unit systems).
- 13/13 CTest targets pass, also with ASan/UBSan and `-Werror`.
- MOM's dictionary test still passes.
- **Cross-check against OpenSMOKE++2's `Conversions.cpp`** (205 unit/quantity pairs, default kmol system): 149 are identical and 5 are unsupported on purpose (gallons, volumetric ounces). The rest differ only by constant choices:
  - thermochemical calorie 4.184 J versus 4.18443 J (relative difference 1e-4);
  - `lb` exact versus 0.454 (9e-4), and `oz` (2e-5);
  - updated `BTU`, `eV`, `torr` and `psi` (below 1e-7);
  - `km/hr` and `km2/hr`, which are wrong in OpenSMOKE++2 (`m_from_km = 0.3048`).
- **With the kmol default, molar quantities agree with OpenSMOKE++2.** Strict SI (`units::si`) differs by the expected factor of 1000 per mole.
- **All 29 unit strings used in the 147 corpus input files parse.** Two OpenSMOKE++2 examples (`perfectlystirredreactor/06a`, `06b`) write `kcal/m2/K` for a heat-transfer coefficient, which has no time dimension. Once that keyword declares `dim::HeatTransferCoefficient`, they will be rejected and must use `kcal/m2/K/s`. The solver code for that keyword uses yet another calorie value (4186.8 J/kcal).

### Phase 0 + Phase 1: done (branch `feature/phase-0-1`, 2026-11-21)

Decisions taken: **C++20** baseline; input-format extensions **a, b, c and d**
enabled (these were planned for Phase 2 and were delivered early).

Delivered:
- **Phase 0, safety net.**
  - Dependency-free unit-test harness (`tests/test_framework.h`) with 42 test cases in 6 suites: lexer, syntax, include, reads, grammar, validation.
  - Integration tests over `examples/`.
  - A corpus test over OpenSMOKE++2's `.dic` files.
  - CMake options for warnings, `-Werror` and ASan/UBSan.
  - A GitHub Actions matrix: GCC 11, GCC 14, Clang 18 with sanitizers, GCC 14 with sanitizers, macOS.
  - Tests are built only when this is the top-level project.
- **New lexer and parser** (`src/DictionaryParser.*`, `Lexer::tokenize`), which replace the line-based `DictionaryFile`:
  - tokens with line and column;
  - `Lexer::SyntaxError` with messages of the form `file:line:col: error: ...`;
  - (a) `//` starts a comment only at the start of a token, which fixes D1;
  - (b) quoted strings, which fix D11;
  - (c) braces anywhere, which fixes the first part of D12;
  - (d) inline dictionaries (`@Key { ... }`, stored as `parent/Key`) and `#include` with include-once semantics and cycle detection;
  - stray text outside dictionaries is now an error. Previously it was silently ignored, so for example a misspelled `Dictonary` block went unnoticed;
  - `DictionaryManager::ReadDictionariesFromString`, `names()` and `contains()`.
- **Phase 1 fixes:**
  - D2: "no grammar attached";
  - D3: a complete coherence analysis, including transitive dead-keyword and satisfiability checks;
  - D4 and D5: all problems are collected into one exception, and nothing is printed to stdout;
  - D6: declared versus requested type, and the `Read*` function to use;
  - D7: grammar errors name their dictionary;
  - D8: booleans are case-insensitive, with correct messages;
  - a new error for reading a keyword that the grammar doesn't declare;
  - `TryReadDouble` gains the `NoGrammar` and `UndeclaredKeyword` codes;
  - `SetDictionary` validates the sizes of its vectors.

Verification:
- **Golden comparison** of the old and new parsers on 147 files (144 from OpenSMOKE++2 and 3 examples): all 632 dictionaries are identical, with the same keywords, options and start lines. The only differences are 6 end-line values in one CRLF file, where the old value was wrong because a line containing only blanks plus `\r` was counted as content.
- **Grammar coherence on real grammars:**
  - all 5 MOM grammars (195 keywords) pass;
  - 98 of 99 OpenSMOKE++2 grammars (1,483 keywords, translated to grammar files) pass;
  - the one failure, `solididealreactors/grammar/Grammar_GasStatus.h`, references undeclared keywords and is rejected by the old library too. It is a latent OpenSMOKE++2 bug (the duplicated grammar described in §2.3).
- **MOM** compiles unchanged against the new headers, and its own `test_dictionary_setup` passes when linked against the new library.
- **Clean build** with `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion ... -Werror` and ASan/UBSan, using Apple Clang 21. GCC has not been verified locally; the CI matrix covers it.

API notes: the public signatures used by MOM and OpenSMOKE++2 are unchanged. Removed or changed:
- `DictionaryFile.h` is removed. It was internal and has no users.
- The protected `DictionaryGrammar::Check*` helpers are replaced by private `Collect*` functions. No users.
- `ShowListAvailableKeywords` now takes an optional stream.
- `CheckOption(name)` is now `const`.

Still open from §2.1:
- D9: argument-order overloads.
- D10: the 7-line grammar-file format.
- The second part of D12: nested `Dictionary` blocks remain disallowed by design; use inline blocks.
- D13 and D14: the out-parameter API. This is addressed by the typed front end in Phase 6.

A candidate for the next step: make `CheckOption()` also reject keywords that the grammar doesn't declare. This would catch the 18 dead `CheckOption`/`Read` pairs found in OpenSMOKE++2 at run time, but it changes behavior for existing code, so it should be opt-in first.

---

## 1. How the library works today

```
file ──► Lexer::read_and_preprocess_file   (strip "//", tabs → spaces, blank/good lines)
     ──► DictionaryManager                  (split on lines starting with "Dictionary")
     ──► DictionaryFile::Analyze/Transfer   (braces, '@' … ';' entries, duplicate keys)
     ──► Dictionary                         (parallel vectors: keyword, raw option string, lines)
               ▲
               │ SetGrammar(grammar)  → grammar.UserDefined() → DefineRules() → Check()
               │                      → Dictionary::Checks(): @Help, undefined, compulsory,
               │                                              required, conflicting
user code ──► dict.CheckOption("@X") / dict.ReadDouble("@X", out) / ReadMeasure("@X", v, units)
               (string key looked up at run time, type compared with the grammar's enum,
                raw option re-tokenized and converted on every call)
```

- **The grammar** is a list of `DictionaryKeyWord` objects. Each one holds a name, a value of an 18-entry type enum, a comment, an `is_compulsory` flag, and three space-separated keyword lists: compulsory alternatives, required keywords and conflicting keywords.
- **Values** are stored as raw strings. They are only interpreted when user code calls a `Read*` function. The keyword type is checked only at that moment, by comparing an enum against the grammar.
- **Units** are returned as a bare string. All conversion is done in user code.
- **Sub-dictionaries** are plain string references (`SINGLE_DICTIONARY`). The caller resolves them through the manager and decides which grammar to attach.

The good parts are worth keeping:
- the file format;
- the idea of a per-dictionary grammar;
- source line numbers in errors;
- the recent cleanup: no Boost, `std::from_chars`, exceptions instead of `exit()`, and the start of a `ParseResult`.

---

## 2. Weaknesses, limitations and defects

### 2.1 Confirmed defects in the current library

Items marked **[probe]** were reproduced with the throwaway program.

| # | Defect | Where |
|---|---|---|
| D1 | **[probe]** `//` is stripped anywhere in a line, so `@Url http://x;` loses its `;` and the parse fails. A path containing `//` is silently truncated. | `DictionaryLexer.cpp` `read_and_preprocess_file` |
| D2 | **[probe]** Reading a keyword when no grammar is attached fails with *"is of different type"* and prints `Expected type: 1` on stdout. | `Dictionary::CheckOption(name, type)` |
| D3 | **[probe]** `Check()` accepts grammars that are incoherent: a keyword that conflicts with itself; A both *requires* and *conflicts with* B; alternative groups that are not closed (A↔B and A↔C, but B and C not linked); keyword names without `@`. The problem is only reported later, and only if a user's file happens to trigger it, phrased as **the user's** input error. | `DictionaryGrammar::Check` |
| D4 | `CheckCompulsoryKeyWords` returns at the first alternative-group violation, so only one problem is reported per run. | `DictionaryGrammar.cpp` |
| D5 | Diagnostics are split. Details go to `std::cout`, while the exception only says *"See the messages reported above"*. A GUI, a log file or a test cannot capture them. | all `Check*` functions |
| D6 | The type-mismatch message gives neither the declared type nor the requested type. It prints the enum's integer value to stdout. | `Dictionary::CheckOption` |
| D7 | `DictionaryGrammar::name_` is never set. `file_name_` is empty for C++-defined grammars, so messages read *"Grammar defined in file : …"* and *"List of available options in Dictionary "*. | `DictionaryGrammar` |
| D8 | The error from `ReadBool` says *"requires a number (int)"*. Booleans are case-sensitive, so `True` and `ON` are rejected. | `Dictionary::ReadBool` |
| D9 | `ReadOption(vector<double>&, vector<string>&)` means VECTOR_MEASURE and `(vector<string>&, vector<double>&)` means VECTOR_STRING_DOUBLE. **The meaning is carried only by the order of the arguments.** VECTOR_MEASURE also allows a different unit on each element. | `Dictionary.h` |
| D10 | The 4-argument `DictionaryKeyWord` constructor validates nothing. The 7-argument one validates by building fake grammar-file lines. The grammar-file format requires exactly 7 lines in a fixed order. | `DictionaryKeyWord.cpp` |
| D11 | Values cannot contain `@`, `;`, `{`, `}` or `//`. There is no quoting, so a string with spaces is impossible, except through PATH or SEQUENCE. Even there, internal whitespace is collapsed to single spaces when lines are joined. | `DictionaryFile::Transfer` |
| D12 | Braces must sit on their own line. Nested dictionaries are impossible. There is no include mechanism. | `DictionaryFile::Analyze` |
| D13 | The API is out-parameter based and mutating (`Read(name, out&)`). `CheckOption` is non-const. A typed non-throwing read exists only for `double`. | `Dictionary.h` |
| D14 | Re-tokenization and number parsing happen on every read. This is harmless for speed, but it means the same value can be read under two different types. | `Dictionary.cpp` |

### 2.2 Design-level limitations (the root causes)

1. **Keywords are stringly typed.** A keyword is a string literal that appears in three places: the grammar, the `CheckOption` call and the `Read*` call. Nothing ties them together. This explains both problems you described:
   - reading an undeclared keyword compiles fine;
   - reading a keyword with the wrong type compiles fine.
2. **The type lives in an enum, not in the C++ type system.** The grammar declares `SINGLE_MEASURE`, and the reader must independently remember to call `ReadMeasure`. The return type cannot be deduced from the keyword.
3. **The rule model is too narrow and is welded to the keyword.** Rules are per-keyword lists (`alternatives`, `required`, `conflicting`), so group rules must be written N times by hand. Common rules cannot be expressed:
   - "exactly 2 of {T, P, ρ}";
   - "at most one of …";
   - rules that depend on a value (`@PressureCoefficient` only when `@Type == NonIsothermal-UserDefinedVolume`);
   - value constraints: range, positivity, vector length, even length, sum to 1, admissible values.
4. **The grammar has no semantic content.** It carries no defaults, no admissible values, no units or dimension, and no allowed range. These live in comment strings such as `"(default: 1800 kg/m3)"` and in if/else chains in user code, and the two drift apart.
5. **Validation is fail-fast and goes to stdout.** No structured diagnostic object exists: no severity, no code, no column, no hint, no complete list of errors.
6. **The checks that keep a grammar coherent are pairwise and shallow.** There is no transitive reasoning: A requires B, and B conflicts with compulsory C, so A is unusable. There is no detection of dead keywords and no check that the grammar can be satisfied at all.
7. **Units are not modelled at all.** A measure is a `(double, string)` pair.
8. **Sub-dictionaries are untyped references.** Each reader resolves the reference and chooses which grammar to apply. The same dictionary can therefore be validated against different grammars, or against none.
9. **The lexer is line-based and position-free.** It has no tokens with columns, no quoting and no escape mechanism. Errors carry line numbers but no column, and they print no excerpt of the source.

### 2.3 Evidence from OpenSMOKE++2

The survey quantifies how these limitations show up in practice. Two items were also spot-checked by hand.

- **Reading the wrong keyword:**
  - `ThermogravimetricAnalysis.cpp:400-401` checks `@PolymerFVTBeta` and then reads `@PolymerFVTKTg`, a keyword no grammar declares *(verified)*.
  - `OpenSMOKE_LaminarFlamelets.cpp:299` reads `@DensityCorrectionOnStoichiometricScalarDissipationRate`, but the grammar declares `@DensityCorrectionOnScalarDissipationRate`. The option is unreachable *(verified)*.
  - In total there are about 18 such reads of undeclared keywords. **A typed-key API turns every one of them into a compile error.**
- **Hand-written unit conversion:**
  - about **1,013** `units == "…"` comparisons in 66 files, against 332 `ReadMeasure` calls;
  - the accepted unit sets differ from site to site, and the °C offset is sometimes 273 and sometimes 273.15;
  - `Conversions.h` exists but its conversion functions are never called, and it contains a bug: `m_from_km = 0.3048` *(verified)*;
  - the spellings differ: `hr` in the library, `h` in user code;
  - one label, `kcal/m2/K`, is dimensionally ambiguous.
- **Option types used:**
  - SINGLE_MEASURE is the most used type (307), followed by SINGLE_DICTIONARY (259) and VECTOR_STRING_DOUBLE (71);
  - VECTOR_MEASURE, VECTOR_BOOL, SINGLE_CHAR and VECTOR_CHAR are never used;
  - lists with a trailing unit (`@ListOfValues 1000 1100 1200 K;`) are parsed by hand from VECTOR_STRING with an unguarded `lexical_cast`;
  - records are flattened into VECTOR_DOUBLE (`@InternalConnections 3 7 250.3`), and XY profiles are a flat list of pairs;
  - an odd count is silently truncated.
- **Enumerations** exist only as if/else chains plus a hand-written "Available: A | B" string, and the two drift apart. Feature gating (`#if OPENSMOKEPP_USE_SUNDIALS`) is mixed into those chains.
- **Rules checked by hand:**
  - "exactly 2 of T/P/ρ";
  - "vector length == 1";
  - "fractions sum to 1", sometimes with copy-paste messages that say *mass* in *mole* branches.
- **Duplicated grammars:**
  - `Grammar_GasStatus` is defined twice, with the same class name and the **same header guard**;
  - 17 keyword names have different types in different grammars (`@Temperature` is a bool in one place and a measure in another);
  - the `ReadDictionary` → `manager(name)` → `SetupFromDictionary` pattern is repeated about 277 times.

---

## 3. Design goals for the new architecture

1. **Compile-time safety where the information is static.** Reading a keyword that is not in the grammar, or reading it as the wrong type, must not compile. Errors in the structure of a grammar (duplicates, self-references, contradictions) must not compile either.
2. **Errors that depend on the input** are collected exhaustively, located precisely (file:line:col with an excerpt of the source), returned as data, and never printed by the library.
3. **The grammar is the single source of truth** for type, units or dimension, default, admissible values, constraints, documentation and `@Help` output.
4. **Units are declared by physical dimension** and converted to SI automatically.
5. **Complex options are built by composing a small set of value parsers.** The enum of types does not keep growing.
6. **Migration is incremental.** The existing `Read*` API keeps working on top of the new core, so OpenSMOKE++2 can move one solver at a time.
7. **The constraints in AI_CONTEXT.md are respected:** the standard library only, the heavy logic compiled in `src/`, and a thin template layer in the headers.

---

## 4. Proposed architecture

```
 ┌────────────────────────────────────────────────────────────────────────┐
 │ L5  Typed front end (header, templates)                                │
 │     Key<"@Temperature", Quantity<Temperature>>, Grammar<...>,          │
 │     View<G>::get(key) → deduced type; static_asserts on grammar shape   │
 ├────────────────────────────────────────────────────────────────────────┤
 │ L4  Grammar model + rule engine + coherence analyzer (compiled)        │
 │     one runtime/constexpr data model shared by C++ grammars,           │
 │     grammar files and the legacy DictionaryKeyWord adapter             │
 ├────────────────────────────────────────────────────────────────────────┤
 │ L3  Value system: parsers/combinators + constraints + units (compiled) │
 ├────────────────────────────────────────────────────────────────────────┤
 │ L2  Syntax tree: Document → DictionaryNode → Entry{key, tokens, range} │
 ├────────────────────────────────────────────────────────────────────────┤
 │ L1  Lexer with SourceLocation, quoting, comments                       │
 ├────────────────────────────────────────────────────────────────────────┤
 │ L0  Diagnostics: Diagnostic{severity, code, range, message, notes,     │
 │     hint}; DiagnosticSink; DictionaryError carrying all diagnostics    │
 └────────────────────────────────────────────────────────────────────────┘
 Legacy adapter: Dictionary::Read*(…) implemented on top of L2–L4.
```

### 4.1 Diagnostics (L0)

```cpp
struct SourceLocation { std::shared_ptr<const SourceFile> file; uint32_t line, column; };
struct SourceRange    { SourceLocation begin, end; };
enum class Severity   { Note, Warning, Error };
enum class DiagCode   { UnknownKeyword, MissingCompulsory, Conflict, RequiresMissing,
                        Cardinality, WrongTokenCount, BadNumber, BadUnit, WrongDimension,
                        OutOfRange, NotAdmissible, DanglingReference, GrammarIncoherent, ... };
struct Diagnostic     { Severity severity; DiagCode code; SourceRange where;
                        std::string message; std::vector<Diagnostic> notes; std::string hint; };
class  DictionaryError : public std::runtime_error { std::vector<Diagnostic> diags; };
```

- Diagnostics are rendered like compiler errors, with a caret under the offending token:

  ```
  input.dic:23:19: error: unit 'K' has dimension Temperature, but @Velocity expects Length/Time
      @Velocity  19.5 K;
                      ^
  ```
- When a keyword is unknown, the message suggests the closest declared keyword (edit distance): "did you mean @MoleFractions?".
- Validation runs **every** rule and gathers every error before throwing once. A `Strict` / `Lenient` policy decides whether warnings are promoted to errors.
- The library itself never writes to `std::cout`.

### 4.2 Lexer and syntax tree (L1, L2)

- Tokens carry line and column. The syntax tree keeps the original token sequence of each entry, so any value type can re-interpret it.
- Comments:
  - `//` starts a comment **only at the start of a token** (after whitespace or at the beginning of the line), which fixes D1;
  - optionally, `/* … */` block comments.
- Quoted strings, `"with spaces; @ and //"`, with `\"` escapes. Unquoted input keeps today's behaviour, so every existing `.dic` file parses the same way (to be verified on the corpus, see §5 Phase 0).
- Brace placement can be relaxed (`Dictionary x {` on one line) while staying backward compatible.
- Optional extensions, later and behind a flag: nested anonymous dictionaries; an `#include "file.dic"` directive.
- The duplicate-dictionary check covers every file loaded into a manager. When the same dictionary name appears twice, the error reports both locations.

### 4.3 Value system (L3): parsers and combinators instead of a growing enum

A value type is a small policy class with a compile-time name, and its implementation is compiled:

```cpp
template<class V> concept ValueType = requires(TokenSpan t, ParseContext& c) {
    typename V::value_type;
    { V::describe() } -> std::convertible_to<std::string_view>;   // for @Help / docs
    { V::parse(t, c) } -> std::same_as<std::optional<typename V::value_type>>; // diags into c
};
```

**Primitives:**
- `Int`, `Real`, `Bool` (case-insensitive true/false/on/off/yes/no);
- `String`, `Path` (quoted or raw; resolved relative to the input file's folder);
- `DictRef<G>` (see §4.6);
- `Enum<E>` with a constexpr table of `{"Isothermal-ConstantVolume", E::IsoV}`, aliases (`"CVODE"|"Cvode"`), and an optional `available_if(flag, "built without Sundials")`;
- `Quantity<Dim>` (see §4.5).

**Combinators:**
- `List<T>`, with optional `min_size` / `max_size` / `exact_size`;
- `Tuple<A, B, …>`, a fixed record such as `Tuple<Int, Int, Quantity<MassFlowRate>>`;
- `Repeat<Tuple<…>>`, a list of records, for example `@InternalConnections 3 7 250.3 kg/s 4 8 1.2 kg/s;`;
- `Pairs<Name, Real>`, the generalized VECTOR_STRING_DOUBLE;
- `Composition` = `Pairs<Species, Real>` plus an optional `normalized(tol)` constraint;
- `QuantityList<Dim>`, which accepts both `1000 1100 1200 K` (trailing shared unit) and `1000 K 1100 K`;
- `Table<Quantity<X>, Quantity<Y>>` for XY profiles, which rejects odd counts;
- `OneOf<Enum<Special>, List<String>>`, for "ALL | NONE | list".

**Constraints** are attached to a key and checked with the source location:
- `positive()`, `non_negative()`, `in_range(a, b)`, `in_range(a, b, open/closed)`;
- `size(n)`, `sum_to(1.0, 1e-6)`, `unique()`, `matches(regex)`;
- `exists()` for paths;
- user-defined lambdas `check([](const T&) -> std::optional<std::string>)`.

The old 18 enum values map onto these types one to one. That mapping is what makes the legacy adapter possible.

### 4.4 Grammar model, rules and coherence analysis (L4)

There is **one** grammar data model. It is filled from a C++ grammar (the typed front end, or the legacy `DictionaryKeyWord` adapter) or from a grammar file:

```cpp
struct KeySpec   { std::string_view name; const ValueTypeInfo* type; std::string_view doc;
                   Presence presence /*Optional|Compulsory*/; DefaultValue def;
                   std::vector<Constraint> constraints; Dimension dim; bool deprecated; ... };
struct Rule      { RuleKind kind; std::vector<KeyId> keys; int n = 0; Condition when; };
// RuleKind: Required, Exactly(n), AtLeast(n), AtMost(n), Requires(a → B…),
//           Conflicts(a, b…), When(cond, Rule), Deprecated(a, replacement)
```

- **Group rules replace the per-keyword lists:**
  - `exactly_one_of(@MoleFractions, @MassFractions, @Moles, @Masses, @EquivalenceRatio)` replaces 5 permuted declarations;
  - `exactly(2, @Temperature, @Pressure, @Density)` replaces the C++ code that checks it by hand;
  - `when(@Type == "NonIsothermal-UserDefinedVolume", requires(@VolumeProfile))` is a conditional rule;
  - the legacy fields are translated as follows: `compulsory_alternatives` becomes `exactly_one_of`, `required_keywords` becomes `requires` and `conflicting_keywords` becomes `conflicts`.
- **The coherence analyzer** runs whenever a grammar is built. For typed grammars it also runs at compile time (§4.7). Keyword counts are small (at most about 60), so a complete analysis is affordable:
  1. The structure is valid: names start with `@` and are unique; every rule refers to a declared key; there are no self-references.
  2. Pairwise contradictions: `requires(a,b)` together with `conflicts(a,b)`; a compulsory key that conflicts with another compulsory key; `exactly(n, S)` with `n > |S|`.
  3. **Closure reasoning:** build the transitive closure of `requires` (Floyd–Warshall over at most about 60 nodes). A key is **dead** if its closure contains two keys that conflict, contains a key that conflicts with a compulsory key, or breaks the upper bound of a cardinality rule. A dead key is reported as a *grammar* error, never as a user error.
  4. **Satisfiability:** check that at least one keyword set satisfies every rule. A tiny DPLL search over the keys that actually take part in rules is enough. Groups are small, so it finishes in microseconds.
  5. Defaults are coherent: the default parses against the type; it satisfies the constraints and the enum values; it has the right dimension; a compulsory key with a default is flagged as a warning.
  6. Overlapping rules are reported as warnings (for example, a key both compulsory and in an `exactly_one_of` group).
  7. Optionally, across grammars: the same key name declared with incompatible types in grammars that are registered together. This would have caught the 17 inconsistent names and the duplicated `Grammar_GasStatus`.
- **Usage tracking:** the view records which keys were read. `report_unused()` emits warnings for keys that are present in the file but that the solver never consumed. This catches mismatches such as the LaminarFlamelets one even in legacy code.
- **`@Help`** stops being a side effect that terminates the program. It becomes `grammar.describe(format)`, which produces text, Markdown or JSON from the model: type, units, default, admissible values and rules. The same output can generate documentation for every solver.

### 4.5 Units of measure

**Dimensions** are a constexpr vector of rational exponents over SI base quantities plus mole: `L, M, T, Θ, N, I, J`. For example:

```cpp
namespace dim {
  inline constexpr Dimension Length{1,0,0,0,0}, Time{0,0,1,0,0}, Temperature{0,0,0,1,0}, ...;
  inline constexpr Dimension Velocity = Length / Time;
  inline constexpr Dimension HeatTransferCoeff = Power / Area / Temperature;
  inline constexpr Dimension Dimensionless{};
}
// declaration in a grammar:
Key<"@FuelVelocity", Quantity<dim::Velocity>>       // file: 19.5 cm/s  → 0.195  (m/s)
Key<"@SootDensity",  Quantity<dim::Density>>        // file: 1800 kg/m3 → 1800
Key<"@EigenValueStartingGuess", Quantity<dim::Mass / (dim::Length^3) / (dim::Time^2)>>
```

**The unit-expression parser** is runtime and compiled. Units are always data in the input file, so they cannot be checked at compile time.
- **Grammar:** `unit := factor (('/'|'.'|'*') factor)*` and `factor := prefix? symbol exponent?`. It accepts `kg/m3/s2`, `W/m2/K`, `J/mol/K`, `cm3/mol/s`, `kg.m-3` and `1/s`.
- **Registry:**
  - SI symbols with prefixes (`mm`, `µm`/`um`, `kPa`, `ms`);
  - common non-SI units: `atm`, `bar`, `torr`, `psi`, `cal`, `kcal`, `BTU`, `eV`, `L`/`l`, `min`, `h`/`hr`, `micron`, `in`, `ft`, `lb`, `deg`, `rpm`;
  - **aliases are decided once**, centrally (`h` = `hr`, `C` = `°C` = `degC`).
- **The result** is `{factor, offset, Dimension}`. The library checks it against the declared dimension and converts to SI: `si = value*factor + offset`.
- **Affine units:** an offset unit (°C, °F) is accepted only for `AbsoluteTemperature`. A `TemperatureDifference` uses the factor only. This removes the 273-versus-273.15 bugs and the "is this an offset or a factor" question.
- **Ambiguous combinations are rejected** with a helpful message, for example a missing time dimension in `kcal/m2/K` against `HeatTransferCoeff`.
- **Dimensionless quantities:**
  - `Quantity<dim::Dimensionless>` accepts a bare number, or optionally `%`, `ppm` or `-`;
  - plain `Real` stays for pure numbers;
  - a dimensionless unit such as `@FilterWidth 1 dimensionless` becomes explicit.
- **Return type:** `get()` returns a `double` in SI by default, which stays compatible with existing solver code. You can opt in to `get_as(key, "cm/s")` (converted to that unit) and `get_raw(key)` (value, unit string and source range) for logging.
- **Dependencies:** I recommend a small in-house unit module (about 600–900 lines) rather than mp-units or Au. Those are heavyweight C++20 dependencies whose strength is compile-time unit *arithmetic*, which you don't need here. The new module replaces `Conversions.h` in OpenSMOKE++2. The unit table is cross-checked in tests against the existing constants, which also catches the `m_from_km` bug.

### 4.6 Typed sub-dictionary references and the manager

- `Key<"@InitialStatus", DictRef<GasStatusGrammar>>` means that the referenced dictionary is **always** validated against `GasStatusGrammar`, whoever reads it.
- `manager.validate<RootGrammar>("BatchReactor")` walks the whole reference graph. It reports:
  - dangling references, with their location;
  - dictionaries that nothing references (a warning, which catches typos in dictionary names);
  - cycles;
  - one dictionary referenced with two different grammars (an error).
- Readers receive a typed `View<G>` and no longer need the manager to resolve the next level. This removes the 3-line pattern repeated 277 times.
- Implicit naming conventions (`"Surface-" + phase`) are supported explicitly: `manager.view<G>(name)` validates, and the not-found diagnostic lists the names that were expected.

### 4.7 The typed, compile-time front end (L5)

This layer answers both of your examples:
- **(1) A wrong keyword fails to compile**, because the key must belong to the grammar.
- **(2) A wrong returned type fails to compile**, because the type is deduced from the key.

```cpp
// --- grammar definition (header) ---------------------------------------
namespace gas_status {
  using namespace dp;                                     // dictionary-parser namespace
  inline constexpr auto T   = key<"@Temperature", Quantity<dim::AbsoluteTemperature>>("Temperature");
  inline constexpr auto P   = key<"@Pressure",    Quantity<dim::Pressure>>("Pressure");
  inline constexpr auto Rho = key<"@Density",     Quantity<dim::Density>>("Density");
  inline constexpr auto X   = key<"@MoleFractions", Composition>("Mole fractions").normalized(1e-6);
  inline constexpr auto Y   = key<"@MassFractions", Composition>("Mass fractions").normalized(1e-6);
  inline constexpr auto Phi = key<"@EquivalenceRatio", Real>("Equivalence ratio").positive();

  struct Grammar : dp::Grammar<Grammar> {
    static constexpr auto keys  = keyset(T, P, Rho, X, Y, Phi /*, ...*/);
    static constexpr auto rules = ruleset(
        exactly(2, T, P, Rho),
        exactly_one_of(X, Y, Phi),
        requires_(Phi, FuelX | FuelY, OxidX | OxidY));
    // static_assert(dp::coherent<Grammar>()) is performed by the base:
    // duplicates, undeclared keys in rules, self-reference, requires∩conflicts,
    // dead keys, unsatisfiable groups, defaults violating constraints.
  };
}

// --- usage ----------------------------------------------------------------
auto mix = manager.view<gas_status::Grammar>("initial-mixture"); // validated once, all errors collected
double T  = mix.get(gas_status::T);                // SI kelvin, type deduced
auto   X  = mix.get_if(gas_status::X);             // std::optional<Composition>
double Sc = soot.get_or(three_eq::Schmidt, 50.0);  // or the grammar's declared default: soot.get(three_eq::Schmidt)

mix.get(ode::RelTol);          // ✗ static_assert: key "@RelativeTolerance" is not part of gas_status::Grammar
int bad = mix.get(gas_status::T); // ✗ no implicit narrowing from Quantity to int (or a deduced-type mismatch with auto)
```

**Techniques involved:**
- C++20 class-type NTTP string literals (`fixed_string`) let the keyword name be a template parameter.
- `keyset(...)` is a constexpr tuple. Membership in `View<G>::get<K>()` is checked with `static_assert` and a readable message.
- The coherence analysis of §4.4 is written as `constexpr` functions over fixed-size arrays. They are evaluated in `static_assert` for typed grammars and reused unchanged at run time for grammar files and legacy grammars.

**Where compile-time checking stops.** Anything that depends on the input file stays a runtime check that collects diagnostics:
- whether a keyword is present;
- unit strings;
- number syntax;
- whether references resolve.

**Binary/header split** (AI_CONTEXT §2):
- The typed layer is thin: keys and grammars become pointers to constexpr `KeySpec` / `Rule` tables.
- `get()` dispatches to compiled, non-template functions that work on `ValueTypeInfo`, so lexing, unit parsing, validation and rule evaluation all stay in `src/`.
- The only templates that get instantiated are small accessors, plus the `ValueType` policies for user-defined enums and records. Use `extern template` for the built-in ones.

### 4.8 Legacy adapter

- `Dictionary`, `DictionaryGrammar` and `DictionaryKeyWord` keep their current signatures:
  - `AddKeyWord(DictionaryKeyWord(...))` translates into `KeySpec` plus rules;
  - `Read*` / `ReadOption` call into the new value system;
  - `ReadMeasure` keeps returning the raw unit, and a new `ReadMeasure(name, Dimension) -> double` is added.
- With a single switch, the legacy path gets the new collected diagnostics and the coherence analyzer. Every existing grammar is checked the first time it is used. Expect this to surface real issues in the 99 OpenSMOKE++2 grammars.
- Mark the legacy API `[[deprecated]]` only at the end of the migration.

---

## 5. Phased plan

Each phase leaves the library in a releasable state, and OpenSMOKE++2 keeps building throughout.

**Phase 0: safety net (small)**
- Unit tests for the lexer, the parser, each `Read*` function and each grammar check. Include **negative** tests for every defect in §2.1.
- A **corpus test** that parses all 144 `.dic` files of OpenSMOKE++2 and stores a normalized dump (keyword, tokens, lines) as golden output. This guards the backward compatibility of every later lexer change.
- CI with `-Wall -Wextra -Werror`, ASan/UBSan, and at least clang and gcc (plus Intel/MSVC if they are targets).
- Choose the language baseline: **C++20 plus a small `expected` polyfill**, or **C++23** (`std::expected`, `std::format`). Your HPC compilers decide this. The design works with either.

**Phase 1: quick fixes in the current code (small)**
- D1 (`//` only at token start), D2 (a clear "no grammar attached" message), D3 (the stronger `Check()`), D4, D5 (gather messages into the exception instead of `cout`), D6, D7, D8.
- These fixes help OpenSMOKE++2 right away, and none of them breaks the API.

**Phase 2: diagnostics, lexer and syntax tree (L0–L2) (medium)**
- Tokens with locations, quoting, the syntax tree, compiler-style rendering, "did you mean".
- `Dictionary` is reimplemented on top of the syntax tree. The corpus test must stay unchanged.

**Phase 3: grammar model, rule engine and coherence analyzer (L4, runtime) (medium)**
- Group, cardinality and conditional rules; defaults; deprecations; usage tracking; `describe()`.
- The legacy `DictionaryKeyWord` and grammar files are translated into the model.
- A new grammar-file format (simple and declarative, one block per key with named fields in any order) can be added alongside the 7-line format.

**Phase 4: value system and combinators (L3) (medium)**
- The primitives, `Enum<E>`, `List`, `Tuple`, `Repeat`, `Pairs`/`Composition`, `QuantityList`, `Table`, `OneOf`, and constraints.
- Non-throwing `try_get` for every type, which replaces the double-only `TryReadDouble`.

**Phase 5: units (medium)**
- The dimension algebra, the unit parser and registry, affine temperature handling, `Quantity<Dim>` and `ReadMeasure(name, dim)`.
- Tests cross-check against `Conversions.h` and catch its known bug.

**Phase 6: typed compile-time front end (L5) (medium–large)**
- `fixed_string` keys, `Grammar<>` with `static_assert` coherence, `View<G>`, `DictRef<G>` and `manager.validate<Root>()`.
- **Compile-fail tests** (CTest targets marked `WILL_FAIL`, or `try_compile`) prove that a wrong keyword and a wrong type are rejected.

**Phase 7: pilot migration in OpenSMOKE++2**
- Pick representative pieces: `Grammar_GasStatus` with `GetGasStatusFromDictionary` (the hardest rules and units), `OdeSolver_Parameters` (enums with feature gating), and `BatchReactor` (sub-dictionaries and time units).
- Measure how many lines and how many `units ==` comparisons disappear.
- Remove the duplicated grammars and the clashing header guards.
- Then migrate solver by solver, and deprecate the legacy API at the end.

---

## 6. Risks and open decisions

1. **C++20 versus C++23 baseline.** This affects `std::expected` and `std::format`. Settle it in Phase 0, based on the oldest compiler used for OpenSMOKE++2 builds.
2. **Template code versus the precompiled-binary policy.** The typed layer has to live in headers. The design keeps it thin, so only key descriptors and accessors are header code.
3. **Lexer changes and backward compatibility.** `//` handling and quoting affect every input file. The corpus test is the gate.
4. **Unit aliases are a policy decision.** For example: is `C` Celsius or coulomb? (Celsius, in the context of the temperature dimension.) Is `l` litre? Is `M` molar? Decide once and document the decision.
5. **Strictness rollout.** Unknown-unit, unused-key and dead-key warnings could break existing inputs if they are promoted to errors too early. Use a policy flag, starting lenient.
6. **Units on the output side.** Should the library also format values back to user units in reports? This is not required now, but `get_raw` keeps the option open.
