cmake_minimum_required(VERSION 3.30)

project(DictionaryParser
	VERSION 1.0.0
	LANGUAGES CXX
)

enable_testing()

add_library(dictionary_parser
	src/Dictionary.cpp
	src/DictionaryFile.cpp
	src/DictionaryGrammar.cpp
	src/DictionaryInputFile.cpp
	src/DictionaryKeyWord.cpp
	src/DictionaryLexer.cpp
	src/DictionaryManager.cpp
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

add_executable(test_parser_smoke
	tests/test_parser_smoke.cpp
)

target_link_libraries(test_parser_smoke
	PRIVATE
		DictionaryParser::dictionary_parser
)

set_target_properties(test_parser_smoke PROPERTIES
	CXX_EXTENSIONS OFF
)

add_test(
	NAME Test_Example_Input_1
	COMMAND test_parser_smoke "${CMAKE_CURRENT_SOURCE_DIR}/examples/input.1.dic"
)

add_test(
	NAME Test_Example_Input_2
	COMMAND test_parser_smoke "${CMAKE_CURRENT_SOURCE_DIR}/examples/input.2.dic"
)
