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

#include "DictionaryFile.h"
#include "DictionaryLexer.h"

#include <algorithm>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace OpenSMOKEpp {
void DictionaryFile::ErrorMessage(const std::string message) const {
  std::stringstream stream;
  stream << "Dictionary " << name_ << " starting at line " << index_first_line_
         << " of file " << file_name_ << ": " << message;
  throw std::runtime_error(stream.str());
}

void DictionaryFile::Write(std::ostream &fout) const {
  fout << "Dictionary: " << name_ << std::endl;
  for (unsigned int i = 0; i < good_lines_.size(); i++)
    fout << indices_of_good_lines_[i] << "*" << good_lines_[i] << "*"
         << std::endl;
  fout << "Dictionary(ext): " << name_ << std::endl;
  for (unsigned int i = 0; i < extended_lines_.size(); i++)
    fout << "*" << extended_lines_[i] << "*" << std::endl;
}

void DictionaryFile::Analyze() {
  if (lines_.empty())
    ErrorMessage("The dictionary block is empty.");

  // Recognize the name of the dictionary
  {
    lines_[0] = Lexer::trim_copy(lines_[0]);
    const auto tokens = Lexer::split_whitespace(lines_[0]);
    const std::size_t n = tokens.size();

    if (n != 2)
      ErrorMessage("The name of the dictionary is not specified correctly.");

    if (tokens[0] != "Dictionary")
      ErrorMessage("The name of the dictionary is not specified correctly.");
    name_ = std::string(tokens[1]);
  }

  // Control the syntax
  std::vector<unsigned int> i_forward;
  std::vector<unsigned int> i_backward;
  i_forward.reserve(lines_.size());
  i_backward.reserve(lines_.size());
  {
    unsigned int n_forward = 0;
    unsigned int n_backward = 0;
    for (unsigned int i = 0; i < lines_.size(); i++) {
      const auto forward_count = Lexer::count_char(lines_[i], '{');
      for (std::size_t j = 0; j < forward_count; ++j) {
        ++n_forward;
        i_forward.push_back(i);
      }

      const auto backward_count = Lexer::count_char(lines_[i], '}');
      for (std::size_t j = 0; j < backward_count; ++j) {
        ++n_backward;
        i_backward.push_back(i);
      }
    }

    if (n_forward == n_backward && n_forward > 1)
      ErrorMessage("It seems that sub-dictionaries are defined inside the main "
                   "dictionary. This is not possible.");

    if (n_forward != 1 || n_backward != 1)
      ErrorMessage("Wrong number of curly brackets {}");

    if (i_forward[0] >= i_backward[0])
      ErrorMessage("Wrong position of curly brackets {}");
  }

  // Check the curly braces
  {
    lines_[i_forward[0]] = Lexer::trim_copy(lines_[i_forward[0]]);
    const auto tokens = Lexer::split_whitespace(lines_[i_forward[0]]);
    const std::size_t n = tokens.size();

    if (n != 1 || tokens[0] != "{")
      ErrorMessage("The left hand curly bracket { is not specified correctly.");
  }
  // Check the curly braces
  {
    lines_[i_backward[0]] = Lexer::trim_copy(lines_[i_backward[0]]);
    const auto tokens = Lexer::split_whitespace(lines_[i_backward[0]]);
    const std::size_t n = tokens.size();

    if (n != 1 || tokens[0] != "}")
      ErrorMessage(
          "The right hand curly bracket } is not specified correctly.");
  }

  // Select lines
  {
    good_lines_.reserve(i_backward[0] - i_forward[0] - 1);
    indices_of_good_lines_.reserve(i_backward[0] - i_forward[0] - 1);
    for (unsigned int i = i_forward[0] + 1; i < i_backward[0]; i++) {
      if (lines_[i].find_first_not_of(' ') !=
          lines_[i].npos) // has only spaces?
      {
        good_lines_.push_back(lines_[i]);
        indices_of_good_lines_.push_back(index_first_line_ + i);
      }
    }

    if (good_lines_.empty())
      ErrorMessage(
          "No keyword entries are specified inside the dictionary block.");

    good_lines_[0] = Lexer::trim_copy(good_lines_[0]);
    if (good_lines_[0].empty() || good_lines_[0].front() != '@') {
      std::stringstream line_index;
      line_index << indices_of_good_lines_[0];
      std::string message = "The line " + line_index.str() +
                            " is not written correctly. Remember that a "
                            "keyword must be preceeded by the @ character.";
      ErrorMessage(message);
    }
  }

  // Analyze and compact the lines
  {
    unsigned int n_start = 0;
    i_start_.reserve(good_lines_.size() + 1);
    for (unsigned int i = 0; i < good_lines_.size(); i++) {
      for (std::size_t j = 0; j < Lexer::count_char(good_lines_[i], '@'); ++j) {
        n_start++;
        i_start_.push_back(i);
      }
    }
    i_start_.push_back(static_cast<unsigned int>(good_lines_.size()));

    if (n_start == 0)
      ErrorMessage("No keyword entries starting with @ are specified inside "
                   "the dictionary block.");

    extended_lines_.resize(n_start);
    for (unsigned int i = 0; i < n_start; i++) {
      std::size_t compacted_size = 0;
      for (unsigned int j = i_start_[i]; j < i_start_[i + 1]; j++)
        compacted_size += good_lines_[j].size() + 1;

      extended_lines_[i].reserve(compacted_size);
      for (unsigned int j = i_start_[i]; j < i_start_[i + 1]; j++)
        extended_lines_[i] += good_lines_[j] + " ";
    }

    for (unsigned int i = 0; i < n_start; i++) {
      extended_lines_[i] = Lexer::trim_copy(extended_lines_[i]);

      // Check if closure char is present
      if (extended_lines_[i].find_first_of(';') == extended_lines_[i].npos) {
        std::stringstream line1;
        line1 << indices_of_good_lines_[i_start_[i]];
        std::stringstream line2;
        line2 << indices_of_good_lines_[i_start_[i + 1] - 1];

        std::string message;
        if (i_start_[i] == i_start_[i + 1] - 1) {
          message = "The line " + line1.str() +
                    " is not written correctly: no ; character.";
        } else
          message = "The lines " + line1.str() + "-" + line2.str() +
                    " are not written correctly: no ; character.";
        ErrorMessage(message);
      }

      // Check if the line starts with a @ char
      if (extended_lines_[i].find_first_of('@') != 0) {
        std::stringstream line1;
        line1 << indices_of_good_lines_[i_start_[i]];
        std::stringstream line2;
        line2 << indices_of_good_lines_[i_start_[i + 1] - 1];

        std::string message;
        if (i_start_[i] == i_start_[i + 1] - 1)
          message = "The line " + line1.str() +
                    " is not written correctly: A keyword must be preceeded by "
                    "the @ character.";
        else
          message = "The lines " + line1.str() + "-" + line2.str() +
                    " are not written correctly: A keyword must be preceeded "
                    "by the @ character.";
        ErrorMessage(message);
      }

      // Check if only one ; char is present at the end
      if (extended_lines_[i].find_first_of(';') !=
          (extended_lines_[i].size() - 1)) {
        std::stringstream line1;
        line1 << indices_of_good_lines_[i_start_[i]];
        std::stringstream line2;
        line2 << indices_of_good_lines_[i_start_[i + 1] - 1];

        std::string message;
        if (i_start_[i] == i_start_[i + 1] - 1)
          message = "The line " + line1.str() +
                    " is not written correctly: Too many ; characters.";
        else
          message = "The lines " + line1.str() + "-" + line2.str() +
                    " are not written correctly: Too many ; characters.";
        ErrorMessage(message);
      }

      // Check if only one @ char is present as the first character
      if (extended_lines_[i].find_last_of('@') != 0) {
        std::stringstream line1;
        line1 << indices_of_good_lines_[i_start_[i]];
        std::stringstream line2;
        line2 << indices_of_good_lines_[i_start_[i + 1] - 1];

        std::string message;
        if (i_start_[i] == i_start_[i + 1] - 1)
          message = "The line " + line1.str() +
                    " is not written correctly: Too many @ characters.";
        else
          message = "The lines " + line1.str() + "-" + line2.str() +
                    " are not written correctly: Too many @ characters.";
        ErrorMessage(message);
      }
    }
  }
}

void DictionaryFile::Transfer(Dictionary &dictionary) {
  if (i_start_.size() != extended_lines_.size() + 1)
    ErrorMessage("Internal parsing error: inconsistent keyword line mapping.");

  std::vector<std::string> keywords(extended_lines_.size());
  std::vector<std::string> options(extended_lines_.size());
  std::vector<unsigned int> startingline(extended_lines_.size());
  std::vector<unsigned int> endingline(extended_lines_.size());
  for (unsigned int i = 0; i < extended_lines_.size(); i++) {
    const auto tokens = Lexer::split_whitespace(extended_lines_[i]);

    if (tokens.empty())
      ErrorMessage("Internal parsing error: empty compacted keyword entry.");

    if (i_start_[i] >= indices_of_good_lines_.size() || i_start_[i + 1] == 0 ||
        i_start_[i + 1] - 1 >= indices_of_good_lines_.size())
      ErrorMessage(
          "Internal parsing error: keyword source-line index out of range.");

    unsigned int count = 0;
    options[i].reserve(extended_lines_[i].size());
    for (const auto token : tokens) {
      if (count == 0)
        keywords[i] = std::string(token);
      else
        options[i] += std::string(token) + " ";
      count++;
    }

    startingline[i] = indices_of_good_lines_[i_start_[i]];
    endingline[i] = indices_of_good_lines_[i_start_[i + 1] - 1];
  }

  for (unsigned int i = 0; i < extended_lines_.size(); i++) {
    keywords[i] = Lexer::trim_copy(keywords[i]);

    if (options[i].size() == 0)
      keywords[i].erase(keywords[i].size() - 1);
    else {
      options[i] = Lexer::trim_copy(options[i]);
      options[i].erase(options[i].size() - 1);
    }
    if (options[i].size() != 0)
      options[i] = Lexer::trim_copy(options[i]);
  }

  // Check for duplicates
  std::unordered_map<std::string, unsigned int> first_keyword_line;
  first_keyword_line.reserve(keywords.size());
  for (unsigned int i = 0; i < keywords.size(); i++) {
    const auto [it, inserted] =
        first_keyword_line.emplace(keywords[i], startingline[i]);
    if (!inserted) {
      std::stringstream line1;
      line1 << it->second;
      std::stringstream line2;
      line2 << startingline[i];
      std::string message = "The same keyword (" + keywords[i] +
                            ") is specified twice at lines " + line1.str() +
                            " and " + line2.str();
      ErrorMessage(message);
    }
  }

  // Set the dictionary
  dictionary.SetName(name_);
  dictionary.SetFileName(file_name_);
  dictionary.SetDictionary(keywords, options, startingline, endingline);
}

} // namespace OpenSMOKEpp
