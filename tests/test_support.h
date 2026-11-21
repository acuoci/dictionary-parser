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

#ifndef OpenSMOKEpp_TestSupport_H
#define OpenSMOKEpp_TestSupport_H

#include "Dictionary.h"
#include "DictionaryGrammar.h"
#include "DictionaryKeyWord.h"
#include "DictionaryManager.h"
#include "DictionaryRules.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace testing {
// Grammar built from a list of keyword rules.
class ListGrammar : public OpenSMOKEpp::DictionaryGrammar {
public:
  explicit ListGrammar(std::vector<OpenSMOKEpp::DictionaryKeyWord> keywords)
      : rules_(std::move(keywords)) {}

protected:
  void DefineRules() override {
    for (const auto &keyword : rules_)
      AddKeyWord(keyword);
  }

private:
  std::vector<OpenSMOKEpp::DictionaryKeyWord> rules_;
};

// Grammar built from keyword rules and structural rules.
class RuleGrammar : public OpenSMOKEpp::DictionaryGrammar {
public:
  RuleGrammar(std::vector<OpenSMOKEpp::DictionaryKeyWord> keywords,
              std::vector<OpenSMOKEpp::Rule> rules)
      : keywords_list_(std::move(keywords)), rules_list_(std::move(rules)) {}

protected:
  void DefineRules() override {
    for (const auto &keyword : keywords_list_)
      AddKeyWord(keyword);
    for (const auto &rule : rules_list_)
      AddRule(rule);
  }

private:
  std::vector<OpenSMOKEpp::DictionaryKeyWord> keywords_list_;
  std::vector<OpenSMOKEpp::Rule> rules_list_;
};

// Builds a grammar with rules and validates the dictionary against it.
inline void attach(OpenSMOKEpp::Dictionary &dictionary,
                   std::vector<OpenSMOKEpp::DictionaryKeyWord> keywords,
                   std::vector<OpenSMOKEpp::Rule> rules) {
  RuleGrammar grammar(std::move(keywords), std::move(rules));
  dictionary.SetGrammar(grammar);
}

// Shorthand constructors for keyword rules.
inline auto kw(const std::string &name, OpenSMOKEpp::DictionaryKeyWordTypes type,
               bool compulsory = false) -> OpenSMOKEpp::DictionaryKeyWord {
  return OpenSMOKEpp::DictionaryKeyWord(name, type, "test keyword", compulsory);
}

inline auto kw(const std::string &name, OpenSMOKEpp::DictionaryKeyWordTypes type,
               bool compulsory, const std::string &alternatives,
               const std::string &required, const std::string &conflicting)
    -> OpenSMOKEpp::DictionaryKeyWord {
  return OpenSMOKEpp::DictionaryKeyWord(name, type, "test keyword", compulsory,
                                        alternatives, required, conflicting);
}

// Parses a source text into a fresh manager.
inline auto parse(std::string_view text) -> OpenSMOKEpp::DictionaryManager {
  OpenSMOKEpp::DictionaryManager manager;
  manager.ReadDictionariesFromString(text, "test.dic");
  return manager;
}

// Builds a grammar and validates the dictionary `name` against it.
inline void attach(OpenSMOKEpp::Dictionary &dictionary,
                   std::vector<OpenSMOKEpp::DictionaryKeyWord> keywords) {
  ListGrammar grammar(std::move(keywords));
  dictionary.SetGrammar(grammar);
}

// Unique temporary directory removed at scope exit.
class TemporaryDirectory {
public:
  TemporaryDirectory() {
    static std::atomic<int> counter{0};
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("dictionary_parser_test_" + std::to_string(stamp) + "_" +
             std::to_string(counter++));
    std::filesystem::create_directories(path_);
  }
  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }
  TemporaryDirectory(const TemporaryDirectory &) = delete;
  TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

  [[nodiscard]] auto path() const -> const std::filesystem::path & {
    return path_;
  }

  auto write(const std::string &relative, std::string_view content) const
      -> std::filesystem::path {
    const auto file = path_ / relative;
    std::filesystem::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary);
    out << content;
    return file;
  }

private:
  std::filesystem::path path_;
};
} // namespace testing

#endif // OpenSMOKEpp_TestSupport_H
