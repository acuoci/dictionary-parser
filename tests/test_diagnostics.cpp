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

#include "DictionaryDiagnostics.h"
#include "DictionaryUnits.h"
#include "test_framework.h"
#include "test_support.h"

#include <memory>
#include <string>
#include <vector>

using namespace OpenSMOKEpp;
using testing::attach;
using testing::kw;
using testing::parse;

namespace {
auto located(const std::shared_ptr<const SourceFile> &source, unsigned line,
             unsigned column, unsigned length) -> SourceLocation {
  return SourceLocation{source, source->name(), line, column, length};
}

template <typename F> auto catch_error(F &&f) -> DictionaryError {
  try {
    f();
  } catch (const DictionaryError &error) {
    return error;
  }
  throw std::runtime_error("no DictionaryError was thrown");
}
} // namespace

// ---------------------------------------------------------------------------
// Source files and rendering
// ---------------------------------------------------------------------------
TEST_CASE("diagnostics", "source lines with LF, CRLF and no final newline") {
  const SourceFile source("a.dic", "first\r\nsecond\n\nlast");
  CHECK(source.number_of_lines() == 4);
  CHECK(source.line(1) == "first");
  CHECK(source.line(2) == "second");
  CHECK(source.line(3) == "");
  CHECK(source.line(4) == "last");
  CHECK(source.line(0) == "");
  CHECK(source.line(5) == "");
}

TEST_CASE("diagnostics", "compiler-style rendering with excerpt, hint and notes") {
  const auto source = std::make_shared<const SourceFile>(
      "flame.dic", "Dictionary flame\n{\n\t@Velocity  19.5 K;\n\t@Lean true;\n}\n");
  Diagnostic diagnostic{Severity::Error, DiagnosticCode::InvalidUnit,
                        located(source, 3, 18, 1), "wrong units", "use m/s",
                        {}};
  diagnostic.notes.push_back(Diagnostic{Severity::Note, DiagnosticCode::Other,
                                        located(source, 4, 2, 5),
                                        "related keyword", {}, {}});
  CHECK(format_diagnostic(diagnostic) ==
        "flame.dic:3:18: error: wrong units\n"
        "    3 |  @Velocity  19.5 K;\n"
        "      |                  ^\n"
        "      = hint: use m/s\n"
        "flame.dic:4:2: note: related keyword\n"
        "    4 |  @Lean true;\n"
        "      |  ^~~~~");
  CHECK(format_diagnostic(diagnostic, false) ==
        "flame.dic:3:18: error: wrong units\n"
        "  hint: use m/s\n"
        "flame.dic:4:2: note: related keyword");
}

TEST_CASE("diagnostics", "locations without source, line or file") {
  CHECK(format_diagnostic(Diagnostic{Severity::Error, DiagnosticCode::Other,
                                     SourceLocation{nullptr, "x.dic", 7, 3, 2},
                                     "message", {}, {}}) ==
        "x.dic:7:3: error: message");
  CHECK(format_diagnostic(Diagnostic{Severity::Warning, DiagnosticCode::Other,
                                     SourceLocation{nullptr, "x.dic", 0, 0, 0},
                                     "message", {}, {}}) ==
        "x.dic: warning: message");
  CHECK(format_diagnostic(Diagnostic{Severity::Error, DiagnosticCode::Other,
                                     {}, "message", {}, {}}) ==
        "error: message");
}

TEST_CASE("diagnostics", "the marker is clipped to the end of the line") {
  const auto source = std::make_shared<const SourceFile>("a.dic", "@X 1;");
  CHECK(format_diagnostic(Diagnostic{Severity::Error, DiagnosticCode::Other,
                                     located(source, 1, 4, 50), "m", {}, {}}) ==
        "a.dic:1:4: error: m\n"
        "    1 | @X 1;\n"
        "      |    ^~");
}

TEST_CASE("diagnostics", "DictionaryError layout and queries") {
  const DictionaryError single(
      {Diagnostic{Severity::Error, DiagnosticCode::Conflict, {}, "one", {}, {}}},
      "Context");
  CHECK(std::string(single.what()) == "Context\nerror: one");
  CHECK(single.error_count() == 1);
  CHECK(single.has(DiagnosticCode::Conflict));
  CHECK(!single.has(DiagnosticCode::Syntax));

  const DictionaryError several(
      {Diagnostic{Severity::Error, DiagnosticCode::Other, {}, "a", {}, {}},
       Diagnostic{Severity::Error, DiagnosticCode::Other, {}, "b", {}, {}},
       Diagnostic{Severity::Note, DiagnosticCode::Other, {}, "c", {}, {}}},
      "Context");
  CHECK(std::string(several.what()) ==
        "Context: 2 errors found\nerror: a\nerror: b\nnote: c");
  CHECK(several.error_count() == 2);

  const DictionaryError bare(
      {Diagnostic{Severity::Error, DiagnosticCode::Other, {}, "x", {}, {}}});
  CHECK(std::string(bare.what()) == "error: x");
  CHECK(to_string(DiagnosticCode::UnknownKeyword) == "unknown-keyword");
  CHECK(to_string(Severity::Warning) == "warning");
}

// ---------------------------------------------------------------------------
// Diagnostics produced by the library
// ---------------------------------------------------------------------------
TEST_CASE("diagnostics", "syntax errors are DictionaryErrors with a location") {
  const auto error =
      catch_error([] { (void)parse("Dictionary d\n{\n  @X \"abc\n}\n"); });
  REQUIRE(error.diagnostics().size() == 1);
  const Diagnostic &d = error.diagnostics().front();
  CHECK(d.code == DiagnosticCode::Syntax);
  CHECK(d.location.file_name == "test.dic");
  CHECK(d.location.line == 3);
  CHECK(d.location.column == 6);
  CHECK(d.location.source != nullptr);
}

TEST_CASE("diagnostics", "include errors have their own code") {
  const auto error = catch_error(
      [] { (void)parse("#include \"missing-file.dic\"\nDictionary d { @X 1; }"); });
  CHECK(error.has(DiagnosticCode::Include));
  CHECK(error.diagnostics().front().location.line == 1);
}

TEST_CASE("diagnostics", "value errors point at the offending token") {
  auto manager = parse("Dictionary d\n"
                       "{\n"
                       "  @Values  1 2 x 4;\n"
                       "  @Times   1 2 3 parsec;\n"
                       "  @Pairs   1 m 2 Kg;\n"
                       "}\n");
  auto &d = manager("d");

  // Found when the grammar is attached.
  const auto attached = catch_error([&] {
    attach(d, {kw("@Values", VECTOR_DOUBLE), kw("@Times", VECTOR_MEASURE),
               kw("@Pairs", VECTOR_MEASURE)});
  });
  REQUIRE(attached.diagnostics().size() == 1);
  CHECK(attached.diagnostics().front().code == DiagnosticCode::InvalidValue);
  CHECK(attached.diagnostics().front().location.line == 3);
  CHECK(attached.diagnostics().front().location.column == 16);
  CHECK(std::string(attached.what()).find("      |                ^") !=
        std::string::npos);

  // Unit errors point at the unit token.
  const auto units = catch_error([&] {
    attach(d, {kw("@Values", VECTOR_STRING),
               kw("@Times", VECTOR_MEASURE).SetDimension(units::dim::Time),
               kw("@Pairs", VECTOR_MEASURE).SetDimension(units::dim::Length)});
  });
  REQUIRE(units.diagnostics().size() == 2);
  for (const auto &diagnostic : units.diagnostics()) {
    CHECK(diagnostic.code == DiagnosticCode::InvalidUnit);
    CHECK(diagnostic.location.column == 18);
  }
  CHECK(units.diagnostics().front().location.length == 6);
}

TEST_CASE("diagnostics", "programming errors have dedicated codes") {
  auto manager = parse("Dictionary d { @T 300 K; }");
  auto &d = manager("d");
  CHECK(catch_error([&] { (void)d.Read<double>("@T"); })
            .has(DiagnosticCode::NoGrammar));
  attach(d, {kw("@T", SINGLE_MEASURE), kw("@P", SINGLE_MEASURE)});
  CHECK(catch_error([&] { (void)d.Read<double>("@Typo"); })
            .has(DiagnosticCode::UndeclaredKeyword));
  // Without a dimension, a measure is a (value, units) record.
  CHECK(catch_error([&] { (void)d.Read<double>("@T"); })
            .has(DiagnosticCode::WrongType));
  const auto missing = catch_error([&] { (void)d.Read<double>("@P"); });
  CHECK(missing.has(DiagnosticCode::MissingKeyword));
  CHECK(missing.diagnostics().front().location.line == 1); // dictionary name
}

TEST_CASE("diagnostics", "grammar incoherence has its own code") {
  auto manager = parse("Dictionary d { @A 1; }");
  const auto error = catch_error([&] {
    attach(manager("d"), {kw("@A", SINGLE_INT, false, "none", "none", "@A")});
  });
  CHECK(error.has(DiagnosticCode::GrammarIncoherent));
  CHECK(error.context() == "Grammar of dictionary d");
}

TEST_CASE("diagnostics", "entries and locations are exposed") {
  auto manager = parse("Dictionary d\n{\n  @A 1;\n  @B \"x y\" z;\n}\n");
  const auto &d = manager("d");
  REQUIRE(d.entries().size() == 2);
  const auto &b = d.entries()[1];
  CHECK(b.keyword == "@B");
  CHECK(b.option == "\"x y\" z");
  CHECK(b.values == (std::vector<std::string>{"x y", "z"}));
  REQUIRE(b.tokens.size() == 2);
  CHECK(b.tokens[0].kind == Lexer::TokenKind::QuotedString);
  CHECK(b.tokens[1].column == 12);
  CHECK(d.location_of("@B").line == 4);
  CHECK(d.location_of("@B").column == 3);
  CHECK(d.location_of("@Missing").line == 1);
  CHECK(d.location().column == 12);
}

TEST_CASE("diagnostics", "dictionaries built without a source still name the line") {
  Dictionary d;
  d.SetName("legacy");
  d.SetFileName("old.dic");
  d.SetDictionary({"@N"}, {"abc"}, {12}, {12});
  CHECK_THROWS_WITH(attach(d, {kw("@N", SINGLE_INT)}),
                    "old.dic:12: error: keyword @N: the value must be an "
                    "integer, but 'abc' was given");
}
