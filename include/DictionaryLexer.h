#ifndef OpenSMOKEpp_DictionaryLexer_H
#define	OpenSMOKEpp_DictionaryLexer_H

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace OpenSMOKEpp::Lexer
{
	struct PreprocessedFile
	{
		std::filesystem::path file_name;
		std::vector<int> indices_of_good_lines;
		std::vector<int> indices_of_blank_lines;
		std::vector<std::string> good_lines;
		std::vector<std::string> blank_lines;
		std::vector<std::string> clean_lines;
	};

	[[nodiscard]] auto trim(std::string_view text) noexcept -> std::string_view;
	[[nodiscard]] auto trim_copy(std::string_view text) -> std::string;
	[[nodiscard]] auto strip_comment(std::string_view text) noexcept -> std::string_view;
	[[nodiscard]] auto is_blank(std::string_view text) noexcept -> bool;
	[[nodiscard]] auto split_whitespace(std::string_view text) -> std::vector<std::string_view>;
	[[nodiscard]] auto count_char(std::string_view text, char value) noexcept -> std::size_t;
	[[nodiscard]] auto count_substring(std::string_view text, std::string_view needle) noexcept -> std::size_t;
	[[nodiscard]] auto count_substring_case_insensitive(std::string_view text, std::string_view needle) noexcept -> std::size_t;
	[[nodiscard]] auto contains_substring(std::string_view text, std::string_view needle) noexcept -> bool;

	auto replace_tabs_with_spaces(std::string& text) noexcept -> void;
	[[nodiscard]] auto read_and_preprocess_file(const std::filesystem::path& file_name) -> PreprocessedFile;
}

#endif	// OpenSMOKEpp_DictionaryLexer_H
