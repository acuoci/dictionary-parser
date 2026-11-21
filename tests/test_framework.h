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

#ifndef OpenSMOKEpp_TestFramework_H
#define OpenSMOKEpp_TestFramework_H

// Minimal, dependency-free unit-test harness.
//
//   TEST_CASE("suite", "name") { CHECK(a == b); CHECK_THROWS_WITH(f(), "text"); }
//
// The executable runs every test, or only the suites given on the command
// line, and returns a non-zero exit code if any check failed.

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace testing {
struct TestCase {
  std::string suite;
  std::string name;
  std::function<void()> body;
};

inline auto registry() -> std::vector<TestCase> & {
  static std::vector<TestCase> tests;
  return tests;
}

inline auto failures() -> int & {
  static int count = 0;
  return count;
}

struct Registrar {
  Registrar(std::string suite, std::string name, std::function<void()> body) {
    registry().push_back({std::move(suite), std::move(name), std::move(body)});
  }
};

inline void report_failure(const char *file, int line, const std::string &what) {
  ++failures();
  std::cerr << file << ":" << line << ": check failed: " << what << '\n';
}

// Thrown by REQUIRE to abort the current test case.
struct RequireFailed {};
} // namespace testing

#define TESTING_CONCAT_IMPL(a, b) a##b
#define TESTING_CONCAT(a, b) TESTING_CONCAT_IMPL(a, b)

#define TEST_CASE(suite, name)                                                 \
  static void TESTING_CONCAT(test_function_, __LINE__)();                      \
  static const ::testing::Registrar TESTING_CONCAT(test_registrar_, __LINE__)( \
      suite, name, &TESTING_CONCAT(test_function_, __LINE__));                 \
  static void TESTING_CONCAT(test_function_, __LINE__)()

#define CHECK(expression)                                                      \
  do {                                                                         \
    try {                                                                      \
      if (!(expression))                                                       \
        ::testing::report_failure(__FILE__, __LINE__, #expression);            \
    } catch (const std::exception &testing_error_) {                          \
      ::testing::report_failure(__FILE__, __LINE__,                            \
                                std::string(#expression) +                     \
                                    " threw: " + testing_error_.what());       \
    }                                                                          \
  } while (false)

#define REQUIRE(expression)                                                    \
  do {                                                                         \
    const int testing_before_ = ::testing::failures();                         \
    CHECK(expression);                                                         \
    if (::testing::failures() != testing_before_)                              \
      throw ::testing::RequireFailed{};                                        \
  } while (false)

#define CHECK_NOTHROW(expression)                                              \
  do {                                                                         \
    try {                                                                      \
      expression;                                                              \
    } catch (const std::exception &testing_error_) {                          \
      ::testing::report_failure(__FILE__, __LINE__,                            \
                                std::string(#expression) +                     \
                                    " threw: " + testing_error_.what());       \
    }                                                                          \
  } while (false)

// Checks that `expression` throws a std::exception whose what() contains
// `fragment`.
#define CHECK_THROWS_WITH(expression, fragment)                                \
  do {                                                                         \
    bool testing_thrown_ = false;                                              \
    try {                                                                      \
      expression;                                                              \
    } catch (const std::exception &testing_error_) {                          \
      testing_thrown_ = true;                                                  \
      const std::string testing_what_ = testing_error_.what();                 \
      if (testing_what_.find(fragment) == std::string::npos)                   \
        ::testing::report_failure(                                             \
            __FILE__, __LINE__,                                                \
            std::string(#expression) + " threw \"" + testing_what_ +           \
                "\", which does not contain \"" + (fragment) + "\"");          \
    }                                                                          \
    if (!testing_thrown_)                                                      \
      ::testing::report_failure(__FILE__, __LINE__,                            \
                                std::string(#expression) + " did not throw");  \
  } while (false)

// Same as CHECK_THROWS_WITH, but also checks the exception type.
#define CHECK_THROWS_AS_WITH(expression, type, fragment)                       \
  do {                                                                         \
    bool testing_thrown_ = false;                                              \
    try {                                                                      \
      expression;                                                              \
    } catch (const type &testing_error_) {                                     \
      testing_thrown_ = true;                                                  \
      const std::string testing_what_ = testing_error_.what();                 \
      if (testing_what_.find(fragment) == std::string::npos)                   \
        ::testing::report_failure(                                             \
            __FILE__, __LINE__,                                                \
            std::string(#expression) + " threw \"" + testing_what_ +           \
                "\", which does not contain \"" + (fragment) + "\"");          \
    } catch (const std::exception &testing_error_) {                          \
      testing_thrown_ = true;                                                  \
      ::testing::report_failure(__FILE__, __LINE__,                            \
                                std::string(#expression) +                     \
                                    " threw an unexpected exception type: " +  \
                                    testing_error_.what());                    \
    }                                                                          \
    if (!testing_thrown_)                                                      \
      ::testing::report_failure(__FILE__, __LINE__,                            \
                                std::string(#expression) + " did not throw");  \
  } while (false)

#endif // OpenSMOKEpp_TestFramework_H
