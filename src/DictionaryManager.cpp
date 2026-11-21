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

#include <fstream>
#include <iterator>
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
    throw std::runtime_error("Unable to open dictionary file: " +
                             path.string());
  std::ostringstream buffer;
  buffer << input.rdbuf();
  if (input.bad())
    throw std::runtime_error("I/O error while reading dictionary file: " +
                             path.string());
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

void DictionaryManager::ErrorMessage(std::string_view message) const {
  throw std::runtime_error("DictionaryManager: " + std::string(message));
}

void DictionaryManager::ReadDictionariesFromFile(std::string_view file_name) {
  const std::filesystem::path path{std::string(file_name)};
  const std::string text = read_whole_file(path);

  std::vector<std::filesystem::path> stack{canonical_or_normal(path)};
  std::set<std::filesystem::path> visited{stack.front()};
  const std::size_t before = map_of_dictionaries_.size();
  Load(text, std::string(file_name), path.parent_path(), stack, visited);
  if (map_of_dictionaries_.size() == before)
    ErrorMessage("No dictionaries are specified in file " +
                 std::string(file_name) + ".");
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
    ErrorMessage("No dictionaries are specified in " +
                 std::string(virtual_file_name) + ".");
}

void DictionaryManager::Load(std::string_view text,
                             const std::string &display_name,
                             const std::filesystem::path &directory,
                             std::vector<std::filesystem::path> &stack,
                             std::set<std::filesystem::path> &visited) {
  detail::ParsedSource parsed = detail::parse_source(text, display_name);

  for (auto &block : parsed.dictionaries) {
    const std::string origin =
        display_name + ":" + std::to_string(block.line);
    if (const auto it = origins_.find(block.name); it != origins_.end())
      ErrorMessage("The dictionary '" + block.name +
                   "' is defined more than once: at " + it->second +
                   " and at " + origin + ".");

    std::vector<std::string> keywords;
    std::vector<std::string> options;
    std::vector<std::vector<std::string>> values;
    std::vector<unsigned int> starting_lines;
    std::vector<unsigned int> ending_lines;
    keywords.reserve(block.entries.size());
    options.reserve(block.entries.size());
    values.reserve(block.entries.size());
    starting_lines.reserve(block.entries.size());
    ending_lines.reserve(block.entries.size());
    for (auto &entry : block.entries) {
      keywords.push_back(std::move(entry.keyword));
      options.push_back(std::move(entry.option));
      values.push_back(std::move(entry.values));
      starting_lines.push_back(entry.first_line);
      ending_lines.push_back(entry.last_line);
    }

    Dictionary dictionary;
    dictionary.SetName(block.name);
    dictionary.SetFileName(display_name);
    dictionary.SetUnitSystem(unit_system_);
    dictionary.SetDictionary(std::move(keywords), std::move(options),
                             std::move(values), std::move(starting_lines),
                             std::move(ending_lines));
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
      throw Lexer::SyntaxError(display_name, include.line, include.column,
                               "circular #include: " + chain);
    }
    if (visited.contains(key))
      continue; // each file is read at most once

    std::string included_text;
    try {
      included_text = read_whole_file(resolved);
    } catch (const std::runtime_error &) {
      throw Lexer::SyntaxError(display_name, include.line, include.column,
                               "cannot open included file '" + include.path +
                                   "' (resolved to '" + resolved.string() +
                                   "')");
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
  if (it == map_of_dictionaries_.end())
    ErrorMessage("The " + std::string(name) + " dictionary was not defined.");
  return it->second;
}

void DictionaryManager::SetUnitSystem(const units::UnitSystem &system) {
  unit_system_ = system;
  for (auto &[name, dictionary] : map_of_dictionaries_)
    dictionary.SetUnitSystem(system);
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
