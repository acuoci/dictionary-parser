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

#include "DictionaryKeyWord.h"
#include "DictionaryLexer.h"

#include <algorithm>
#include <ostream>
#include <stdexcept>

namespace OpenSMOKEpp {
namespace {
struct TypeInfo {
  DictionaryKeyWordTypes type;
  std::string_view name;
  std::string_view reader;
};

constexpr TypeInfo type_table[] = {
    {NONE, "none", "(no reader)"},
    {SINGLE_INT, "single_int", "ReadInt"},
    {SINGLE_DOUBLE, "single_double", "ReadDouble"},
    {SINGLE_STRING, "single_string", "ReadString"},
    {SINGLE_CHAR, "single_char", "ReadChar"},
    {SINGLE_BOOL, "single_bool", "ReadBool"},
    {SINGLE_MEASURE, "single_measure", "ReadMeasure"},
    {SINGLE_PATH, "single_path", "ReadPath"},
    {SINGLE_DICTIONARY, "single_dictionary", "ReadDictionary"},
    {VECTOR_INT, "vector_int", "ReadOption(std::vector<int>&)"},
    {VECTOR_DOUBLE, "vector_double", "ReadOption(std::vector<double>&)"},
    {VECTOR_STRING, "vector_string", "ReadOption(std::vector<std::string>&)"},
    {VECTOR_CHAR, "vector_char", "ReadOption(std::vector<char>&)"},
    {VECTOR_BOOL, "vector_bool", "ReadOption(BoolVector&)"},
    {VECTOR_MEASURE, "vector_measure",
     "ReadOption(std::vector<double>&, std::vector<std::string>&)"},
    {VECTOR_STRING_DOUBLE, "vector_string_double",
     "ReadOption(std::vector<std::string>&, std::vector<double>&)"},
    {VECTOR_INT_STRING, "vector_int_string",
     "ReadOption(std::vector<int>&, std::vector<std::string>&)"},
    {SEQUENCE_STRING, "sequence_string", "ReadSequence"},
};
} // namespace

auto keyword_type_name(const DictionaryKeyWordTypes type) noexcept
    -> std::string_view {
  for (const auto &info : type_table)
    if (info.type == type)
      return info.name;
  return "unknown";
}

auto keyword_type_reader(const DictionaryKeyWordTypes type) noexcept
    -> std::string_view {
  for (const auto &info : type_table)
    if (info.type == type)
      return info.reader;
  return "unknown";
}

void DictionaryKeyWord::ErrorMessage(const std::string message) const {
  throw std::runtime_error("Keyword " + name_ + ": " + message);
}

DictionaryKeyWord::DictionaryKeyWord(const std::string &name,
                                     const DictionaryKeyWordTypes type,
                                     const std::string &short_comment,
                                     const bool is_compulsory)
    : name_(name), type_(type), comment_short_(short_comment),
      is_compulsory_(is_compulsory) {
  SetTypeASCII();
}

DictionaryKeyWord::DictionaryKeyWord(const std::string &name,
                                     const DictionaryKeyWordTypes type,
                                     const std::string &short_comment,
                                     const bool is_compulsory,
                                     const std::string &compulsory_alternatives,
                                     const std::string &required_keywords,
                                     const std::string &conflicting_keywords)
    : name_(name), type_(type), comment_short_(short_comment),
      is_compulsory_(is_compulsory) {

  std::string line1 = "compulsory_alternatives: " + compulsory_alternatives;
  SetCompulsoryAlternatives(line1);

  std::string line2 = "required_keywords: " + required_keywords;
  SetRequiredKeyWords(line2);

  std::string line3 = "conflicting_keywords: " + conflicting_keywords;
  SetConflictingKeywords(line3);

  SetTypeASCII();
}

void DictionaryKeyWord::SetTypeASCII() {
  const std::string_view name = keyword_type_name(type_);
  if (name == "unknown")
    ErrorMessage("The keyword type is not specified correctly.");
  type_ascii_ = std::string(name);
}

void DictionaryKeyWord::SetKeyword(const std::string &line) {
  const auto tokens = Lexer::split_whitespace_view(line);

  if (tokens.size() != 2 || tokens[0] != "keyword:")
    ErrorMessage("The keyword name is not specified correctly.");
  name_ = std::string(tokens[1]);
}

void DictionaryKeyWord::SetType(const std::string &line) {
  const auto tokens = Lexer::split_whitespace_view(line);

  if (tokens.size() != 2 || tokens[0] != "type:")
    ErrorMessage("The keyword type is not specified correctly.");
  for (const auto &info : type_table)
    if (info.name == tokens[1]) {
      type_ = info.type;
      type_ascii_ = std::string(info.name);
      return;
    }
  ErrorMessage("The keyword type is not specified correctly: unknown type '" +
               std::string(tokens[1]) + "'.");
}

void DictionaryKeyWord::SetComment(const std::string &line) {
  const auto tokens = Lexer::split_whitespace_view(line);

  if (tokens.empty() || tokens[0] != "short_comment:")
    ErrorMessage("The keyword short comment is not specified correctly.");

  comment_short_.clear();
  for (std::size_t i = 1; i < tokens.size(); ++i)
    comment_short_ += std::string(tokens[i]) + " ";
}

void DictionaryKeyWord::SetIsCompulsory(const std::string &line) {
  const auto tokens = Lexer::split_whitespace_view(line);

  if (tokens.size() != 2 || tokens[0] != "is_compulsory:")
    ErrorMessage("The keyword is_compulsory is not specified correctly.");
  if (tokens[1] == "true")
    is_compulsory_ = true;
  else if (tokens[1] == "false")
    is_compulsory_ = false;
  else
    ErrorMessage("The keyword is_compulsory is not specified correctly.");
}

void DictionaryKeyWord::SetCompulsoryAlternatives(const std::string &line) {
  const auto tokens = Lexer::split_whitespace_view(line);

  if (tokens.empty() || tokens[0] != "compulsory_alternatives:")
    ErrorMessage(
        "The keyword compulsory_alternatives are not specified correctly.");

  compulsory_alternatives_.clear();
  if (tokens.size() == 2 && tokens[1] == "none")
    return;

  if (std::find(tokens.begin() + 1, tokens.end(), "none") != tokens.end())
    ErrorMessage(
        "The keyword compulsory_alternatives are not specified correctly.");

  if (is_compulsory_ == false && tokens.size() > 1)
    ErrorMessage(
        "The keyword compulsory_alternatives are not specified correctly.");

  compulsory_alternatives_.reserve(tokens.size() > 0 ? tokens.size() - 1 : 0);
  for (std::size_t i = 1; i < tokens.size(); ++i)
    compulsory_alternatives_.push_back(std::string(tokens[i]));
}

void DictionaryKeyWord::SetRequiredKeyWords(const std::string &line) {
  const auto tokens = Lexer::split_whitespace_view(line);

  if (tokens.empty() || tokens[0] != "required_keywords:")
    ErrorMessage("The keyword required_keywords are not specified correctly.");

  required_keywords_.clear();
  if (tokens.size() == 2 && tokens[1] == "none")
    return;

  if (std::find(tokens.begin() + 1, tokens.end(), "none") != tokens.end())
    ErrorMessage("The keyword required_keywords are not specified correctly.");

  required_keywords_.reserve(tokens.size() > 0 ? tokens.size() - 1 : 0);
  for (std::size_t i = 1; i < tokens.size(); ++i)
    required_keywords_.push_back(std::string(tokens[i]));
}

void DictionaryKeyWord::SetConflictingKeywords(const std::string &line) {
  const auto tokens = Lexer::split_whitespace_view(line);

  if (tokens.empty() || tokens[0] != "conflicting_keywords:")
    ErrorMessage(
        "The keyword conflicting_keywords are not specified correctly.");

  conflicting_keywords_.clear();
  if (tokens.size() == 2 && tokens[1] == "none")
    return;

  if (std::find(tokens.begin() + 1, tokens.end(), "none") != tokens.end())
    ErrorMessage(
        "The keyword conflicting_keywords are not specified correctly.");

  conflicting_keywords_.reserve(tokens.size() > 0 ? tokens.size() - 1 : 0);
  for (std::size_t i = 1; i < tokens.size(); ++i)
    conflicting_keywords_.push_back(std::string(tokens[i]));
}

DictionaryKeyWord::DictionaryKeyWord(std::vector<std::string> &lines) {
  if (lines.size() != 7)
    ErrorMessage("Wrong numer of lines defining the keyword");

  for (auto &line : lines)
    line = Lexer::trim_copy(line);

  SetKeyword(lines[0]);
  SetType(lines[1]);
  SetComment(lines[2]);
  SetIsCompulsory(lines[3]);
  SetCompulsoryAlternatives(lines[4]);
  SetRequiredKeyWords(lines[5]);
  SetConflictingKeywords(lines[6]);
}

void DictionaryKeyWord::SetDimensionFromLine(const std::string &line) {
  const auto tokens = Lexer::split_whitespace_view(line);
  if (tokens.size() < 2 || tokens.size() > 3 || tokens[0] != "dimension:" ||
      (tokens.size() == 3 && tokens[2] != "difference"))
    ErrorMessage("The keyword dimension is not specified correctly: expected "
                 "'dimension: <SI unit> [difference]'.");
  try {
    units::Dimension dimension = units::parse_unit(tokens[1]).dimension;
    if (tokens.size() == 3) {
      if (!dimension.is_temperature())
        ErrorMessage("Only a temperature dimension can be marked as "
                     "'difference'.");
      dimension = dimension.as_difference();
    }
    dimension_ = dimension;
  } catch (const units::UnitError &error) {
    ErrorMessage(std::string("The keyword dimension is not valid: ") +
                 error.what());
  }
}

void DictionaryKeyWord::ShortSummary(std::ostream &fout) const {
  fout << name_ << " (" << type_ascii_;
  if (dimension_)
    fout << ", " << units::describe(*dimension_);
  fout << ")" << std::endl;
  fout << " * " << comment_short_ << std::endl;
}

} // namespace OpenSMOKEpp
