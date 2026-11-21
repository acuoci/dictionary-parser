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

#include "DictionaryManager.h"
#include "DictionaryLexer.h"
#include "DictionaryParser.h"
#include "DictionaryUtilities.h"

#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace OpenSMOKEpp {
namespace {
[[nodiscard]] auto read_whole_file(const std::filesystem::path &path)
    -> std::string {
  std::ifstream input(path, std::ios::in | std::ios::binary);
  if (!input.is_open())
    throw DictionaryError(
        {Diagnostic{Severity::Error, DiagnosticCode::Other,
                    SourceLocation{nullptr, path.string(), 0, 0, 0},
                    "unable to open the dictionary file", {}, {}}},
        "DictionaryManager");
  std::ostringstream buffer;
  buffer << input.rdbuf();
  if (input.bad())
    throw DictionaryError(
        {Diagnostic{Severity::Error, DiagnosticCode::Other,
                    SourceLocation{nullptr, path.string(), 0, 0, 0},
                    "I/O error while reading the dictionary file", {}, {}}},
        "DictionaryManager");
  return buffer.str();
}

[[nodiscard]] auto canonical_or_normal(const std::filesystem::path &path)
    -> std::filesystem::path {
  std::error_code error;
  auto result = std::filesystem::weakly_canonical(path, error);
  if (error)
    return std::filesystem::absolute(path).lexically_normal();
  return result;
}
} // namespace

void DictionaryManager::ErrorMessage(std::string_view message,
                                     SourceLocation location,
                                     std::string hint) const {
  throw DictionaryError({Diagnostic{Severity::Error, DiagnosticCode::Other,
                                    std::move(location), std::string(message),
                                    std::move(hint), {}}},
                        "DictionaryManager");
}

void DictionaryManager::ReadDictionariesFromFile(std::string_view file_name) {
  const std::filesystem::path path{std::string(file_name)};
  const std::string text = read_whole_file(path);

  std::vector<std::filesystem::path> stack{canonical_or_normal(path)};
  std::set<std::filesystem::path> visited{stack.front()};
  const std::size_t before = map_of_dictionaries_.size();
  Load(text, std::string(file_name), path.parent_path(), stack, visited);
  if (map_of_dictionaries_.size() == before)
    ErrorMessage("no dictionaries are specified in this file",
                 SourceLocation{nullptr, std::string(file_name), 0, 0, 0});
}

void DictionaryManager::ReadDictionariesFromString(
    std::string_view text, std::string_view virtual_file_name,
    const std::filesystem::path &include_directory) {
  std::vector<std::filesystem::path> stack;
  std::set<std::filesystem::path> visited;
  const std::size_t before = map_of_dictionaries_.size();
  Load(text, std::string(virtual_file_name), include_directory, stack,
       visited);
  if (map_of_dictionaries_.size() == before)
    ErrorMessage("no dictionaries are specified in this file",
                 SourceLocation{nullptr, std::string(virtual_file_name), 0, 0,
                                0});
}

void DictionaryManager::Load(std::string_view text,
                             const std::string &display_name,
                             const std::filesystem::path &directory,
                             std::vector<std::filesystem::path> &stack,
                             std::set<std::filesystem::path> &visited) {
  const auto source = std::make_shared<const SourceFile>(display_name,
                                                         std::string(text));
  detail::ParsedSource parsed = detail::parse_source(source);
  const auto at = [&source](const unsigned int line, const unsigned int column,
                            const unsigned int length) {
    return SourceLocation{source, source->name(), line, column, length};
  };

  for (auto &block : parsed.dictionaries) {
    const SourceLocation origin = at(block.line, block.column, block.length);
    if (const auto it = origins_.find(block.name); it != origins_.end()) {
      Diagnostic duplicate{Severity::Error,
                           DiagnosticCode::DuplicateDictionary,
                           origin,
                           "the dictionary '" + block.name +
                               "' is defined more than once",
                           {},
                           {}};
      duplicate.notes.push_back(Diagnostic{Severity::Note,
                                           DiagnosticCode::DuplicateDictionary,
                                           it->second,
                                           "previous definition is here",
                                           {},
                                           {}});
      throw DictionaryError({std::move(duplicate)}, "DictionaryManager");
    }

    std::vector<Dictionary::Entry> entries;
    entries.reserve(block.entries.size());
    for (auto &parsed_entry : block.entries) {
      Dictionary::Entry entry;
      entry.keyword = std::move(parsed_entry.keyword);
      entry.option = std::move(parsed_entry.option);
      entry.values = std::move(parsed_entry.values);
      entry.tokens = std::move(parsed_entry.tokens);
      entry.first_line = parsed_entry.first_line;
      entry.last_line = parsed_entry.last_line;
      entry.column = parsed_entry.column;
      entries.push_back(std::move(entry));
    }

    Dictionary dictionary;
    dictionary.SetName(block.name);
    dictionary.SetFileName(display_name);
    dictionary.SetUnitSystem(unit_system_);
    dictionary.SetWarningsAsErrors(warnings_as_errors_);
    dictionary.SetEntries(std::move(entries));
    dictionary.SetLocation(origin);
    origins_.emplace(block.name, origin);
    map_of_dictionaries_.emplace(block.name, std::move(dictionary));
  }

  for (const auto &include : parsed.includes) {
    const std::filesystem::path relative{include.path};
    const std::filesystem::path resolved =
        relative.is_absolute() ? relative : directory / relative;
    const std::filesystem::path key = canonical_or_normal(resolved);

    for (std::size_t i = 0; i < stack.size(); ++i) {
      if (stack[i] != key)
        continue;
      std::string chain;
      for (std::size_t j = i; j < stack.size(); ++j)
        chain += stack[j].filename().string() + " -> ";
      chain += key.filename().string();
      throw Lexer::SyntaxError(at(include.line, include.column, 8),
                               "circular #include: " + chain,
                               DiagnosticCode::Include);
    }
    if (visited.contains(key))
      continue; // each file is read at most once

    std::string included_text;
    try {
      included_text = read_whole_file(resolved);
    } catch (const DictionaryError &) {
      throw Lexer::SyntaxError(at(include.line, include.column, 8),
                               "cannot open included file '" + include.path +
                                   "' (resolved to '" + resolved.string() +
                                   "')",
                               DiagnosticCode::Include);
    }

    visited.insert(key);
    stack.push_back(key);
    Load(included_text, resolved.lexically_normal().string(),
         resolved.parent_path(), stack, visited);
    stack.pop_back();
  }
}

Dictionary &DictionaryManager::operator()(std::string_view name) {
  const auto it = map_of_dictionaries_.find(name);
  if (it == map_of_dictionaries_.end()) {
    const std::string suggestion = detail::closest_match(name, names());
    ErrorMessage("the dictionary '" + std::string(name) + "' was not defined",
                 {},
                 suggestion.empty() ? std::string()
                                    : "did you mean '" + suggestion + "'?");
  }
  return it->second;
}

void DictionaryManager::SetUnitSystem(const units::UnitSystem &system) {
  unit_system_ = system;
  for (auto &[name, dictionary] : map_of_dictionaries_)
    dictionary.SetUnitSystem(system);
}

void DictionaryManager::SetWarningsAsErrors(const bool value) {
  warnings_as_errors_ = value;
  for (auto &[name, dictionary] : map_of_dictionaries_)
    dictionary.SetWarningsAsErrors(value);
}

std::vector<Diagnostic> DictionaryManager::CheckUnusedKeywords() const {
  std::vector<Diagnostic> warnings;
  for (const auto &[name, dictionary] : map_of_dictionaries_) {
    if (!dictionary.has_grammar())
      continue;
    Dictionary copy = dictionary;
    copy.SetWarningsAsErrors(false);
    auto found = copy.CheckUnusedKeywords();
    std::move(found.begin(), found.end(), std::back_inserter(warnings));
  }
  if (warnings_as_errors_ && !warnings.empty()) {
    for (auto &warning : warnings)
      warning.severity = Severity::Error;
    throw DictionaryError(std::move(warnings), "DictionaryManager");
  }
  return warnings;
}

bool DictionaryManager::contains(std::string_view name) const {
  return map_of_dictionaries_.find(name) != map_of_dictionaries_.end();
}

std::vector<std::string> DictionaryManager::names() const {
  std::vector<std::string> result;
  result.reserve(map_of_dictionaries_.size());
  for (const auto &[name, dictionary] : map_of_dictionaries_)
    result.push_back(name);
  return result;
}
} // namespace OpenSMOKEpp
