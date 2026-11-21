cmake_minimum_required(VERSION 3.30)

project(DictionaryParser
	VERSION 1.1.0
	LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(DICTIONARY_PARSER_BUILD_TESTS
	"Build the unit and integration tests" ${PROJECT_IS_TOP_LEVEL})
option(DICTIONARY_PARSER_WARNINGS_AS_ERRORS
	"Treat compiler warnings as errors" OFF)
option(DICTIONARY_PARSER_SANITIZE
	"Build with AddressSanitizer and UndefinedBehaviorSanitizer (GCC/Clang)" OFF)

set(_default_corpus "")
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../OpenSMOKEpp2")
	set(_default_corpus "${CMAKE_CURRENT_SOURCE_DIR}/../OpenSMOKEpp2")
endif()
set(DICTIONARY_PARSER_CORPUS_DIR "${_default_corpus}" CACHE PATH
	"Directory searched recursively for *.dic files by the corpus test (empty: disabled)")

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------
add_library(dictionary_parser
	src/Dictionary.cpp
	src/DictionaryDiagnostics.cpp
	src/DictionaryFormats.cpp
	src/DictionaryGrammar.cpp
	src/DictionaryInputFile.cpp
	src/DictionaryKeyWord.cpp
	src/DictionaryLexer.cpp
	src/DictionaryManager.cpp
	src/DictionaryParser.cpp
	src/DictionaryRules.cpp
	src/DictionaryUnits.cpp
	src/DictionaryUtilities.cpp
	src/DictionaryValues.cpp
)

add_library(DictionaryParser::dictionary_parser ALIAS dictionary_parser)

target_compile_features(dictionary_parser PUBLIC cxx_std_20)

target_include_directories(dictionary_parser
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(dictionary_parser PROPERTIES
	CXX_EXTENSIONS OFF
	EXPORT_NAME dictionary_parser
)

# Warning and sanitizer flags, applied to the library and to the tests.
add_library(dictionary_parser_build_flags INTERFACE)
if (MSVC)
	target_compile_options(dictionary_parser_build_flags INTERFACE /W4 /permissive-)
	if (DICTIONARY_PARSER_WARNINGS_AS_ERRORS)
		target_compile_options(dictionary_parser_build_flags INTERFACE /WX)
	endif()
else()
	target_compile_options(dictionary_parser_build_flags INTERFACE
		-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
		-Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual)
	if (DICTIONARY_PARSER_WARNINGS_AS_ERRORS)
		target_compile_options(dictionary_parser_build_flags INTERFACE -Werror)
	endif()
	if (DICTIONARY_PARSER_SANITIZE)
		target_compile_options(dictionary_parser_build_flags INTERFACE
			-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
		target_link_options(dictionary_parser_build_flags INTERFACE
			-fsanitize=address,undefined)
	endif()
endif()
target_link_libraries(dictionary_parser PRIVATE $<BUILD_INTERFACE:dictionary_parser_build_flags>)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if (DICTIONARY_PARSER_BUILD_TESTS)
	enable_testing()

	function(dictionary_parser_add_test_executable name)
		add_executable(${name} ${ARGN})
		target_link_libraries(${name} PRIVATE
			DictionaryParser::dictionary_parser dictionary_parser_build_flags)
		target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
		set_target_properties(${name} PROPERTIES CXX_EXTENSIONS OFF)
	endfunction()

	# Unit tests: one executable, one CTest entry per suite.
	dictionary_parser_add_test_executable(unit_tests
		tests/test_main.cpp
		tests/test_lexer.cpp
		tests/test_syntax.cpp
		tests/test_reads.cpp
		tests/test_grammar.cpp
		tests/test_units.cpp
		tests/test_diagnostics.cpp
		tests/test_rules.cpp
		tests/test_formats.cpp
	)
	target_compile_definitions(unit_tests PRIVATE
		DICTIONARY_PARSER_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/examples")
	foreach(suite lexer syntax include reads grammar validation units measures diagnostics rules semantics formats typed)
		add_test(NAME Unit_${suite} COMMAND unit_tests ${suite})
	endforeach()

	# Integration tests over the example input files.
	dictionary_parser_add_test_executable(test_parser_smoke tests/test_parser_smoke.cpp)
	add_test(NAME Test_Example_Input_1
		COMMAND test_parser_smoke "${CMAKE_CURRENT_SOURCE_DIR}/examples/input.1.dic")
	add_test(NAME Test_Example_Input_2
		COMMAND test_parser_smoke "${CMAKE_CURRENT_SOURCE_DIR}/examples/input.2.dic")
	add_test(NAME Test_Example_Syntax_Extensions
		COMMAND test_parser_smoke "${CMAKE_CURRENT_SOURCE_DIR}/examples/syntax-extensions/main.dic")

	dictionary_parser_add_test_executable(test_three_equations_grammar
		tests/test_three_equations_grammar.cpp)
	add_test(NAME Test_Example_Input_3_ThreeEquations_Grammar
		COMMAND test_three_equations_grammar "${CMAKE_CURRENT_SOURCE_DIR}/examples/input3.dic")

	# Corpus test: every *.dic file below DICTIONARY_PARSER_CORPUS_DIR must parse.
	dictionary_parser_add_test_executable(test_corpus tests/test_corpus.cpp)
	if (DICTIONARY_PARSER_CORPUS_DIR AND EXISTS "${DICTIONARY_PARSER_CORPUS_DIR}")
		add_test(NAME Test_Corpus COMMAND test_corpus "${DICTIONARY_PARSER_CORPUS_DIR}")
	endif()
endif()
