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
| Validation | Grammar layer for required keywords, conflicts, alternatives, and typed reads, with a coherence analysis of the grammar itself |
| Error handling | Standard exceptions; all problems reported together, `file:line:column` locations, "did you mean" suggestions; nothing printed by the library |
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

Syntax errors are thrown as `OpenSMOKEpp::Lexer::SyntaxError` (derived from
`std::runtime_error`), which also exposes the file, line and column:

```text
input.dic:23:5: error: unexpected keyword @Pressure: missing ';' at the end of keyword @Temperature (line 22)
```

Grammar validation reports every problem at once in a single exception:

```text
Dictionary initial-mixture defined in file input.dic: 2 errors found:
  - the keyword @Temprature is not recognized in this dictionary (did you mean @Temperature?)
  - the compulsory keyword @Temperature is missing
```

## Quick Start

The example below parses `examples/input.1.dic`, retrieves the `dae-parameters` dictionary, applies a minimal grammar, and reads typed values safely.

```cpp
#include "Dictionary.h"
#include "DictionaryGrammar.h"
#include "DictionaryKeyWord.h"
#include "DictionaryManager.h"

#include <exception>
#include <iostream>
#include <string>

class DaeParametersGrammar final : public OpenSMOKEpp::DictionaryGrammar
{
protected:
    void DefineRules() override
    {
        AddKeyWord(OpenSMOKEpp::DictionaryKeyWord(
            "@DaeSolver",
            OpenSMOKEpp::SINGLE_STRING,
            "DAE solver name",
            false));

        AddKeyWord(OpenSMOKEpp::DictionaryKeyWord(
            "@RelativeTolerance",
            OpenSMOKEpp::SINGLE_DOUBLE,
            "Relative tolerance",
            false));

        AddKeyWord(OpenSMOKEpp::DictionaryKeyWord(
            "@AbsoluteTolerance",
            OpenSMOKEpp::SINGLE_DOUBLE,
            "Absolute tolerance",
            false));
    }
};

int main()
{
    try
    {
        OpenSMOKEpp::DictionaryManager manager;
        manager.ReadDictionariesFromFile("examples/input.1.dic");

        auto& dae = manager("dae-parameters");

        DaeParametersGrammar grammar;
        dae.SetGrammar(grammar);

        std::string solver;
        double relative_tolerance = 0.0;
        double absolute_tolerance = 0.0;

        dae.ReadString("@DaeSolver", solver);
        dae.ReadDouble("@RelativeTolerance", relative_tolerance);
        dae.ReadDouble("@AbsoluteTolerance", absolute_tolerance);

        std::cout << "Solver: " << solver << '\n';
        std::cout << "Relative tolerance: " << relative_tolerance << '\n';
        std::cout << "Absolute tolerance: " << absolute_tolerance << '\n';
    }
    catch (const std::exception& error)
    {
        std::cerr << "Dictionary parsing failed: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
```

### Grammar Coherence

When a grammar is built, its rules are checked before any dictionary is
validated. The grammar is rejected (with every problem listed) if, for example:

- a keyword name does not start with `@` or is defined twice;
- a rule references an undefined keyword or the keyword itself;
- a keyword both requires and conflicts with another keyword;
- compulsory alternatives do not form a closed group, or a conflict is not
  declared on both keywords;
- following `required_keywords` transitively, a keyword would force two
  conflicting keywords, or two members of the same alternative group, so it
  can never be used;
- no input could ever satisfy the grammar.

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

- `Unit_*`: lexer, syntax, `#include`, typed reads, grammar coherence and
  dictionary validation (`unit_tests [suite...]` runs selected suites).
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
| `DictionaryKeyWord.h` | Keyword metadata and expected value types |
| `DictionaryInputFile.h` | Preprocessed input-file representation |
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
