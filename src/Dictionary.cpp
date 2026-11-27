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
#include "DictionaryValues.h"

#include <algorithm>
#include <iterator>
#include <ostream>
#include <utility>

namespace OpenSMOKEpp {
namespace {
constexpr std::size_t npos_index = static_cast<std::size_t>(-1);

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
  ReportWarnings();
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
  ReportWarnings();
}

void Dictionary::ReportWarnings() {
  if (warning_stream_ == nullptr)
    return;
  for (const auto &warning : warnings_) {
    std::string text = format_diagnostic(warning);
    if (reported_warnings_.insert(text).second)
      *warning_stream_ << text << '\n';
  }
  warning_stream_->flush();
}

bool Dictionary::CheckOption(std::string_view name_of_keyword) const {
  return IndexOf(name_of_keyword) != npos_index;
}

void Dictionary::FailLookup(std::string_view name_of_keyword,
                            const ParseError &error) const {
  DiagnosticCode code = DiagnosticCode::Other;
  SourceLocation where;
  switch (error.code) {
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
  Fail({Diagnostic{Severity::Error, code, where, error.message, {}, {}}});
}

auto Dictionary::LookupAny(std::string_view name_of_keyword,
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

Value Dictionary::ReadValue(std::string_view name_of_keyword) {
  const DictionaryKeyWord *declared =
      has_grammar_ ? grammar_.FindKeyWord(name_of_keyword) : nullptr;
  return ParseWith(name_of_keyword, declared != nullptr
                                        ? detail::format_of(*declared)
                                        : formats::Text());
}

Value Dictionary::ReadValue(std::string_view name_of_keyword,
                            const units::Dimension &dimension) {
  const DictionaryKeyWord *declared =
      has_grammar_ ? grammar_.FindKeyWord(name_of_keyword) : nullptr;
  return ParseWith(name_of_keyword,
                   declared != nullptr
                       ? detail::format_of(*declared).WithDimension(dimension)
                       : formats::Text());
}

// (an absent grammar or keyword is reported by LookupAny before parsing)
Value Dictionary::ParseWith(std::string_view name_of_keyword,
                            const ValueFormat &format) {
  std::size_t index = 0;
  if (const auto error = LookupAny(name_of_keyword, index))
    FailLookup(name_of_keyword, *error);
  const DictionaryKeyWord &declared = *grammar_.FindKeyWord(name_of_keyword);
  if (declared.type() == NONE)
    return Value{};
  auto outcome =
      detail::parse_value(format, entries_[index].values, unit_system_);
  if (outcome.problems.empty())
    return std::move(outcome.value);
  std::vector<Diagnostic> diagnostics;
  for (auto &problem : outcome.problems) {
    const SourceLocation where = problem.token == detail::keyword_token
                                     ? KeywordLocation(index)
                                     : TokenLocation(index, problem.token);
    diagnostics.push_back(Diagnostic{
        Severity::Error, problem.code, where,
        "keyword " + entries_[index].keyword + ": " + problem.message,
        std::move(problem.hint), {}});
  }
  Fail(std::move(diagnostics));
}

void Dictionary::ThrowConversionError(std::string_view name_of_keyword,
                                      const std::string &reason) const {
  const DictionaryKeyWord *declared = grammar_.FindKeyWord(name_of_keyword);
  std::string message = "the value of " + std::string(name_of_keyword) +
                        " cannot be read as the requested C++ type: " + reason;
  if (declared != nullptr)
    message += " (the value is " +
               detail::format_of(*declared).describe(unit_system_) + ")";
  Fail({Diagnostic{Severity::Error, DiagnosticCode::WrongType, {},
                   std::move(message), {}, {}}});
}

auto Dictionary::ToParseError(const DictionaryError &error) -> ParseError {
  const Diagnostic &d = error.diagnostics().front();
  ParseErrorCode code = ParseErrorCode::InvalidValue;
  switch (d.code) {
  case DiagnosticCode::NoGrammar: code = ParseErrorCode::NoGrammar; break;
  case DiagnosticCode::UndeclaredKeyword:
    code = ParseErrorCode::UndeclaredKeyword;
    break;
  case DiagnosticCode::MissingKeyword:
    code = ParseErrorCode::MissingKeyword;
    break;
  case DiagnosticCode::WrongType: code = ParseErrorCode::WrongType; break;
  default: break;
  }
  return ParseError{code, d.message, d.location.line, d.location.column};
}

} // namespace OpenSMOKEpp
