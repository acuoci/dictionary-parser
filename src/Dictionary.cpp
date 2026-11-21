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
#include "DictionaryUnits.h"
#include "DictionaryValues.h"

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
constexpr std::size_t npos_index = static_cast<std::size_t>(-1);

[[nodiscard]] auto parse_char_token(const std::string_view token,
                                    char &value) noexcept -> bool {
  if (token.size() != 1)
    return false;
  value = token.front();
  return true;
}

using detail::is_number;
using detail::parse_bool_token;
using detail::parse_number;
using detail::quote;
using detail::split_measures;
} // namespace

auto Dictionary::Context() const -> std::string {
  return "Dictionary " + name_ + " defined in file " + file_name_;
}

void Dictionary::Fail(std::vector<Diagnostic> diagnostics) const {
  throw DictionaryError(std::move(diagnostics), Context());
}

void Dictionary::ErrorMessage(std::string_view message,
                              const DiagnosticCode code) const {
  Fail({Diagnostic{Severity::Error, code, {}, std::string(message), {}, {}}});
}

auto Dictionary::IndexOf(std::string_view name_of_keyword) const noexcept
    -> std::size_t {
  for (std::size_t i = 0; i < entries_.size(); ++i)
    if (!entries_[i].from_default && entries_[i].keyword == name_of_keyword)
      return i;
  return npos_index;
}

auto Dictionary::KeywordLocation(const std::size_t index) const
    -> SourceLocation {
  SourceLocation where = location_;
  if (where.file_name.empty())
    where.file_name = file_name_;
  const Entry &entry = entries_[index];
  where.line = entry.first_line;
  where.column = entry.column;
  where.length = static_cast<unsigned int>(entry.keyword.size());
  return where;
}

auto Dictionary::TokenLocation(const std::size_t index,
                               const std::size_t token) const
    -> SourceLocation {
  const Entry &entry = entries_[index];
  if (token >= entry.tokens.size() || entry.tokens[token].line == 0)
    return KeywordLocation(index);
  SourceLocation where = KeywordLocation(index);
  where.line = entry.tokens[token].line;
  where.column = entry.tokens[token].column;
  where.length = static_cast<unsigned int>(entry.tokens[token].spelling.size());
  return where;
}

SourceLocation Dictionary::location_of(std::string_view name_of_keyword) const {
  const std::size_t index = IndexOf(name_of_keyword);
  if (index == npos_index) {
    SourceLocation where = location_;
    if (where.file_name.empty())
      where.file_name = file_name_;
    return where;
  }
  return KeywordLocation(index);
}

void Dictionary::EntryError(const std::size_t index, std::string_view message,
                            const std::size_t token,
                            const DiagnosticCode code) const {
  const SourceLocation where =
      token == npos_index ? KeywordLocation(index) : TokenLocation(index, token);
  std::string text = "keyword " + entries_[index].keyword + ": " +
                     std::string(message);
  if (where.file_name.empty() && entries_[index].first_line != 0)
    text = "line " + std::to_string(entries_[index].first_line) + ", " + text;
  Fail({Diagnostic{Severity::Error, code, where, std::move(text), {}, {}}});
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
  std::vector<Entry> entries(n);
  for (std::size_t i = 0; i < n; ++i) {
    entries[i].keyword = std::move(keywords[i]);
    entries[i].option = std::move(options[i]);
    entries[i].values = std::move(values[i]);
    entries[i].first_line = starting_lines[i];
    entries[i].last_line = ending_lines[i];
  }
  SetEntries(std::move(entries));
}

void Dictionary::SetEntries(std::vector<Entry> entries) {
  for (std::size_t i = 0; i < entries.size(); ++i)
    for (std::size_t j = i + 1; j < entries.size(); ++j)
      if (entries[i].keyword == entries[j].keyword)
        throw std::invalid_argument("Dictionary::SetEntries: the keyword " +
                                    entries[i].keyword +
                                    " is specified twice");
  entries_ = std::move(entries);
}

void Dictionary::SetLocation(SourceLocation location) {
  location_ = std::move(location);
}

void Dictionary::Summary(std::ostream &fout) const {
  fout << "Dictionary " << name_ << " defined in file " << file_name_
       << std::endl;
  for (const auto &entry : entries_)
    fout << entry.keyword << "*" << entry.option << "*" << entry.first_line
         << "-" << entry.last_line << std::endl;
}

void Dictionary::Checks() {
  std::vector<std::string> keywords;
  keywords.reserve(entries_.size());
  for (const auto &entry : entries_)
    keywords.push_back(entry.keyword);

  SourceLocation header = location_;
  if (header.file_name.empty())
    header.file_name = file_name_;

  if (grammar_.HasHelpKeyWord(keywords)) {
    Diagnostic help;
    help.code = DiagnosticCode::HelpRequested;
    help.location = location_of("@Help");
    help.message = "simulation stopped because the user requested @Help\n" +
                   grammar_.AvailableKeyWordsTable(name_, unit_system_);
    Fail({std::move(help)});
  }

  std::vector<Diagnostic> diagnostics;
  const auto convert = [&](std::vector<DictionaryGrammar::Violation> list) {
    for (auto &violation : list) {
      Diagnostic diagnostic;
      diagnostic.code = violation.code;
      diagnostic.message = std::move(violation.message);
      diagnostic.hint = std::move(violation.hint);
      diagnostic.location = violation.keyword.empty()
                                ? header
                                : location_of(violation.keyword);
      for (auto &[keyword, note] : violation.related)
        diagnostic.notes.push_back(Diagnostic{Severity::Note, violation.code,
                                              location_of(keyword),
                                              std::move(note), {}, {}});
      diagnostics.push_back(std::move(diagnostic));
    }
  };

  // Effective single value of a keyword: file value, or grammar default.
  const auto value_of =
      [this](std::string_view keyword) -> std::optional<std::string> {
    const std::size_t index = IndexOf(keyword);
    if (index != npos_index)
      return entries_[index].values.size() == 1
                 ? std::optional<std::string>(entries_[index].values[0])
                 : std::nullopt;
    const DictionaryKeyWord *declared = grammar_.FindKeyWord(keyword);
    if (declared == nullptr || !declared->default_value())
      return std::nullopt;
    const auto tokens = Lexer::tokenize(*declared->default_value(), "default");
    return tokens.size() == 1 ? std::optional<std::string>(tokens[0].text)
                              : std::nullopt;
  };

  const auto undefined = grammar_.CollectUndefinedKeyWords(keywords);
  const bool has_undefined = !undefined.empty();
  convert(undefined);
  convert(grammar_.CollectRuleViolations(keywords, value_of));

  // Values of every declared keyword written in the file.
  std::vector<Diagnostic> warnings;
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    const DictionaryKeyWord *declared =
        grammar_.FindKeyWord(entries_[i].keyword);
    if (declared == nullptr)
      continue;
    for (auto &problem :
         detail::validate_value(*declared, entries_[i].values, unit_system_)) {
      const SourceLocation where = problem.token == detail::keyword_token
                                       ? KeywordLocation(i)
                                       : TokenLocation(i, problem.token);
      std::string text = "keyword " + entries_[i].keyword + ": " +
                         std::move(problem.message);
      if (where.file_name.empty() && entries_[i].first_line != 0)
        text = "line " + std::to_string(entries_[i].first_line) + ", " + text;
      diagnostics.push_back(Diagnostic{Severity::Error, problem.code, where,
                                       std::move(text),
                                       std::move(problem.hint), {}});
    }
    if (declared->deprecated())
      warnings.push_back(Diagnostic{
          warnings_as_errors_ ? Severity::Error : Severity::Warning,
          DiagnosticCode::Deprecated, KeywordLocation(i),
          "the keyword " + entries_[i].keyword + " is deprecated",
          *declared->deprecated(), {}});
  }

  const bool failed = !diagnostics.empty() || (warnings_as_errors_ && !warnings.empty());
  std::move(warnings.begin(), warnings.end(), std::back_inserter(diagnostics));
  if (!failed) {
    warnings_ = std::move(diagnostics);
    return;
  }
  if (has_undefined)
    diagnostics.push_back(Diagnostic{
        Severity::Note, DiagnosticCode::UnknownKeyword, {},
        "keywords accepted by this dictionary:\n" +
            grammar_.AvailableKeyWordsTable(name_, unit_system_),
        {}, {}});
  Fail(std::move(diagnostics));
}

void Dictionary::RemoveDefaultEntries() {
  entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                [](const Entry &entry) {
                                  return entry.from_default;
                                }),
                 entries_.end());
}

void Dictionary::AddDefaultEntries() {
  for (const auto &keyword : grammar_.keywords()) {
    if (!keyword.default_value() || IndexOf(keyword.name()) != npos_index)
      continue;
    Entry entry;
    entry.keyword = keyword.name();
    entry.from_default = true;
    for (auto &token : Lexer::tokenize(*keyword.default_value(),
                                       "default of " + keyword.name())) {
      if (!entry.option.empty())
        entry.option += ' ';
      entry.option += token.spelling;
      entry.values.push_back(token.text);
      token.line = 0; // not a position in the input file
      token.column = 0;
      entry.tokens.push_back(std::move(token));
    }
    entries_.push_back(std::move(entry));
  }
}

std::vector<std::string> Dictionary::UnusedKeywords() const {
  std::vector<std::string> unused;
  for (const auto &entry : entries_)
    if (!entry.from_default && grammar_.FindKeyWord(entry.keyword) != nullptr &&
        !used_.contains(entry.keyword))
      unused.push_back(entry.keyword);
  return unused;
}

std::vector<Diagnostic> Dictionary::CheckUnusedKeywords() const {
  std::vector<Diagnostic> warnings;
  for (const auto &keyword : UnusedKeywords())
    warnings.push_back(Diagnostic{
        warnings_as_errors_ ? Severity::Error : Severity::Warning,
        DiagnosticCode::UnusedKeyword, location_of(keyword),
        "the keyword " + keyword + " is never read by the program",
        "check the keyword name used in the code that reads dictionary " +
            name_,
        {}});
  if (warnings_as_errors_ && !warnings.empty())
    Fail(std::move(warnings));
  return warnings;
}

void Dictionary::SetGrammar(DictionaryGrammar &grammar) {
  has_grammar_ = false;
  RemoveDefaultEntries();
  warnings_.clear();
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
  AddDefaultEntries();
  used_.clear();
  has_grammar_ = true;
}

void Dictionary::SetGrammar(std::string_view file_name) {
  has_grammar_ = false;
  RemoveDefaultEntries();
  warnings_.clear();
  grammar_.name_ = name_;
  grammar_.ReadFromFile(file_name);
  Checks();
  AddDefaultEntries();
  used_.clear();
  has_grammar_ = true;
}

bool Dictionary::CheckOption(std::string_view name_of_keyword) const {
  return IndexOf(name_of_keyword) != npos_index;
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

  std::size_t found = IndexOf(name_of_keyword);
  if (found == npos_index)
    for (std::size_t i = 0; i < entries_.size(); ++i)
      if (entries_[i].from_default && entries_[i].keyword == name_of_keyword)
        found = i;
  if (found == npos_index)
    return ParseError{ParseErrorCode::MissingKeyword,
                      "the required keyword " + keyword +
                          " is not present in the dictionary",
                      0};

  used_.insert(keyword);
  index = found;
  return std::nullopt;
}

std::size_t
Dictionary::CheckOption(std::string_view name_of_keyword,
                        const DictionaryKeyWordTypes expected_type) const {
  std::size_t index = 0;
  if (const auto error = Lookup(name_of_keyword, expected_type, index)) {
    DiagnosticCode code = DiagnosticCode::Other;
    SourceLocation where;
    switch (error->code) {
    case ParseErrorCode::NoGrammar: code = DiagnosticCode::NoGrammar; break;
    case ParseErrorCode::UndeclaredKeyword:
      code = DiagnosticCode::UndeclaredKeyword;
      break;
    case ParseErrorCode::WrongType: code = DiagnosticCode::WrongType; break;
    case ParseErrorCode::MissingKeyword:
      code = DiagnosticCode::MissingKeyword;
      where = location_of(name_of_keyword);
      break;
    default: code = DiagnosticCode::InvalidValue; break;
    }
    Fail({Diagnostic{Severity::Error, code, where, error->message, {}, {}}});
  }
  return index;
}

auto Dictionary::TryReadDouble(std::string_view name_of_keyword)
    -> ParseResult<double> {
  std::size_t index = 0;
  if (auto error = Lookup(name_of_keyword, SINGLE_DOUBLE, index))
    return ParseResult<double>::Failure(std::move(*error));

  const auto &tokens = entries_[index].values;
  if (tokens.size() != 1)
    return ParseResult<double>::Failure(ParseError{
        ParseErrorCode::BadTokenCount,
        "the keyword " + std::string(name_of_keyword) +
            " requires exactly one number (double), but " +
            std::to_string(tokens.size()) + " values were given",
        entries_[index].first_line, entries_[index].column});

  double value = 0.0;
  if (!parse_number(tokens[0], value))
    return ParseResult<double>::Failure(
        ParseError{ParseErrorCode::BadNumber,
                   "the keyword " + std::string(name_of_keyword) +
                       " requires a finite number (double), but " +
                       quote(tokens[0]) + " was given",
                   entries_[index].first_line,
                   entries_[index].tokens.empty()
                       ? entries_[index].column
                       : entries_[index].tokens[0].column});

  return ParseResult<double>::Success(value);
}

void Dictionary::ReadDouble(std::string_view name_of_keyword, double &value) {
  const std::size_t index = CheckOption(name_of_keyword, SINGLE_DOUBLE);
  const auto result = TryReadDouble(name_of_keyword);
  if (!result) {
    const ParseError &error = result.error();
    const std::size_t token =
        error.code == ParseErrorCode::BadNumber ? 0 : npos_index;
    std::string message = error.message;
    const std::string prefix = "the keyword " + entries_[index].keyword + " ";
    if (message.starts_with(prefix))
      message = message.substr(prefix.size());
    EntryError(index, message, token);
  }
  value = result.value();
}

void Dictionary::ReadMeasure(std::string_view name_of_keyword, double &value,
                             std::string &units) {
  const std::size_t index = CheckOption(name_of_keyword, SINGLE_MEASURE);
  const auto &tokens = entries_[index].values;
  if (tokens.size() != 2)
    EntryError(index, "a measure requires exactly two values (number and "
                      "units, e.g. '300 K'), but " +
                          std::to_string(tokens.size()) + " were given");
  if (!parse_number(tokens[0], value))
    EntryError(index,
               "the magnitude of a measure must be a finite number, "
               "but " +
                   quote(tokens[0]) + " was given",
               0);
  units = tokens[1];
}

void Dictionary::ReadInt(std::string_view name_of_keyword, int &value) {
  const std::size_t index = CheckOption(name_of_keyword, SINGLE_INT);
  const auto &tokens = entries_[index].values;
  if (tokens.size() != 1)
    EntryError(index, "exactly one integer is required, but " +
                          std::to_string(tokens.size()) + " values were given");
  if (!parse_number(tokens[0], value))
    EntryError(index,
               "an integer is required, but " + quote(tokens[0]) + " was given",
               0);
}

void Dictionary::ReadPath(std::string_view name_of_keyword,
                          std::filesystem::path &value) {
  const std::size_t index = CheckOption(name_of_keyword, SINGLE_PATH);
  const auto &tokens = entries_[index].values;
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
  const auto &tokens = entries_[index].values;
  if (tokens.size() != 1)
    EntryError(index, "exactly one dictionary name is required, but " +
                          std::to_string(tokens.size()) + " values were given");
  value = tokens[0];
}

void Dictionary::ReadSequence(std::string_view name_of_keyword,
                              std::string &value) {
  const std::size_t index = CheckOption(name_of_keyword, SEQUENCE_STRING);
  value = entries_[index].option;
}

void Dictionary::ReadBool(std::string_view name_of_keyword, bool &value) {
  const std::size_t index = CheckOption(name_of_keyword, SINGLE_BOOL);
  const auto &tokens = entries_[index].values;
  if (tokens.size() != 1)
    EntryError(index, "exactly one boolean (true/false/on/off) is required, "
                      "but " +
                          std::to_string(tokens.size()) + " values were given");
  if (!parse_bool_token(tokens[0], value))
    EntryError(index,
               "a boolean (true/false/on/off) is required, but " +
                   quote(tokens[0]) + " was given",
               0);
}

void Dictionary::ReadChar(std::string_view name_of_keyword, char &value) {
  const std::size_t index = CheckOption(name_of_keyword, SINGLE_CHAR);
  const auto &tokens = entries_[index].values;
  if (tokens.size() != 1)
    EntryError(index, "exactly one character is required, but " +
                          std::to_string(tokens.size()) + " values were given");
  if (!parse_char_token(tokens[0], value))
    EntryError(index,
               "a single character is required, but " + quote(tokens[0]) +
                   " was given",
               0);
}

void Dictionary::ReadString(std::string_view name_of_keyword,
                            std::string &value) {
  const std::size_t index = CheckOption(name_of_keyword, SINGLE_STRING);
  const auto &tokens = entries_[index].values;
  if (tokens.size() != 1)
    EntryError(index, "exactly one string is required (use quotes for text "
                      "containing blanks), but " +
                          std::to_string(tokens.size()) + " values were given");
  value = tokens[0];
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<std::string> &values) {
  const std::size_t index = CheckOption(name_of_keyword, VECTOR_STRING);
  const auto &tokens = entries_[index].values;
  if (tokens.empty())
    EntryError(index, "a list of strings is required, but no value was given");
  values = tokens;
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<double> &values) {
  const std::size_t index = CheckOption(name_of_keyword, VECTOR_DOUBLE);
  const auto &tokens = entries_[index].values;
  if (tokens.empty())
    EntryError(index,
               "a list of numbers (double) is required, but no value was given");
  values.resize(tokens.size());
  for (std::size_t i = 0; i < tokens.size(); ++i)
    if (!parse_number(tokens[i], values[i]))
      EntryError(index, "value " + std::to_string(i + 1) +
                            " must be a finite number (double), but " +
                            quote(tokens[i]) + " was given", i);
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<int> &values) {
  const std::size_t index = CheckOption(name_of_keyword, VECTOR_INT);
  const auto &tokens = entries_[index].values;
  if (tokens.empty())
    EntryError(index, "a list of integers is required, but no value was given");
  values.resize(tokens.size());
  for (std::size_t i = 0; i < tokens.size(); ++i)
    if (!parse_number(tokens[i], values[i]))
      EntryError(index, "value " + std::to_string(i + 1) +
                            " must be an integer, but " + quote(tokens[i]) +
                            " was given", i);
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<char> &values) {
  const std::size_t index = CheckOption(name_of_keyword, VECTOR_CHAR);
  const auto &tokens = entries_[index].values;
  if (tokens.empty())
    EntryError(index,
               "a list of characters is required, but no value was given");
  values.resize(tokens.size());
  for (std::size_t i = 0; i < tokens.size(); ++i)
    if (!parse_char_token(tokens[i], values[i]))
      EntryError(index, "value " + std::to_string(i + 1) +
                            " must be a single character, but " +
                            quote(tokens[i]) + " was given", i);
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            BoolVector &values) {
  const std::size_t index = CheckOption(name_of_keyword, VECTOR_BOOL);
  const auto &tokens = entries_[index].values;
  if (tokens.empty())
    EntryError(index, "a list of booleans is required, but no value was given");
  values.clear();
  values.reserve(tokens.size());
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    bool flag = false;
    if (!parse_bool_token(tokens[i], flag))
      EntryError(index, "value " + std::to_string(i + 1) +
                            " must be a boolean (true/false/on/off), but " +
                            quote(tokens[i]) + " was given", i);
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
  const auto &tokens = entries_[index].values;
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
      EntryError(index,
                 "the value paired with " + quote(tokens[i]) +
                     " must be a finite number (double), but " +
                     quote(tokens[i + 1]) + " was given",
                 i + 1);
  }
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<int> &values,
                            std::vector<std::string> &names) {
  const std::size_t index = CheckOption(name_of_keyword, VECTOR_INT_STRING);
  const auto &tokens = entries_[index].values;
  const std::size_t n = tokens.size();
  if (n == 0 || n % 2 != 0)
    EntryError(index, "pairs of an integer and a name are required, but " +
                          std::to_string(n) + " values were given");
  names.resize(n / 2);
  values.resize(n / 2);
  for (std::size_t i = 0; i < n; i += 2) {
    if (!parse_number(tokens[i], values[i / 2]))
      EntryError(index,
                 "pair " + std::to_string(i / 2 + 1) +
                     " must start with an integer, but " + quote(tokens[i]) +
                     " was given",
                 i);
    names[i / 2] = tokens[i + 1];
  }
}

void Dictionary::ReadOption(std::string_view name_of_keyword,
                            std::vector<double> &values,
                            std::vector<std::string> &names) {
  const std::size_t index = CheckOption(name_of_keyword, VECTOR_MEASURE);
  if (const auto error = split_measures(entries_[index].values, values, names))
    EntryError(index, *error);
}

auto Dictionary::ResolveDimension(std::string_view name_of_keyword,
                                  const units::Dimension *expected) const
    -> units::Dimension {
  const DictionaryKeyWord *declared = grammar_.FindKeyWord(name_of_keyword);
  const auto &grammar_dimension = declared->dimension();
  if (grammar_dimension && expected != nullptr) {
    if (!(*grammar_dimension == *expected) ||
        grammar_dimension->temperature_difference !=
            expected->temperature_difference)
      ErrorMessage("the keyword " + std::string(name_of_keyword) +
                   " is declared with dimension " +
                   units::describe(*grammar_dimension) +
                   " in the grammar, but it was read as " +
                   units::describe(*expected),
               DiagnosticCode::WrongDimension);
    return *grammar_dimension;
  }
  if (grammar_dimension)
    return *grammar_dimension;
  if (expected != nullptr)
    return *expected;
  ErrorMessage("the keyword " + std::string(name_of_keyword) +
               " declares no dimension in the grammar: declare it with "
               "DictionaryKeyWord::SetDimension() (or a 'dimension:' line in a "
               "grammar file), or pass the expected dimension to the read "
               "function",
               DiagnosticCode::WrongDimension);
}

void Dictionary::ReadMeasure(std::string_view name_of_keyword, double &value) {
  const std::size_t index = CheckOption(name_of_keyword, SINGLE_MEASURE);
  const units::Dimension dimension = ResolveDimension(name_of_keyword, nullptr);
  double magnitude = 0.0;
  std::string unit;
  ReadMeasure(name_of_keyword, magnitude, unit);
  try {
    value = units::to_si(magnitude, unit, dimension, unit_system_);
  } catch (const units::UnitError &error) {
    EntryError(index, error.what(), 1, DiagnosticCode::InvalidUnit);
  }
}

void Dictionary::ReadMeasure(std::string_view name_of_keyword, double &value,
                             const units::Dimension &expected) {
  const std::size_t index = CheckOption(name_of_keyword, SINGLE_MEASURE);
  const units::Dimension dimension =
      ResolveDimension(name_of_keyword, &expected);
  double magnitude = 0.0;
  std::string unit;
  ReadMeasure(name_of_keyword, magnitude, unit);
  try {
    value = units::to_si(magnitude, unit, dimension, unit_system_);
  } catch (const units::UnitError &error) {
    EntryError(index, error.what(), 1, DiagnosticCode::InvalidUnit);
  }
}

void Dictionary::ReadMeasures(std::string_view name_of_keyword,
                              std::vector<double> &values) {
  const std::size_t index = CheckOption(name_of_keyword, VECTOR_MEASURE);
  const units::Dimension dimension = ResolveDimension(name_of_keyword, nullptr);
  std::vector<std::string> unit_names;
  ReadOption(name_of_keyword, values, unit_names);
  const bool shared = values.size() + 1 == entries_[index].values.size();
  for (std::size_t i = 0; i < values.size(); ++i) {
    try {
      values[i] =
          units::to_si(values[i], unit_names[i], dimension, unit_system_);
    } catch (const units::UnitError &error) {
      EntryError(index, error.what(),
                 shared ? entries_[index].values.size() - 1 : 2 * i + 1,
                 DiagnosticCode::InvalidUnit);
    }
  }
}

void Dictionary::ReadMeasures(std::string_view name_of_keyword,
                              std::vector<double> &values,
                              const units::Dimension &expected) {
  const std::size_t index = CheckOption(name_of_keyword, VECTOR_MEASURE);
  const units::Dimension dimension =
      ResolveDimension(name_of_keyword, &expected);
  std::vector<std::string> unit_names;
  ReadOption(name_of_keyword, values, unit_names);
  const bool shared = values.size() + 1 == entries_[index].values.size();
  for (std::size_t i = 0; i < values.size(); ++i) {
    try {
      values[i] =
          units::to_si(values[i], unit_names[i], dimension, unit_system_);
    } catch (const units::UnitError &error) {
      EntryError(index, error.what(),
                 shared ? entries_[index].values.size() - 1 : 2 * i + 1,
                 DiagnosticCode::InvalidUnit);
    }
  }
}


} // namespace OpenSMOKEpp
