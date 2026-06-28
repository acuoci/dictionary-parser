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
#include "DictionaryFile.h"
#include "DictionaryLexer.h"

#include <stdexcept>
#include <utility>

namespace OpenSMOKEpp {
void DictionaryManager::ErrorMessage(std::string_view message) const {
  throw std::runtime_error("DictionaryManager: " + std::string(message));
}

void DictionaryManager::ReadDictionariesFromFile(std::string_view file_name) {
  const std::string file_name_string(file_name);
  DictionaryInputFile dict_file(file_name_string);

  const int number_of_block_lines =
      static_cast<int>(dict_file.clean_lines().size());

  // Count number of dictionaries
  unsigned int n_dictionaries = 0;
  std::vector<unsigned int> line_dictionaries;
  for (int i = 0; i < number_of_block_lines; i++) {
    for (std::size_t j = 0; j < Lexer::count_substring_case_insensitive(
                                    dict_file.clean_lines()[i], "Dictionary");
         ++j) {
      n_dictionaries++;
      line_dictionaries.push_back(i + 1);
    }
  }
  line_dictionaries.push_back(number_of_block_lines + 1);

  if (n_dictionaries == 0) {
    ErrorMessage("No dictionaries are specified in the current file.");
  }

  for (unsigned int i = 0; i < n_dictionaries; i++) {
    DictionaryFile dictionary_file;

    dictionary_file.SetFileName(file_name_string);
    dictionary_file.SetFirstLine(line_dictionaries[i]);
    for (unsigned int j = line_dictionaries[i] - 1;
         j < line_dictionaries[i + 1] - 1; j++)
      dictionary_file.AddLine(dict_file.clean_lines()[j]);
    dictionary_file.Analyze();

    Dictionary dictionary;
    dictionary_file.Transfer(dictionary);

    const std::string dictionary_name = dictionary.name();
    if (!map_of_dictionaries_.emplace(dictionary_name, std::move(dictionary))
             .second)
      ErrorMessage("The " + dictionary_name +
                   " dictionary is specified more than once.");
  }
}

Dictionary &DictionaryManager::operator()(std::string_view name) {
  const std::string dictionary_name(name);
  std::map<std::string, Dictionary>::iterator dict;
  dict = map_of_dictionaries_.find(dictionary_name);

  if (dict == map_of_dictionaries_.end())
    ErrorMessage("The " + dictionary_name + " dictionary was not defined.");

  return dict->second;
}
} // namespace OpenSMOKEpp
