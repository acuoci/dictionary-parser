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

// Corpus test: parses every *.dic file found recursively below a directory.
//
//   test_corpus <directory> [--dump]
//
// Returns a non-zero exit code if any file fails to parse. With --dump, the
// parsed content of every file is written to the standard output (dictionary
// names, keywords, options and line ranges), which allows comparing the
// output of two versions of the parser.

#include "DictionaryManager.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

auto main(const int argc, char *argv[]) -> int {
  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: test_corpus <directory> [--dump]\n";
    return 1;
  }
  const std::filesystem::path root(argv[1]);
  const bool dump = argc == 3 && std::string(argv[2]) == "--dump";

  std::vector<std::filesystem::path> files;
  std::error_code error;
  for (auto it = std::filesystem::recursive_directory_iterator(
           root, std::filesystem::directory_options::skip_permission_denied,
           error);
       it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
    if (error)
      break;
    if (it->is_directory() && it->path().filename().string().starts_with("build")) {
      it.disable_recursion_pending();
      continue;
    }
    if (it->is_regular_file() && it->path().extension() == ".dic")
      files.push_back(it->path());
  }
  std::sort(files.begin(), files.end());

  int failures = 0;
  std::size_t dictionaries = 0;
  for (const auto &file : files) {
    if (dump)
      std::cout << "FILE " << file.string() << '\n';
    try {
      OpenSMOKEpp::DictionaryManager manager;
      manager.ReadDictionariesFromFile(file.string());
      for (const auto &name : manager.names()) {
        ++dictionaries;
        if (!dump)
          continue;
        std::ostringstream summary;
        manager(name).Summary(summary);
        const std::string text = summary.str();
        std::cout << " DICT " << name << '\n'
                  << text.substr(text.find('\n') + 1);
      }
    } catch (const std::exception &exception) {
      ++failures;
      if (dump)
        std::cout << " ERROR " << exception.what() << '\n';
      std::cerr << "FAILED " << file.string() << ": " << exception.what()
                << '\n';
    }
  }

  std::cerr << files.size() - static_cast<std::size_t>(failures) << "/"
            << files.size() << " files parsed (" << dictionaries
            << " dictionaries)\n";
  if (files.empty()) {
    std::cerr << "No *.dic files found below " << root.string() << '\n';
    return 1;
  }
  return failures == 0 ? 0 : 1;
}
