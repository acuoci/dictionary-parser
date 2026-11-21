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

#include "test_framework.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

auto main(const int argc, char *argv[]) -> int {
  const std::vector<std::string> suites(argv + 1, argv + argc);
  int executed = 0;
  int failed_cases = 0;

  for (const auto &test : testing::registry()) {
    if (!suites.empty() &&
        std::find(suites.begin(), suites.end(), test.suite) == suites.end())
      continue;
    ++executed;
    const int before = testing::failures();
    try {
      test.body();
    } catch (const testing::RequireFailed &) {
      // already reported
    } catch (const std::exception &error) {
      testing::report_failure(__FILE__, __LINE__,
                              "unexpected exception: " +
                                  std::string(error.what()));
    }
    if (testing::failures() != before) {
      ++failed_cases;
      std::cerr << "FAILED: [" << test.suite << "] " << test.name << "\n\n";
    }
  }

  if (executed == 0) {
    std::cerr << "No test cases matched the requested suites.\n";
    return 1;
  }
  std::cout << executed - failed_cases << "/" << executed
            << " test cases passed\n";
  return failed_cases == 0 ? 0 : 1;
}
