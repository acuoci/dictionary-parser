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

#ifndef OpenSMOKEpp_DictionaryDiagnostics_H
#define OpenSMOKEpp_DictionaryDiagnostics_H

#include <cstddef>
#include <iosfwd>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace OpenSMOKEpp {
/**
 * \brief Immutable text of one input file, shared by every object that
 * refers to positions inside it.
 *
 * Diagnostics keep a `std::shared_ptr` to the source so that the offending
 * line can be printed long after parsing (for example when a value is read).
 */
class SourceFile {
public:
  /**
   * \brief Stores a source text.
   *
   * \param[in] name File name used in diagnostics.
   * \param[in] text Full file content.
   */
  SourceFile(std::string name, std::string text);

  /** \brief Returns the file name used in diagnostics. */
  [[nodiscard]] const std::string &name() const noexcept { return name_; }

  /** \brief Returns the full file content. */
  [[nodiscard]] const std::string &text() const noexcept { return text_; }

  /** \brief Returns the number of lines. */
  [[nodiscard]] std::size_t number_of_lines() const noexcept {
    return line_starts_.size();
  }

  /**
   * \brief Returns one line without its end-of-line characters.
   *
   * \param[in] number 1-based line number.
   * \return The line, or an empty view if `number` is out of range.
   */
  [[nodiscard]] std::string_view line(unsigned int number) const noexcept;

private:
  std::string name_;
  std::string text_;
  std::vector<std::size_t> line_starts_;
};

/**
 * \brief Position of a token (or of a whole construct) in an input file.
 */
struct SourceLocation {
  /** \brief Source text, when available (enables line excerpts). */
  std::shared_ptr<const SourceFile> source;
  /** \brief File name; empty when unknown. */
  std::string file_name;
  /** \brief 1-based line; 0 when unknown. */
  unsigned int line = 0;
  /** \brief 1-based column (bytes); 0 when unknown. */
  unsigned int column = 0;
  /** \brief Length of the highlighted text in bytes (at least 1 when shown). */
  unsigned int length = 0;

  /** \brief Returns `true` if a line number is known. */
  [[nodiscard]] bool has_position() const noexcept { return line != 0; }
};

/**
 * \brief Severity of a diagnostic.
 */
enum class Severity {
  /** \brief The input or the program is invalid. */
  Error,
  /** \brief Suspicious but accepted. */
  Warning,
  /** \brief Additional information attached to another diagnostic. */
  Note
};

/**
 * \brief Machine-readable category of a diagnostic.
 */
enum class DiagnosticCode {
  /** \brief Lexical or syntax error in the input file. */
  Syntax,
  /** \brief An `#include` cannot be resolved or forms a cycle. */
  Include,
  /** \brief A dictionary name is defined more than once. */
  DuplicateDictionary,
  /** \brief A keyword is not declared by the grammar. */
  UnknownKeyword,
  /** \brief A compulsory keyword is missing. */
  MissingCompulsory,
  /** \brief None, or more than one, of a group of alternatives is used. */
  AlternativeGroup,
  /** \brief A keyword requires another keyword, which is missing. */
  MissingRequired,
  /** \brief Mutually exclusive keywords are used together. */
  Conflict,
  /** \brief The user requested the list of keywords with `@Help`. */
  HelpRequested,
  /** \brief A value has the wrong number of tokens or cannot be converted. */
  InvalidValue,
  /** \brief Units are unknown, malformed or of the wrong dimension. */
  InvalidUnit,
  /** \brief A keyword was read before a grammar was attached. */
  NoGrammar,
  /** \brief The calling code reads a keyword the grammar does not declare. */
  UndeclaredKeyword,
  /** \brief The calling code reads a keyword with the wrong type. */
  WrongType,
  /** \brief A keyword read by the calling code is absent. */
  MissingKeyword,
  /** \brief The grammar itself is incoherent. */
  GrammarIncoherent,
  /** \brief A counting rule (`exactly`, `at least`, `at most`) is violated. */
  Cardinality,
  /** \brief A keyword is used where a rule forbids it. */
  Forbidden,
  /** \brief A value is not among the allowed values. */
  NotAllowed,
  /** \brief A value is outside its range, or a list has the wrong size or
     sum. */
  OutOfRange,
  /** \brief A deprecated keyword is used. */
  Deprecated,
  /** \brief A keyword is written in the file but never read. */
  UnusedKeyword,
  /** \brief A referenced dictionary is not defined. */
  UnknownDictionary,
  /** \brief A dictionary is used with two different typed grammars. */
  GrammarMismatch,
  /** \brief References between dictionaries form a cycle. */
  ReferenceCycle,
  /** \brief A dictionary is not reached from the validated root. */
  UnreferencedDictionary,
  /** \brief Any other problem. */
  Other
};

/**
 * \brief Returns a short identifier of a code, e.g. `unknown-keyword`.
 */
[[nodiscard]] auto to_string(DiagnosticCode code) noexcept -> std::string_view;

/**
 * \brief Returns `error`, `warning` or `note`.
 */
[[nodiscard]] auto to_string(Severity severity) noexcept -> std::string_view;

/**
 * \brief One problem found while parsing, validating, or reading input.
 */
struct Diagnostic {
  /** \brief Severity. */
  Severity severity = Severity::Error;
  /** \brief Category. */
  DiagnosticCode code = DiagnosticCode::Other;
  /** \brief Where the problem is (may be empty). */
  SourceLocation location;
  /** \brief Description of the problem. */
  std::string message;
  /** \brief Optional suggestion, e.g. `did you mean @Temperature?`. */
  std::string hint;
  /** \brief Related locations, e.g. the other keyword of a conflict. */
  std::vector<Diagnostic> notes;
};

/**
 * \brief Renders a diagnostic in compiler style.
 *
 * \code
 * input.dic:23:19: error: units 'K' (temperature [K]) are not compatible ...
 *    23 |     @Velocity  19.5 K;
 *       |                     ^
 *       = hint: ...
 * \endcode
 *
 * \param[in] diagnostic Diagnostic to render (with its notes).
 * \param[in] with_excerpt Whether to print the source line and the marker
 * when the source text is available.
 * \return Multi-line text without a trailing newline.
 */
[[nodiscard]] auto format_diagnostic(const Diagnostic &diagnostic,
                                     bool with_excerpt = true) -> std::string;

/**
 * \brief Exception carrying one or more diagnostics.
 *
 * `what()` returns the optional context line followed by every diagnostic
 * rendered with `format_diagnostic`. All library errors about input files,
 * grammars and reads are of this type (or derived from it).
 */
class DictionaryError : public std::runtime_error {
public:
  /**
   * \brief Builds the exception.
   *
   * \param[in] diagnostics Problems found (at least one).
   * \param[in] context Optional first line, e.g. the dictionary concerned.
   */
  explicit DictionaryError(std::vector<Diagnostic> diagnostics,
                           std::string context = {});

  /** \brief Returns the diagnostics. */
  [[nodiscard]] const std::vector<Diagnostic> &diagnostics() const noexcept {
    return diagnostics_;
  }

  /** \brief Returns the context line (possibly empty). */
  [[nodiscard]] const std::string &context() const noexcept {
    return context_;
  }

  /** \brief Returns the number of diagnostics with `Severity::Error`. */
  [[nodiscard]] std::size_t error_count() const noexcept;

  /** \brief Tests whether any diagnostic has the given code. */
  [[nodiscard]] bool has(DiagnosticCode code) const noexcept;

private:
  std::vector<Diagnostic> diagnostics_;
  std::string context_;
};

/** \brief Options of `InstallErrorHandler`. */
struct ErrorHandlerOptions {
  /** \brief Stream for the report (`nullptr`: `std::cerr`). */
  std::ostream *stream = nullptr;
  /** \brief First line of the report. */
  std::string title = "Fatal error in the input dictionaries";
  /** \brief Whether to print "Press enter to exit..." and wait for it. */
  bool wait_for_enter = false;
  /** \brief Exit code of the program. */
  int exit_code = 1;
};

/**
 * \brief Reports a `DictionaryError` that no code catches, and exits.
 *
 * Installs a `std::terminate` handler: an uncaught `DictionaryError` is
 * printed (the title, then every diagnostic) and the program exits with
 * `options.exit_code`, instead of aborting with a platform-dependent message
 * (or none). Other uncaught exceptions go to the previous handler. Calling it
 * again only changes the options.
 *
 * \param[in] options Where and how to report, and the exit code.
 */
void InstallErrorHandler(ErrorHandlerOptions options = {});
} // namespace OpenSMOKEpp

#endif // OpenSMOKEpp_DictionaryDiagnostics_H
