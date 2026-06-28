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

#include "Dictionary.h"
#include "DictionaryLexer.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iostream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>

namespace OpenSMOKEpp {
namespace {
[[nodiscard]] auto tokens_from(const std::string &text)
    -> std::vector<std::string_view> {
  return Lexer::split_whitespace(text);
}

template <typename Number>
[[nodiscard]] auto parse_number(const std::string_view token,
                                Number &value) noexcept -> bool {
  const auto *begin = token.data();
  const auto *end = token.data() + token.size();
  const auto [ptr, ec] = std::from_chars(begin, end, value);
  if (ec != std::errc{} || ptr != end)
    return false;

  if constexpr (std::is_floating_point_v<Number>)
    return std::isfinite(value);
  else
    return true;
}

[[nodiscard]] auto parse_char_token(const std::string_view token,
                                    char &value) noexcept -> bool {
  if (token.size() != 1)
    return false;
  value = token.front();
  return true;
}
} // namespace

void Dictionary::ErrorMessage(std::string_view message) const {
  throw std::runtime_error("Dictionary " + name_ + " defined in file " +
                           file_name_ + ": " + std::string(message));
}

void Dictionary::SetDictionary(std::vector<std::string> keywords,
                               std::vector<std::string> options,
                               std::vector<unsigned int> starting_lines,
                               std::vector<unsigned int> ending_lines) {
  keywords_ = std::move(keywords);
  options_ = std::move(options);
  starting_lines_ = std::move(starting_lines);
  ending_lines_ = std::move(ending_lines);
}

void Dictionary::Summary(std::ostream &fout) const {
  fout << "Dictionary " << name_ << " defined in file " << file_name_
       << std::endl;
  for (unsigned int i = 0; i < keywords_.size(); i++)
    fout << keywords_[i] << "*" << options_[i] << "*" << starting_lines_[i]
         << "-" << ending_lines_[i] << std::endl;
}

void Dictionary::Checks() {
  // Check for @Help keyword
  bool help_keyword = grammar_.CheckForHelpKeyWord(keywords_);
  if (help_keyword == true)
    ErrorMessage("Simulation stopped because the user requested @Help");

  // Check for undefined keywords
  bool undefined_keywords = grammar_.CheckUndefinedKeyWords(keywords_);
  if (undefined_keywords == false)
    ErrorMessage("Error in the dictionary (undefined keywords). See the "
                 "messages reported above.");

  // Check for compulsory keywords
  bool compulsory_keywords = grammar_.CheckCompulsoryKeyWords(keywords_);
  if (compulsory_keywords == false)
    ErrorMessage("Error in the dictionary (compulsory keywords). See the "
                 "messages reported above.");

  // Check for required keywords
  bool required_keywords = grammar_.CheckRequiredKeyWords(keywords_);
  if (required_keywords == false)
    ErrorMessage("Error in the dictionary (required keywords). See the "
                 "messages reported above.");

  // Check for conflicting keywords
  bool conflicting_keywords = grammar_.CheckConflictingKeyWords(keywords_);
  if (conflicting_keywords == false)
    ErrorMessage("Error in the dictionary (conflicting keywords). See the "
                 "messages reported above.");
}

void Dictionary::SetGrammar(DictionaryGrammar &grammar) {
  grammar.UserDefined();
  grammar_ = grammar;
  Checks();
}

void Dictionary::SetGrammar(std::string_view file_name) {
  grammar_.ReadFromFile(file_name);
  Checks();
}

bool Dictionary::CheckOption(std::string_view name_of_keyword) {
  const std::vector<std::string>::iterator it =
      find(keywords_.begin(), keywords_.end(), name_of_keyword);

  if (it == keywords_.end())
    return false;
  else
    return true;
}

std::size_t
Dictionary::CheckOption(std::string_view name_of_keyword,
                        const DictionaryKeyWordTypes expected_type) {
  const std::vector<std::string>::iterator it =
      std::find(keywords_.begin(), keywords_.end(), name_of_keyword);

  if (it == keywords_.end()) {
    std::string message = "The required keyword (" +
                          std::string(name_of_keyword) +
                          ") is not present in the dictionary.";
    ErrorMessage(message);
    return 0;
  } else {
    const std::size_t index = static_cast<std::size_t>(it - keywords_.begin());

    if (grammar_.CheckType(name_of_keyword, expected_type) == false) {
      std::stringstream line;
      line << starting_lines_[index];
      std::string message =
          "Error in the keyword at line : " + line.str() + "\n";
      message += "The required keyword (" + std::string(name_of_keyword) +
                 ") is of different type";
      std::cout << "Expected type: " << expected_type << std::endl;
      ErrorMessage(message);
    }

    return index;
  }
}

void Dictionary::ReadDouble(std::string_view name_of_keyword, double &value) {
  size_t index = CheckOption(name_of_keyword, SINGLE_DOUBLE);

  const auto tokens = tokens_from(options_[index]);
  if (tokens.size() != 1) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "The required keyword (" + std::string(name_of_keyword) +
               ") requires a number (double).";
    ErrorMessage(message);
  }

  if (!parse_number(tokens[0], value)) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "Failure in the numerical conversion.";
    ErrorMessage(message);
  }
}

void Dictionary::ReadMeasure(std::string_view name_of_keyword, double &value,
                             std::string &units) {
  size_t index = CheckOption(name_of_keyword, SINGLE_MEASURE);

  const auto tokens = tokens_from(options_[index]);
  if (tokens.size() != 2) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "The required keyword (" + std::string(name_of_keyword) +
               ") requires a measure (double + string).";
    ErrorMessage(message);
  }

  if (!parse_number(tokens[0], value)) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "Failure in the numerical conversion.";
    ErrorMessage(message);
  }
  units = std::string(tokens[1]);
}

void Dictionary::ReadInt(std::string_view name_of_keyword, int &value) {
  size_t index = CheckOption(name_of_keyword, SINGLE_INT);

  const auto tokens = tokens_from(options_[index]);
  if (tokens.size() != 1) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "The required keyword (" + std::string(name_of_keyword) +
               ") requires a number (int).";
    ErrorMessage(message);
  }

  if (!parse_number(tokens[0], value)) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "Failure in the numerical conversion.";
    ErrorMessage(message);
  }
}

void Dictionary::ReadPath(std::string_view name_of_keyword,
                          std::filesystem::path &value) {
  size_t index = CheckOption(name_of_keyword, SINGLE_PATH);

  value = std::filesystem::path(options_[index]);
}

void Dictionary::ReadDictionary(std::string_view name_of_keyword,
                                std::string &value) {
  size_t index = CheckOption(name_of_keyword, SINGLE_DICTIONARY);

  const auto tokens = tokens_from(options_[index]);
  if (tokens.size() != 1) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "The required keyword (" + std::string(name_of_keyword) +
               ") requires a dictionary (string)";
    ErrorMessage(message);
  }

  value = std::string(tokens[0]);
}

void Dictionary::ReadSequence(std::string_view name_of_keyword,
                              std::string &value) {
  size_t index = CheckOption(name_of_keyword, SEQUENCE_STRING);

  value = options_[index];
}

void Dictionary::ReadBool(std::string_view name_of_keyword, bool &value) {
  size_t index = CheckOption(name_of_keyword, SINGLE_BOOL);

  const auto tokens = tokens_from(options_[index]);
  if (tokens.size() != 1) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "The required keyword (" + std::string(name_of_keyword) +
               ") requires a number (int).";
    ErrorMessage(message);
  }

  const std::string_view number = tokens[0];
  if (number == "true" || number == "on")
    value = true;
  else if (number == "false" || number == "off")
    value = false;
  else {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "Only boolean types can be accepted.";
    ErrorMessage(message);
  }
}

void Dictionary::ReadChar(std::string_view name_of_keyword, char &value) {
  size_t index = CheckOption(name_of_keyword, SINGLE_CHAR);

  const auto tokens = tokens_from(options_[index]);
  if (tokens.size() != 1) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "The required keyword (" + std::string(name_of_keyword) +
               ") requires a char";
    ErrorMessage(message);
  }

  if (!parse_char_token(tokens[0], value)) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "Failure in the character conversion.";
    ErrorMessage(message);
  }
}

void Dictionary::ReadString(std::string_view name_of_keyword,
                            std::string &value) {
  size_t index = CheckOption(name_of_keyword, SINGLE_STRING);

  const auto tokens = tokens_from(options_[index]);
  if (tokens.size() != 1) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "The required keyword (" + std::string(name_of_keyword) +
               ") requires a string";
    ErrorMessage(message);
  }

  value = std::string(tokens[0]);
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<std::string> &values) {
  size_t index = CheckOption(name_of_keyword, VECTOR_STRING);

  const auto tokens = tokens_from(options_[index]);
  if (tokens.empty()) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "The required keyword (" + std::string(name_of_keyword) +
               ") requires a list of strings";
    ErrorMessage(message);
  }

  values.clear();
  values.reserve(tokens.size());
  std::transform(
      tokens.begin(), tokens.end(), std::back_inserter(values),
      [](const std::string_view token) { return std::string(token); });
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<double> &values) {
  size_t index = CheckOption(name_of_keyword, VECTOR_DOUBLE);

  const auto tokens = tokens_from(options_[index]);
  if (tokens.empty()) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "The required keyword (" + std::string(name_of_keyword) +
               ") requires a list of numbers (double).";
    ErrorMessage(message);
  }

  values.resize(tokens.size());
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    if (!parse_number(tokens[i], values[i])) {
      std::stringstream line;
      line << starting_lines_[index];
      std::string message =
          "Error in the keyword at line : " + line.str() + "\n";
      message += "Failure in the numerical conversion.";
      ErrorMessage(message);
    }
  }
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<int> &values) {
  size_t index = CheckOption(name_of_keyword, VECTOR_INT);

  const auto tokens = tokens_from(options_[index]);
  if (tokens.empty()) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "The required keyword (" + std::string(name_of_keyword) +
               ") requires a list of numbers (int).";
    ErrorMessage(message);
  }

  values.resize(tokens.size());
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    if (!parse_number(tokens[i], values[i])) {
      std::stringstream line;
      line << starting_lines_[index];
      std::string message =
          "Error in the keyword at line : " + line.str() + "\n";
      message += "Failure in the numerical conversion.";
      ErrorMessage(message);
    }
  }
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<char> &values) {
  size_t index = CheckOption(name_of_keyword, VECTOR_CHAR);

  const auto tokens = tokens_from(options_[index]);
  if (tokens.empty()) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "The required keyword (" + std::string(name_of_keyword) +
               ") requires a list of chars.";
    ErrorMessage(message);
  }

  values.resize(tokens.size());
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    if (!parse_char_token(tokens[i], values[i])) {
      std::stringstream line;
      line << starting_lines_[index];
      std::string message =
          "Error in the keyword at line : " + line.str() + "\n";
      message += "Failure in the character conversion.";
      ErrorMessage(message);
    }
  }
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<bool> &values) {
  size_t index = CheckOption(name_of_keyword, VECTOR_BOOL);

  const auto tokens = tokens_from(options_[index]);
  if (tokens.empty()) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "The required keyword (" + std::string(name_of_keyword) +
               ") requires a list of booleans.";
    ErrorMessage(message);
  }

  values.clear();
  values.reserve(tokens.size());
  for (const auto token : tokens) {
    if (token == "true" || token == "on")
      values.push_back(true);
    else if (token == "false" || token == "off")
      values.push_back(false);
    else {
      std::stringstream line;
      line << starting_lines_[index];
      std::string message =
          "Error in the keyword at line : " + line.str() + "\n";
      message += "Only boolean types can be accepted.";
      ErrorMessage(message);
    }
  }
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<std::string> &names,
                            std::vector<double> &values) {
  size_t index = CheckOption(name_of_keyword, VECTOR_STRING_DOUBLE);

  const auto tokens = tokens_from(options_[index]);
  const std::size_t n = tokens.size();

  if (n == 0 || n % 2 != 0) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "The required keyword (" + std::string(name_of_keyword) +
               ") requires a list of string and numbers (double)";
    ErrorMessage(message);
  }

  names.resize(n / 2);
  values.resize(n / 2);

  for (std::size_t i = 0; i < n; i += 2) {
    const std::size_t count = i / 2;
    names[count] = std::string(tokens[i]);

    if (!parse_number(tokens[i + 1], values[count])) {
      std::stringstream line;
      line << starting_lines_[index];
      std::string message =
          "Error in the keyword at line : " + line.str() + "\n";
      message += "Failure in the numerical conversion.";
      ErrorMessage(message);
    }
  }
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<int> &values,
                            std::vector<std::string> &names) {
  size_t index = CheckOption(name_of_keyword, VECTOR_INT_STRING);

  const auto tokens = tokens_from(options_[index]);
  const std::size_t n = tokens.size();

  if (n == 0 || n % 2 != 0) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "The required keyword (" + std::string(name_of_keyword) +
               ") requires a list of numbers (int) and strings";
    ErrorMessage(message);
  }

  names.resize(n / 2);
  values.resize(n / 2);

  for (std::size_t i = 0; i < n; i += 2) {
    const std::size_t count = i / 2;
    if (!parse_number(tokens[i], values[count])) {
      std::stringstream line;
      line << starting_lines_[index];
      std::string message =
          "Error in the keyword at line : " + line.str() + "\n";
      message += "Failure in the numerical conversion.";
      ErrorMessage(message);
    }
    names[count] = std::string(tokens[i + 1]);
  }
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<double> &values,
                            std::vector<std::string> &names) {
  size_t index = CheckOption(name_of_keyword, VECTOR_MEASURE);

  const auto tokens = tokens_from(options_[index]);
  const std::size_t n = tokens.size();

  if (n == 0 || n % 2 != 0) {
    std::stringstream line;
    line << starting_lines_[index];
    std::string message = "Error in the keyword at line : " + line.str() + "\n";
    message += "The required keyword (" + std::string(name_of_keyword) +
               ") requires a list of numbers (double) and strings";
    ErrorMessage(message);
  }

  names.resize(n / 2);
  values.resize(n / 2);

  for (std::size_t i = 0; i < n; i += 2) {
    const std::size_t count = i / 2;
    if (!parse_number(tokens[i], values[count])) {
      std::stringstream line;
      line << starting_lines_[index];
      std::string message =
          "Error in the keyword at line : " + line.str() + "\n";
      message += "Failure in the numerical conversion.";
      ErrorMessage(message);
    }
    names[count] = std::string(tokens[i + 1]);
  }
}

} // namespace OpenSMOKEpp
