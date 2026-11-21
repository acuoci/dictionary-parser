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

#ifndef OpenSMOKEpp_DictionaryManager_H
#define OpenSMOKEpp_DictionaryManager_H

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "Dictionary.h"

namespace OpenSMOKEpp {
/**
 * \brief Owns all dictionaries parsed from one or more input files.
 *
 * The manager parses `Dictionary <name> { ... }` blocks, inline blocks
 * (`@Keyword { ... }`, stored as `parent/Keyword`) and `#include "file"`
 * directives, and stores each dictionary in a `std::map` keyed by name. It
 * performs syntax-level parsing only; semantic grammar validation is
 * performed on a retrieved `Dictionary`.
 */
class DictionaryManager {
public:
  /**
   * \brief Reads all dictionary blocks from a file.
   *
   * `#include "path"` directives are resolved relative to the directory of
   * the file containing them. Each file is read at most once per call
   * (repeated includes are skipped); an include cycle is an error.
   *
   * \param[in] file_name Path to an input file using dictionary syntax.
   * \warning Throws `Lexer::SyntaxError` for syntax errors (message
   * `file:line:column: error: ...`) and `std::runtime_error` if a file
   * cannot be read, no dictionary is found, a dictionary name is defined
   * twice, or includes form a cycle.
   */
  void ReadDictionariesFromFile(std::string_view file_name);

  /**
   * \brief Reads all dictionary blocks from an in-memory text.
   *
   * \param[in] text Source text using dictionary syntax.
   * \param[in] virtual_file_name Name used in diagnostics.
   * \param[in] include_directory Base directory for relative `#include`
   * paths; defaults to the current working directory.
   * \warning Same error behavior as `ReadDictionariesFromFile`.
   */
  void ReadDictionariesFromString(
      std::string_view text, std::string_view virtual_file_name = "<string>",
      const std::filesystem::path &include_directory = {});

  /**
   * \brief Returns a parsed dictionary by name.
   *
   * \param[in] name Dictionary name exactly as declared after the
   * `Dictionary` token, or `parent/Keyword` for an inline block.
   * \return Mutable reference to the stored dictionary.
   * \warning Throws `DictionaryError` if no dictionary with `name` was
   * loaded (with a suggestion when a similar name exists).
   */
  Dictionary &operator()(std::string_view name);

  /**
   * \brief Tests whether a dictionary with the given name was loaded.
   *
   * \param[in] name Dictionary name.
   * \return `true` if the dictionary exists.
   */
  [[nodiscard]] bool contains(std::string_view name) const;

  /**
   * \brief Selects the target unit system of all dictionaries.
   *
   * Applies to the dictionaries already loaded and to those loaded later.
   *
   * \param[in] system `units::si_kmol` (kilomole, the OpenSMOKE++
   * convention and the default) or `units::si` (strict SI, mole).
   */
  void SetUnitSystem(const units::UnitSystem &system);

  /**
   * \brief Treats warnings as errors in all dictionaries (see
   * `Dictionary::SetWarningsAsErrors`), including those loaded later.
   *
   * \param[in] value `true` to promote warnings to errors.
   */
  void SetWarningsAsErrors(bool value);

  /**
   * \brief Reports, for every dictionary with a grammar, the keywords written
   * in the file but never read (see `Dictionary::CheckUnusedKeywords`).
   *
   * \return All warnings, dictionary by dictionary.
   * \warning Throws `DictionaryError` if warnings are treated as errors and
   * any keyword is unused.
   */
  std::vector<Diagnostic> CheckUnusedKeywords() const;

  /**
   * \brief Returns the names of all loaded dictionaries, sorted.
   *
   * \return Dictionary names.
   */
  [[nodiscard]] std::vector<std::string> names() const;

private:
  /** \brief Dictionaries indexed by their declared names. */
  std::map<std::string, Dictionary, std::less<>> map_of_dictionaries_;

  /** \brief Target unit system applied to every dictionary. */
  units::UnitSystem unit_system_{};

  /** \brief Warning policy applied to every dictionary. */
  bool warnings_as_errors_ = false;

  /** \brief Where each dictionary was defined. */
  std::map<std::string, SourceLocation, std::less<>> origins_;

  /**
   * \brief Parses one source buffer and, recursively, its includes.
   *
   * \param[in] text Source text.
   * \param[in] display_name File name used in diagnostics.
   * \param[in] directory Base directory for relative includes.
   * \param[in,out] stack Canonical paths of files being processed.
   * \param[in,out] visited Canonical paths of files already processed.
   */
  void Load(std::string_view text, const std::string &display_name,
            const std::filesystem::path &directory,
            std::vector<std::filesystem::path> &stack,
            std::set<std::filesystem::path> &visited);

  /**
   * \brief Raises a manager-scoped parsing or lookup error.
   *
   * \param[in] message Diagnostic message without class prefix.
   * \param[in] location Where the problem is (optional).
   * \param[in] hint Optional suggestion.
   * \warning Always throws `DictionaryError`.
   */
  [[noreturn]] void ErrorMessage(std::string_view message,
                                 SourceLocation location = {},
                                 std::string hint = {}) const;
};
} // namespace OpenSMOKEpp

#endif // OpenSMOKEpp_DictionaryManager_H
