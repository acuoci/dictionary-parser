# OpenSMOKE++ Dictionary Parser

**A standalone, high-performance, modern C++ library for parsing and processing structured input files according to rigorous dictionary syntax rules.**

OpenSMOKE++ Dictionary Parser provides a compact parser for `Dictionary ... { ... }` input files with keyword-based options, multiline entries, comments, source-line diagnostics, and optional grammar validation. The parser core is fully decoupled from Boost, Eigen, and the wider OpenSMOKE++ framework.

## Highlights

| Area | Description |
| --- | --- |
| Language standard | Pure modern C++ with C++20/C++23-ready interfaces |
| Dependencies | Zero external runtime or parser dependencies |
| Architecture | Public headers in `include/`, private compiled implementation in `src/` |
| Parsing model | Dictionary blocks, `@Keyword` entries, semicolon terminators, multiline values |
| Validation | Optional grammar layer for required keywords, conflicts, alternatives, and typed reads |
| Error handling | Standard exceptions with source-aware diagnostics |
| Tests | CTest smoke tests over real input files in `examples/` |
| Documentation | Public headers documented with Doxygen comments |
| License | GNU General Public License v3 |

## Repository Layout

```text
.
├── include/                 # Public API headers
├── src/                     # Private implementation files
├── examples/                # Example dictionary input files
├── tests/                   # Integration smoke tests
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
- `{` and `}` delimit the dictionary body.
- Options begin with `@`.
- Every option terminates with `;`.
- Values are whitespace-tokenized.
- `//` starts a line comment.
- Keyword entries may span multiple physical lines until the terminating `;`.

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

## Build

### Requirements

- CMake 3.30 or newer
- A C++20-capable compiler
- Standard C++ library with `<filesystem>` and `<charconv>` support

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

The repository includes a lightweight integration test harness based on CTest.

From the build directory:

```bash
ctest --output-on-failure
```

Current tests parse the input files in `examples/`, including a grammar-aware test for `input3.dic`.

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
| `DictionaryManager.h` | Reads files and owns parsed dictionaries |
| `Dictionary.h` | Provides keyword lookup, grammar validation, and typed reads |
| `DictionaryGrammar.h` | Base class for user-defined grammar rules |
| `DictionaryKeyWord.h` | Keyword metadata and expected value types |
| `DictionaryInputFile.h` | Preprocessed input-file representation |
| `DictionaryFile.h` | Single dictionary-block parser |
| `DictionaryLexer.h` | Low-level lexical utilities |

## Examples

Example input files are available in:

```text
examples/
├── input.1.dic
├── input.2.dic
└── input3.dic
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
