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

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <utility>

namespace OpenSMOKEpp {
namespace {
[[nodiscard]] auto render(const std::vector<Diagnostic> &diagnostics,
                          const std::string &context) -> std::string {
  std::string text;
  if (!context.empty()) {
    std::size_t errors = 0;
    for (const auto &diagnostic : diagnostics)
      if (diagnostic.severity == Severity::Error)
        ++errors;
    text = context;
    if (diagnostics.size() > 1 || errors != diagnostics.size())
      text += ": " + std::to_string(errors) +
              (errors == 1 ? " error" : " errors") + " found";
    text += '\n';
  }
  for (std::size_t i = 0; i < diagnostics.size(); ++i) {
    if (i != 0)
      text += '\n';
    text += format_diagnostic(diagnostics[i]);
  }
  return text;
}

void append_one(std::string &out, const Diagnostic &diagnostic,
                const bool with_excerpt) {
  const SourceLocation &where = diagnostic.location;
  if (!where.file_name.empty()) {
    out += where.file_name;
    if (where.line != 0) {
      out += ':' + std::to_string(where.line);
      if (where.column != 0)
        out += ':' + std::to_string(where.column);
    }
    out += ": ";
  }
  out += std::string(to_string(diagnostic.severity)) + ": " +
         diagnostic.message;

  const std::string gutter_blank = "      | ";
  bool excerpt = false;
  if (with_excerpt && where.source && where.line != 0 &&
      where.line <= where.source->number_of_lines()) {
    std::string line(where.source->line(where.line));
    std::replace(line.begin(), line.end(), '\t', ' ');
    std::string number = std::to_string(where.line);
    if (number.size() < 5)
      number.insert(0, 5 - number.size(), ' ');
    out += '\n' + number + " | " + line;
    if (where.column != 0 && where.column <= line.size() + 1) {
      const std::size_t available = line.size() + 1 - where.column;
      const std::size_t length = std::max<std::size_t>(
          1, std::min<std::size_t>(where.length, available));
      out += '\n' + gutter_blank + std::string(where.column - 1, ' ') + '^' +
             std::string(length - 1, '~');
    }
    excerpt = true;
  }
  if (!diagnostic.hint.empty())
    out += std::string(excerpt ? "\n      = " : "\n  ") + "hint: " +
           diagnostic.hint;
}
} // namespace

SourceFile::SourceFile(std::string name, std::string text)
    : name_(std::move(name)), text_(std::move(text)) {
  line_starts_.push_back(0);
  for (std::size_t i = 0; i < text_.size(); ++i)
    if (text_[i] == '\n' && i + 1 < text_.size())
      line_starts_.push_back(i + 1);
}

std::string_view SourceFile::line(const unsigned int number) const noexcept {
  if (number == 0 || number > line_starts_.size())
    return {};
  const std::size_t begin = line_starts_[number - 1];
  std::size_t end = text_.find('\n', begin);
  if (end == std::string::npos)
    end = text_.size();
  if (end > begin && text_[end - 1] == '\r')
    --end;
  return std::string_view(text_).substr(begin, end - begin);
}

auto to_string(const DiagnosticCode code) noexcept -> std::string_view {
  switch (code) {
  case DiagnosticCode::Syntax: return "syntax";
  case DiagnosticCode::Include: return "include";
  case DiagnosticCode::DuplicateDictionary: return "duplicate-dictionary";
  case DiagnosticCode::UnknownKeyword: return "unknown-keyword";
  case DiagnosticCode::MissingCompulsory: return "missing-compulsory";
  case DiagnosticCode::AlternativeGroup: return "alternative-group";
  case DiagnosticCode::MissingRequired: return "missing-required";
  case DiagnosticCode::Conflict: return "conflict";
  case DiagnosticCode::HelpRequested: return "help-requested";
  case DiagnosticCode::InvalidValue: return "invalid-value";
  case DiagnosticCode::InvalidUnit: return "invalid-unit";
  case DiagnosticCode::NoGrammar: return "no-grammar";
  case DiagnosticCode::UndeclaredKeyword: return "undeclared-keyword";
  case DiagnosticCode::WrongType: return "wrong-type";
  case DiagnosticCode::MissingKeyword: return "missing-keyword";
  case DiagnosticCode::GrammarIncoherent: return "grammar-incoherent";
  case DiagnosticCode::Cardinality: return "cardinality";
  case DiagnosticCode::Forbidden: return "forbidden";
  case DiagnosticCode::NotAllowed: return "not-allowed";
  case DiagnosticCode::OutOfRange: return "out-of-range";
  case DiagnosticCode::Deprecated: return "deprecated";
  case DiagnosticCode::UnusedKeyword: return "unused-keyword";
  case DiagnosticCode::UnknownDictionary: return "unknown-dictionary";
  case DiagnosticCode::GrammarMismatch: return "grammar-mismatch";
  case DiagnosticCode::ReferenceCycle: return "reference-cycle";
  case DiagnosticCode::UnreferencedDictionary: return "unreferenced-dictionary";
  case DiagnosticCode::Other: return "other";
  }
  return "other";
}

auto to_string(const Severity severity) noexcept -> std::string_view {
  switch (severity) {
  case Severity::Error: return "error";
  case Severity::Warning: return "warning";
  case Severity::Note: return "note";
  }
  return "error";
}

auto format_diagnostic(const Diagnostic &diagnostic, const bool with_excerpt)
    -> std::string {
  std::string out;
  append_one(out, diagnostic, with_excerpt);
  for (const auto &note : diagnostic.notes) {
    out += '\n';
    append_one(out, note, with_excerpt);
  }
  return out;
}

DictionaryError::DictionaryError(std::vector<Diagnostic> diagnostics,
                                 std::string context)
    : std::runtime_error(render(diagnostics, context)),
      diagnostics_(std::move(diagnostics)), context_(std::move(context)) {}

std::size_t DictionaryError::error_count() const noexcept {
  return static_cast<std::size_t>(
      std::count_if(diagnostics_.begin(), diagnostics_.end(),
                    [](const Diagnostic &d) {
                      return d.severity == Severity::Error;
                    }));
}

bool DictionaryError::has(const DiagnosticCode code) const noexcept {
  return std::any_of(diagnostics_.begin(), diagnostics_.end(),
                     [code](const Diagnostic &d) { return d.code == code; });
}
namespace {
ErrorHandlerOptions error_handler_options;
std::terminate_handler previous_terminate_handler = nullptr;
bool error_handler_installed = false;

[[noreturn]] void dictionary_terminate_handler() {
  if (const std::exception_ptr error = std::current_exception()) {
    try {
      std::rethrow_exception(error);
    } catch (const DictionaryError &dictionary_error) {
      std::ostream &out = error_handler_options.stream != nullptr
                              ? *error_handler_options.stream
                              : std::cerr;
      out << '\n' << error_handler_options.title << '\n'
          << dictionary_error.what() << std::endl;
      if (error_handler_options.wait_for_enter) {
        out << "Press enter to exit..." << std::endl;
        (void)std::getchar();
      }
      std::exit(error_handler_options.exit_code);
    } catch (...) {
    }
  }
  if (previous_terminate_handler != nullptr)
    previous_terminate_handler();
  std::abort();
}
} // namespace

void InstallErrorHandler(ErrorHandlerOptions options) {
  error_handler_options = std::move(options);
  if (!error_handler_installed) {
    previous_terminate_handler = std::set_terminate(dictionary_terminate_handler);
    error_handler_installed = true;
  }
}

} // namespace OpenSMOKEpp
