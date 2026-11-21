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
#include <iterator>
#include <ostream>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>

namespace OpenSMOKEpp {
namespace {
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

[[nodiscard]] auto equals_ignore_case(const std::string_view lhs,
                                      const std::string_view rhs) noexcept
    -> bool {
  if (lhs.size() != rhs.size())
    return false;
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    const auto lower = [](const char c) {
      return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    };
    if (lower(lhs[i]) != lower(rhs[i]))
      return false;
  }
  return true;
}

[[nodiscard]] auto parse_bool_token(const std::string_view token,
                                    bool &value) noexcept -> bool {
  if (equals_ignore_case(token, "true") || equals_ignore_case(token, "on")) {
    value = true;
    return true;
  }
  if (equals_ignore_case(token, "false") || equals_ignore_case(token, "off")) {
    value = false;
    return true;
  }
  return false;
}

[[nodiscard]] auto quote(const std::string_view token) -> std::string {
  return "'" + std::string(token) + "'";
}
} // namespace

void Dictionary::ErrorMessage(std::string_view message) const {
  throw std::runtime_error("Dictionary " + name_ + " defined in file " +
                           file_name_ + ": " + std::string(message));
}

void Dictionary::EntryError(const std::size_t index,
                            std::string_view message) const {
  ErrorMessage("line " + std::to_string(starting_lines_[index]) +
               ", keyword " + keywords_[index] + ": " + std::string(message));
}

void Dictionary::SetDictionary(std::vector<std::string> keywords,
                               std::vector<std::string> options,
                               std::vector<unsigned int> starting_lines,
                               std::vector<unsigned int> ending_lines) {
  std::vector<std::vector<std::string>> values;
  values.reserve(options.size());
  for (const auto &option : options)
    values.push_back(Lexer::split_whitespace_copy(option));
  SetDictionary(std::move(keywords), std::move(options), std::move(values),
                std::move(starting_lines), std::move(ending_lines));
}

void Dictionary::SetDictionary(std::vector<std::string> keywords,
                               std::vector<std::string> options,
                               std::vector<std::vector<std::string>> values,
                               std::vector<unsigned int> starting_lines,
                               std::vector<unsigned int> ending_lines) {
  const std::size_t n = keywords.size();
  if (options.size() != n || values.size() != n ||
      starting_lines.size() != n || ending_lines.size() != n)
    throw std::invalid_argument(
        "Dictionary::SetDictionary: all vectors must have the same size");
  keywords_ = std::move(keywords);
  options_ = std::move(options);
  values_ = std::move(values);
  starting_lines_ = std::move(starting_lines);
  ending_lines_ = std::move(ending_lines);
}

void Dictionary::Summary(std::ostream &fout) const {
  fout << "Dictionary " << name_ << " defined in file " << file_name_
       << std::endl;
  for (std::size_t i = 0; i < keywords_.size(); i++)
    fout << keywords_[i] << "*" << options_[i] << "*" << starting_lines_[i]
         << "-" << ending_lines_[i] << std::endl;
}

void Dictionary::Checks() {
  if (grammar_.HasHelpKeyWord(keywords_))
    ErrorMessage("Simulation stopped because the user requested @Help.\n" +
                 grammar_.AvailableKeyWordsTable(name_));

  std::vector<std::string> problems;
  const auto append = [&problems](std::vector<std::string> more) {
    std::move(more.begin(), more.end(), std::back_inserter(problems));
  };

  const auto undefined = grammar_.CollectUndefinedKeyWords(keywords_);
  const bool has_undefined = !undefined.empty();
  append(undefined);
  append(grammar_.CollectCompulsoryErrors(keywords_));
  append(grammar_.CollectRequiredErrors(keywords_));
  append(grammar_.CollectConflictErrors(keywords_));

  if (problems.empty())
    return;

  std::string message = std::to_string(problems.size()) +
                        (problems.size() == 1 ? " error" : " errors") +
                        " found:";
  for (const auto &problem : problems)
    message += "\n  - " + problem;
  if (has_undefined)
    message += "\n" + grammar_.AvailableKeyWordsTable(name_);
  ErrorMessage(message);
}

void Dictionary::SetGrammar(DictionaryGrammar &grammar) {
  has_grammar_ = false;
  const DictionaryGrammar original_state = grammar;
  try {
    grammar.name_ = name_;
    grammar.UserDefined();
    grammar_ = grammar;
    static_cast<DictionaryGrammar &>(grammar) = original_state;
  } catch (...) {
    static_cast<DictionaryGrammar &>(grammar) = original_state;
    throw;
  }
  Checks();
  has_grammar_ = true;
}

void Dictionary::SetGrammar(std::string_view file_name) {
  has_grammar_ = false;
  grammar_.name_ = name_;
  grammar_.ReadFromFile(file_name);
  Checks();
  has_grammar_ = true;
}

bool Dictionary::CheckOption(std::string_view name_of_keyword) const {
  return std::find(keywords_.begin(), keywords_.end(), name_of_keyword) !=
         keywords_.end();
}

auto Dictionary::Lookup(std::string_view name_of_keyword,
                        const DictionaryKeyWordTypes expected_type,
                        std::size_t &index) const
    -> std::optional<ParseError> {
  const std::string keyword(name_of_keyword);
  if (!has_grammar_)
    return ParseError{ParseErrorCode::NoGrammar,
                      "cannot read keyword " + keyword +
                          ": no grammar is attached to this dictionary (call "
                          "SetGrammar() before reading)",
                      0};

  const DictionaryKeyWord *declared = grammar_.FindKeyWord(name_of_keyword);
  if (declared == nullptr)
    return ParseError{ParseErrorCode::UndeclaredKeyword,
                      "the keyword " + keyword +
                          " is not declared in the grammar of this "
                          "dictionary (check the keyword name in the calling "
                          "code)",
                      0};

  if (declared->type() != expected_type)
    return ParseError{
        ParseErrorCode::WrongType,
        "the keyword " + keyword + " is declared as '" +
            std::string(keyword_type_name(declared->type())) +
            "' in the grammar (read it with " +
            std::string(keyword_type_reader(declared->type())) +
            "), but it was read as '" +
            std::string(keyword_type_name(expected_type)) + "'",
        0};

  const auto it =
      std::find(keywords_.begin(), keywords_.end(), name_of_keyword);
  if (it == keywords_.end())
    return ParseError{ParseErrorCode::MissingKeyword,
                      "the required keyword " + keyword +
                          " is not present in the dictionary",
                      0};

  index = static_cast<std::size_t>(std::distance(keywords_.begin(), it));
  return std::nullopt;
}

std::size_t
Dictionary::CheckOption(std::string_view name_of_keyword,
                        const DictionaryKeyWordTypes expected_type) const {
  std::size_t index = 0;
  if (const auto error = Lookup(name_of_keyword, expected_type, index))
    ErrorMessage(error->message);
  return index;
}

auto Dictionary::TryReadDouble(std::string_view name_of_keyword)
    -> ParseResult<double> {
  std::size_t index = 0;
  if (auto error = Lookup(name_of_keyword, SINGLE_DOUBLE, index))
    return ParseResult<double>::Failure(std::move(*error));

  const auto &tokens = values_[index];
  if (tokens.size() != 1)
    return ParseResult<double>::Failure(ParseError{
        ParseErrorCode::BadTokenCount,
        "the keyword " + std::string(name_of_keyword) +
            " requires exactly one number (double), but " +
            std::to_string(tokens.size()) + " values were given",
        starting_lines_[index]});

  double value = 0.0;
  if (!parse_number(tokens[0], value))
    return ParseResult<double>::Failure(
        ParseError{ParseErrorCode::BadNumber,
                   "the keyword " + std::string(name_of_keyword) +
                       " requires a finite number (double), but " +
                       quote(tokens[0]) + " was given",
                   starting_lines_[index]});

  return ParseResult<double>::Success(value);
}

void Dictionary::ReadDouble(std::string_view name_of_keyword, double &value) {
  const auto result = TryReadDouble(name_of_keyword);
  if (!result) {
    const ParseError &error = result.error();
    if (error.line != 0)
      ErrorMessage("line " + std::to_string(error.line) + ": " +
                   error.message);
    ErrorMessage(error.message);
  }
  value = result.value();
}

void Dictionary::ReadMeasure(std::string_view name_of_keyword, double &value,
                             std::string &units) {
  const std::size_t index = CheckOption(name_of_keyword, SINGLE_MEASURE);
  const auto &tokens = values_[index];
  if (tokens.size() != 2)
    EntryError(index, "a measure requires exactly two values (number and "
                      "units, e.g. '300 K'), but " +
                          std::to_string(tokens.size()) + " were given");
  if (!parse_number(tokens[0], value))
    EntryError(index, "the magnitude of a measure must be a finite number, "
                      "but " +
                          quote(tokens[0]) + " was given");
  units = tokens[1];
}

void Dictionary::ReadInt(std::string_view name_of_keyword, int &value) {
  const std::size_t index = CheckOption(name_of_keyword, SINGLE_INT);
  const auto &tokens = values_[index];
  if (tokens.size() != 1)
    EntryError(index, "exactly one integer is required, but " +
                          std::to_string(tokens.size()) + " values were given");
  if (!parse_number(tokens[0], value))
    EntryError(index,
               "an integer is required, but " + quote(tokens[0]) + " was given");
}

void Dictionary::ReadPath(std::string_view name_of_keyword,
                          std::filesystem::path &value) {
  const std::size_t index = CheckOption(name_of_keyword, SINGLE_PATH);
  const auto &tokens = values_[index];
  if (tokens.empty())
    EntryError(index, "a path is required, but no value was given");
  if (tokens.size() == 1) {
    value = std::filesystem::path(tokens[0]);
    return;
  }
  std::string joined;
  for (const auto &token : tokens) {
    if (!joined.empty())
      joined += ' ';
    joined += token;
  }
  value = std::filesystem::path(joined);
}

void Dictionary::ReadDictionary(std::string_view name_of_keyword,
                                std::string &value) {
  const std::size_t index = CheckOption(name_of_keyword, SINGLE_DICTIONARY);
  const auto &tokens = values_[index];
  if (tokens.size() != 1)
    EntryError(index, "exactly one dictionary name is required, but " +
                          std::to_string(tokens.size()) + " values were given");
  value = tokens[0];
}

void Dictionary::ReadSequence(std::string_view name_of_keyword,
                              std::string &value) {
  const std::size_t index = CheckOption(name_of_keyword, SEQUENCE_STRING);
  value = options_[index];
}

void Dictionary::ReadBool(std::string_view name_of_keyword, bool &value) {
  const std::size_t index = CheckOption(name_of_keyword, SINGLE_BOOL);
  const auto &tokens = values_[index];
  if (tokens.size() != 1)
    EntryError(index, "exactly one boolean (true/false/on/off) is required, "
                      "but " +
                          std::to_string(tokens.size()) + " values were given");
  if (!parse_bool_token(tokens[0], value))
    EntryError(index, "a boolean (true/false/on/off) is required, but " +
                          quote(tokens[0]) + " was given");
}

void Dictionary::ReadChar(std::string_view name_of_keyword, char &value) {
  const std::size_t index = CheckOption(name_of_keyword, SINGLE_CHAR);
  const auto &tokens = values_[index];
  if (tokens.size() != 1)
    EntryError(index, "exactly one character is required, but " +
                          std::to_string(tokens.size()) + " values were given");
  if (!parse_char_token(tokens[0], value))
    EntryError(index, "a single character is required, but " +
                          quote(tokens[0]) + " was given");
}

void Dictionary::ReadString(std::string_view name_of_keyword,
                            std::string &value) {
  const std::size_t index = CheckOption(name_of_keyword, SINGLE_STRING);
  const auto &tokens = values_[index];
  if (tokens.size() != 1)
    EntryError(index, "exactly one string is required (use quotes for text "
                      "containing blanks), but " +
                          std::to_string(tokens.size()) + " values were given");
  value = tokens[0];
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<std::string> &values) {
  const std::size_t index = CheckOption(name_of_keyword, VECTOR_STRING);
  const auto &tokens = values_[index];
  if (tokens.empty())
    EntryError(index, "a list of strings is required, but no value was given");
  values = tokens;
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<double> &values) {
  const std::size_t index = CheckOption(name_of_keyword, VECTOR_DOUBLE);
  const auto &tokens = values_[index];
  if (tokens.empty())
    EntryError(index,
               "a list of numbers (double) is required, but no value was given");
  values.resize(tokens.size());
  for (std::size_t i = 0; i < tokens.size(); ++i)
    if (!parse_number(tokens[i], values[i]))
      EntryError(index, "value " + std::to_string(i + 1) +
                            " must be a finite number (double), but " +
                            quote(tokens[i]) + " was given");
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<int> &values) {
  const std::size_t index = CheckOption(name_of_keyword, VECTOR_INT);
  const auto &tokens = values_[index];
  if (tokens.empty())
    EntryError(index, "a list of integers is required, but no value was given");
  values.resize(tokens.size());
  for (std::size_t i = 0; i < tokens.size(); ++i)
    if (!parse_number(tokens[i], values[i]))
      EntryError(index, "value " + std::to_string(i + 1) +
                            " must be an integer, but " + quote(tokens[i]) +
                            " was given");
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<char> &values) {
  const std::size_t index = CheckOption(name_of_keyword, VECTOR_CHAR);
  const auto &tokens = values_[index];
  if (tokens.empty())
    EntryError(index,
               "a list of characters is required, but no value was given");
  values.resize(tokens.size());
  for (std::size_t i = 0; i < tokens.size(); ++i)
    if (!parse_char_token(tokens[i], values[i]))
      EntryError(index, "value " + std::to_string(i + 1) +
                            " must be a single character, but " +
                            quote(tokens[i]) + " was given");
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            BoolVector &values) {
  const std::size_t index = CheckOption(name_of_keyword, VECTOR_BOOL);
  const auto &tokens = values_[index];
  if (tokens.empty())
    EntryError(index, "a list of booleans is required, but no value was given");
  values.clear();
  values.reserve(tokens.size());
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    bool flag = false;
    if (!parse_bool_token(tokens[i], flag))
      EntryError(index, "value " + std::to_string(i + 1) +
                            " must be a boolean (true/false/on/off), but " +
                            quote(tokens[i]) + " was given");
    values.push_back(flag ? 1U : 0U);
  }
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<bool> &values) {
  BoolVector bytes;
  ReadOption(name_of_keyword, bytes);
  values.clear();
  values.reserve(bytes.size());
  std::transform(bytes.begin(), bytes.end(), std::back_inserter(values),
                 [](const std::uint8_t value) { return value != 0U; });
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<std::string> &names,
                            std::vector<double> &values) {
  const std::size_t index = CheckOption(name_of_keyword, VECTOR_STRING_DOUBLE);
  const auto &tokens = values_[index];
  const std::size_t n = tokens.size();
  if (n == 0 || n % 2 != 0)
    EntryError(index, "pairs of a name and a number (double) are required, "
                      "e.g. 'CH4 0.5 O2 0.5', but " +
                          std::to_string(n) + " values were given");
  names.resize(n / 2);
  values.resize(n / 2);
  for (std::size_t i = 0; i < n; i += 2) {
    names[i / 2] = tokens[i];
    if (!parse_number(tokens[i + 1], values[i / 2]))
      EntryError(index, "the value paired with " + quote(tokens[i]) +
                            " must be a finite number (double), but " +
                            quote(tokens[i + 1]) + " was given");
  }
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<int> &values,
                            std::vector<std::string> &names) {
  const std::size_t index = CheckOption(name_of_keyword, VECTOR_INT_STRING);
  const auto &tokens = values_[index];
  const std::size_t n = tokens.size();
  if (n == 0 || n % 2 != 0)
    EntryError(index, "pairs of an integer and a name are required, but " +
                          std::to_string(n) + " values were given");
  names.resize(n / 2);
  values.resize(n / 2);
  for (std::size_t i = 0; i < n; i += 2) {
    if (!parse_number(tokens[i], values[i / 2]))
      EntryError(index, "pair " + std::to_string(i / 2 + 1) +
                            " must start with an integer, but " +
                            quote(tokens[i]) + " was given");
    names[i / 2] = tokens[i + 1];
  }
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<double> &values,
                            std::vector<std::string> &names) {
  const std::size_t index = CheckOption(name_of_keyword, VECTOR_MEASURE);
  const auto &tokens = values_[index];
  const std::size_t n = tokens.size();
  if (n == 0 || n % 2 != 0)
    EntryError(index, "pairs of a number and units are required, e.g. "
                      "'1 m 2 cm', but " +
                          std::to_string(n) + " values were given");
  names.resize(n / 2);
  values.resize(n / 2);
  for (std::size_t i = 0; i < n; i += 2) {
    if (!parse_number(tokens[i], values[i / 2]))
      EntryError(index, "measure " + std::to_string(i / 2 + 1) +
                            " must start with a finite number, but " +
                            quote(tokens[i]) + " was given");
    names[i / 2] = tokens[i + 1];
  }
}

} // namespace OpenSMOKEpp
