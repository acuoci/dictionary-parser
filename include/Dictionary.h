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

#ifndef OpenSMOKEpp_Dictionary_H
#define OpenSMOKEpp_Dictionary_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "DictionaryGrammar.h"

namespace OpenSMOKEpp {
/**
 * \brief Stores and reads one parsed dictionary block.
 *
 * A dictionary contains keyword names, raw option strings, source line
 * ranges, and an optional grammar used to validate keyword availability and
 * typed reads. Values are read on demand through the `Read*` family.
 */
class Dictionary {
public:
  /** \brief Byte-backed boolean vector used to avoid `std::vector<bool>`
   * proxies. */
  using BoolVector = std::vector<std::uint8_t>;

  /**
   * \brief Describes the category of a structured parsing failure.
   */
  enum class ParseErrorCode {
    /** \brief The requested keyword is not present. */
    MissingKeyword,
    /** \brief The requested keyword exists but does not match the expected
       type. */
    WrongType,
    /** \brief The option has an invalid number of tokens. */
    BadTokenCount,
    /** \brief A token failed numeric conversion or finite-value validation. */
    BadNumber,
    /** \brief No grammar is attached to the dictionary. */
    NoGrammar,
    /** \brief The requested keyword is not declared in the grammar. */
    UndeclaredKeyword
  };

  /**
   * \brief Structured parse error returned by non-throwing read functions.
   */
  struct ParseError {
    /** \brief Error category suitable for programmatic handling. */
    ParseErrorCode code;

    /** \brief Human-readable diagnostic including keyword context. */
    std::string message;

    /** \brief 1-based source line when available, otherwise 0. */
    unsigned int line = 0;
  };

  /**
   * \brief Minimal expected-like result for typed parser reads.
   *
   * \tparam T Value type returned on success.
   */
  template <typename T> class ParseResult {
  public:
    /**
     * \brief Constructs a successful parse result.
     *
     * \param[in] value Parsed value.
     * \return Result containing `value`.
     */
    [[nodiscard]] static auto Success(T value) -> ParseResult {
      return ParseResult(std::move(value));
    }

    /**
     * \brief Constructs a failed parse result.
     *
     * \param[in] error Structured parse error.
     * \return Result containing `error`.
     */
    [[nodiscard]] static auto Failure(ParseError error) -> ParseResult {
      return ParseResult(std::move(error));
    }

    /**
     * \brief Reports whether the result contains a value.
     *
     * \return `true` on success, otherwise `false`.
     * \note This function is `[[nodiscard]]` and `noexcept`.
     */
    [[nodiscard]] auto has_value() const noexcept -> bool {
      return std::holds_alternative<T>(storage_);
    }

    /**
     * \brief Reports whether the result contains a value.
     *
     * \return `true` on success, otherwise `false`.
     * \note This function is `[[nodiscard]]` and `noexcept`.
     */
    [[nodiscard]] explicit operator bool() const noexcept {
      return has_value();
    }

    /**
     * \brief Returns the parsed value.
     *
     * \return Constant reference to the parsed value.
     * \warning Calling this function on a failed result throws
     * `std::bad_variant_access`.
     * \note This function is `[[nodiscard]]`.
     */
    [[nodiscard]] auto value() const -> const T & {
      return std::get<T>(storage_);
    }

    /**
     * \brief Returns the parse error.
     *
     * \return Constant reference to the structured parse error.
     * \warning Calling this function on a successful result throws
     * `std::bad_variant_access`.
     * \note This function is `[[nodiscard]]`.
     */
    [[nodiscard]] auto error() const -> const ParseError & {
      return std::get<ParseError>(storage_);
    }

  private:
    explicit ParseResult(T value) : storage_(std::move(value)) {}
    explicit ParseResult(ParseError error) : storage_(std::move(error)) {}

    std::variant<T, ParseError> storage_;
  };

  /**
   * \brief Writes all keyword entries and source line ranges.
   *
   * \param[out] fout Stream receiving diagnostic dictionary contents.
   */
  void Summary(std::ostream &fout) const;

  /**
   * \brief Sets the dictionary name.
   *
   * \param[in] name Name declared after the `Dictionary` token.
   */
  void SetName(const std::string_view name) { name_ = name; }

  /**
   * \brief Sets the source file name for diagnostics.
   *
   * \param[in] file_name Path or file name from which the dictionary was
   * read.
   */
  void SetFileName(const std::string_view file_name) { file_name_ = file_name; }

  /**
   * \brief Sets parsed keyword data and source line ranges.
   *
   * Value tokens are obtained by splitting each option on blanks (no quote
   * processing).
   *
   * \param[in] keywords Keyword names, each including leading `@`.
   * \param[in] options Raw option strings with trailing semicolons removed.
   * \param[in] starting_lines 1-based original start line for each keyword.
   * \param[in] ending_lines 1-based original end line for each keyword.
   * \warning Throws `std::invalid_argument` if the vectors differ in size.
   */
  void SetDictionary(std::vector<std::string> keywords,
                     std::vector<std::string> options,
                     std::vector<unsigned int> starting_lines,
                     std::vector<unsigned int> ending_lines);

  /**
   * \brief Sets parsed keyword data with explicit value tokens.
   *
   * \param[in] keywords Keyword names, each including leading `@`.
   * \param[in] options Option text as written (quotes kept), tokens joined
   * by single blanks.
   * \param[in] values Value tokens of each keyword (quotes removed).
   * \param[in] starting_lines 1-based original start line for each keyword.
   * \param[in] ending_lines 1-based original end line for each keyword.
   * \warning Throws `std::invalid_argument` if the vectors differ in size.
   */
  void SetDictionary(std::vector<std::string> keywords,
                     std::vector<std::string> options,
                     std::vector<std::vector<std::string>> values,
                     std::vector<unsigned int> starting_lines,
                     std::vector<unsigned int> ending_lines);

  /**
   * \brief Loads a grammar from file and validates this dictionary.
   *
   * \param[in] file_name Path to a grammar file.
   * \warning Throws `std::runtime_error` if the grammar file or dictionary
   * validation fails.
   */
  void SetGrammar(std::string_view file_name);

  /**
   * \brief Applies a hard-coded grammar and validates this dictionary.
   *
   * \param[in] grammar Grammar object whose `DefineRules()` method is invoked
   * before validation. Its base grammar state is restored before this function
   * returns or rethrows.
   * \warning Throws `std::runtime_error` if grammar construction or
   * dictionary validation fails.
   */
  void SetGrammar(DictionaryGrammar &grammar);

  /**
   * \brief Checks whether a keyword exists in this dictionary.
   *
   * \param[in] name_of_keyword Keyword name including leading `@`.
   * \return `true` if the keyword exists, otherwise `false`.
   * \note This function is `[[nodiscard]]`.
   */
  [[nodiscard]] bool CheckOption(std::string_view name_of_keyword) const;

  /**
   * \brief Tests whether a grammar has been successfully attached.
   *
   * \return `true` after a successful `SetGrammar` call.
   */
  [[nodiscard]] bool has_grammar() const noexcept { return has_grammar_; }

  /**
   * \brief Selects the target unit system of the converting reads.
   *
   * \param[in] system `units::si_kmol` (kilomole, the OpenSMOKE++
   * convention and the default) or `units::si` (strict SI, mole).
   */
  void SetUnitSystem(const units::UnitSystem &system) noexcept {
    unit_system_ = system;
  }

  /** \brief Returns the target unit system of the converting reads. */
  [[nodiscard]] const units::UnitSystem &unit_system() const noexcept {
    return unit_system_;
  }

  /**
   * \brief Reads one integer option.
   *
   * \param[in] option Keyword name including leading `@`.
   * \param[out] value Parsed signed integer.
   * \warning Throws `std::runtime_error` if the keyword is absent, has the
   * wrong grammar type, has the wrong token count, or cannot be converted.
   */
  void ReadInt(std::string_view option, int &value);

  /**
   * \brief Reads one floating-point option.
   *
   * \param[in] option Keyword name including leading `@`.
   * \param[out] value Parsed floating-point value.
   * \warning Throws `std::runtime_error` on absence, type mismatch, invalid
   * token count, or numeric conversion failure.
   */
  void ReadDouble(std::string_view option, double &value);

  /**
   * \brief Attempts to read one floating-point option without throwing.
   *
   * \param[in] option Keyword name including leading `@`.
   * \return Successful result containing the parsed value, or a structured
   * parse error with category, message, and source line.
   * \note This function is `[[nodiscard]]`.
   */
  [[nodiscard]] auto TryReadDouble(std::string_view option)
      -> ParseResult<double>;

  /**
   * \brief Reads one character option.
   *
   * \param[in] option Keyword name including leading `@`.
   * \param[out] value Parsed character.
   * \warning The option must contain exactly one one-character token.
   */
  void ReadChar(std::string_view option, char &value);

  /**
   * \brief Reads one string option.
   *
   * \param[in] option Keyword name including leading `@`.
   * \param[out] value Parsed token.
   * \warning The option must contain exactly one whitespace-delimited token.
   */
  void ReadString(std::string_view option, std::string &value);

  /**
   * \brief Reads one boolean option.
   *
   * \param[in] option Keyword name including leading `@`.
   * \param[out] value Parsed boolean.
   * \note Accepted true tokens are `true` and `on`; accepted false
   * tokens are `false` and `off`. Matching is case-insensitive.
   */
  void ReadBool(std::string_view option, bool &value);

  /**
   * \brief Reads one path option.
   *
   * \param[in] name_of_keyword Keyword name including leading `@`.
   * \param[out] value Parsed filesystem path.
   * \note A single (possibly quoted) token is used verbatim, so a quoted
   * path may contain blanks. Several unquoted tokens are joined with single
   * blanks (legacy behavior). No existence check is performed.
   */
  void ReadPath(std::string_view name_of_keyword, std::filesystem::path &value);

  /**
   * \brief Reads a raw string sequence option.
   *
   * \param[in] name_of_keyword Keyword name including leading `@`.
   * \param[out] value Raw option string after semicolon removal.
   * \note Tokens are returned as written (quotes kept), joined by single
   * blanks.
   */
  void ReadSequence(std::string_view name_of_keyword, std::string &value);

  /**
   * \brief Reads one measured scalar option.
   *
   * \param[in] option Keyword name including leading `@`.
   * \param[out] value Parsed floating-point magnitude.
   * \param[out] units Unit token following the magnitude.
   * \warning The parser validates the two-token shape but does not interpret
   * or convert units.
   */
  void ReadMeasure(std::string_view option, double &value, std::string &units);

  /**
   * \brief Reads one measured scalar and converts it to SI.
   *
   * The expected dimension is the one declared in the grammar with
   * `DictionaryKeyWord::SetDimension` (or a `dimension:` line in a grammar
   * file). For example `@Velocity 19.5 cm/s;` gives `0.195` when the
   * keyword is declared with `units::dim::Velocity`.
   *
   * \param[in] name_of_keyword Keyword name including leading `@`.
   * \param[out] value Value in SI units.
   * \warning Throws `std::runtime_error` if the keyword declares no
   * dimension, if the units are unknown or have a different dimension, or for
   * the same reasons as `ReadMeasure(name, value, units)`.
   */
  void ReadMeasure(std::string_view name_of_keyword, double &value);

  /**
   * \brief Reads one measured scalar and converts it to SI, with an explicit
   * expected dimension.
   *
   * Useful for grammars that do not declare dimensions. If the grammar does
   * declare one, it must be identical to `expected`.
   *
   * \param[in] name_of_keyword Keyword name including leading `@`.
   * \param[out] value Value in SI units.
   * \param[in] expected Expected dimension.
   * \warning Throws `std::runtime_error` as `ReadMeasure(name, value)`.
   */
  void ReadMeasure(std::string_view name_of_keyword, double &value,
                   const units::Dimension &expected);

  /**
   * \brief Reads a list of measures (`vector_measure`) converted to SI.
   *
   * Two layouts are accepted: pairs of number and units (`1 m 20 cm`) or
   * numbers followed by one shared unit (`1000 1100 1200 K`).
   *
   * \param[in] name_of_keyword Keyword name including leading `@`.
   * \param[out] values Values in SI units.
   * \warning Throws `std::runtime_error` if the keyword declares no
   * dimension or a value or unit is invalid.
   */
  void ReadMeasures(std::string_view name_of_keyword,
                    std::vector<double> &values);

  /**
   * \brief Reads a list of measures converted to SI, with an explicit
   * expected dimension.
   *
   * \param[in] name_of_keyword Keyword name including leading `@`.
   * \param[out] values Values in SI units.
   * \param[in] expected Expected dimension (must match the grammar's, if
   * declared).
   */
  void ReadMeasures(std::string_view name_of_keyword,
                    std::vector<double> &values,
                    const units::Dimension &expected);

  /**
   * \brief Reads one dictionary-name option.
   *
   * \param[in] name_of_keyword Keyword name including leading `@`.
   * \param[out] value Parsed dictionary name token.
   * \warning The target dictionary existence is not checked here.
   */
  void ReadDictionary(std::string_view name_of_keyword, std::string &value);

  /**
   * \brief Reads a vector of integer tokens.
   *
   * \param[in] option Keyword name including leading `@`.
   * \param[out] value Parsed integer vector.
   * \warning Throws if the option is empty or any token fails conversion.
   */
  void ReadOption(std::string_view option, std::vector<int> &value);

  /**
   * \brief Reads a vector of floating-point tokens.
   *
   * \param[in] option Keyword name including leading `@`.
   * \param[out] value Parsed floating-point vector.
   * \warning Throws if the option is empty or any token fails conversion.
   */
  void ReadOption(std::string_view option, std::vector<double> &value);

  /**
   * \brief Reads a vector of character tokens.
   *
   * \param[in] option Keyword name including leading `@`.
   * \param[out] value Parsed character vector.
   * \warning Every token must contain exactly one character.
   */
  void ReadOption(std::string_view option, std::vector<char> &value);

  /**
   * \brief Reads a vector of string tokens.
   *
   * \param[in] option Keyword name including leading `@`.
   * \param[out] value Parsed string-token vector.
   * \warning Throws if the option has no tokens.
   */
  void ReadOption(std::string_view option, std::vector<std::string> &value);

  /**
   * \brief Reads a vector of boolean tokens.
   *
   * \param[in] option Keyword name including leading `@`.
   * \param[out] value Parsed boolean vector.
   * \warning Throws if the option is empty or any token is not one of
   * `true`, `false`, `on`, or `off`.
   */
  void ReadOption(std::string_view option, BoolVector &value);

  /**
   * \brief Reads a vector of boolean tokens into `std::vector<bool>`.
   *
   * \param[in] option Keyword name including leading `@`.
   * \param[out] value Parsed boolean vector.
   * \warning This overload is retained for source compatibility. Prefer the
   * `BoolVector` overload to avoid `std::vector<bool>` proxy references.
   */
  void ReadOption(std::string_view option, std::vector<bool> &value);

  /**
   * \brief Reads repeated string/floating-point pairs.
   *
   * \param[in] name_of_keyword Keyword name including leading `@`.
   * \param[out] strings Parsed string elements at even pair positions.
   * \param[out] values Parsed floating-point elements at odd pair positions.
   * \warning The option token count must be positive and even.
   */
  void ReadOption(std::string_view name_of_keyword,
                  std::vector<std::string> &strings,
                  std::vector<double> &values);

  /**
   * \brief Reads repeated integer/string pairs.
   *
   * \param[in] name_of_keyword Keyword name including leading `@`.
   * \param[out] values Parsed integer elements at even pair positions.
   * \param[out] names Parsed string elements at odd pair positions.
   * \warning The option token count must be positive and even.
   */
  void ReadOption(std::string_view name_of_keyword, std::vector<int> &values,
                  std::vector<std::string> &names);

  /**
   * \brief Reads repeated floating-point/string pairs.
   *
   * \param[in] name_of_keyword Keyword name including leading `@`.
   * \param[out] values Parsed floating-point elements at even pair positions.
   * \param[out] strings Parsed string elements at odd pair positions.
   * \warning Used for `VECTOR_MEASURE`; unit semantics are left to callers.
   */
  void ReadOption(std::string_view name_of_keyword, std::vector<double> &values,
                  std::vector<std::string> &strings);

  /**
   * \brief Returns the dictionary name.
   *
   * \return Constant reference to the dictionary name.
   * \note This function is `[[nodiscard]]` and `noexcept`.
   */
  [[nodiscard]] const std::string &name() const noexcept { return name_; }

private:
  /** \brief Dictionary name declared in the input file. */
  std::string name_;

  /** \brief Source file path or file name used in diagnostics. */
  std::string file_name_;

  /** \brief Parsed keyword names, each including leading `@`. */
  std::vector<std::string> keywords_;

  /** \brief Raw option strings corresponding to `keywords_`. */
  std::vector<std::string> options_;

  /** \brief 1-based start line for each parsed keyword. */
  std::vector<unsigned int> starting_lines_;

  /** \brief 1-based end line for each parsed keyword. */
  std::vector<unsigned int> ending_lines_;

  /** \brief Value tokens (quotes removed) corresponding to `keywords_`. */
  std::vector<std::vector<std::string>> values_;

  /** \brief Grammar used for validation and typed-read checks. */
  DictionaryGrammar grammar_;

  /** \brief Whether a grammar was successfully attached. */
  bool has_grammar_ = false;

  /** \brief Target unit system of the converting reads. */
  units::UnitSystem unit_system_{};

private:
  /**
   * \brief Validates this dictionary against the current grammar.
   *
   * \warning Throws `std::runtime_error` for help requests, undefined
   * keywords, missing compulsory keywords, missing required keywords, or
   * conflicting keywords.
   */
  void Checks();

  /**
   * \brief Locates a keyword and verifies its grammar type.
   *
   * \param[in] name_of_keyword Keyword name including leading `@`.
   * \param[in] expected_type Expected grammar value type.
   * \return Zero-based index into `keywords_` and `options_`.
   * \warning Throws `std::runtime_error` if no grammar is attached, if the
   * keyword is not declared in the grammar, if its declared type differs
   * from `expected_type`, or if it is absent from the dictionary.
   * \note This function is `[[nodiscard]]`.
   */
  [[nodiscard]] std::size_t
  CheckOption(std::string_view name_of_keyword,
              const DictionaryKeyWordTypes expected_type) const;

  /**
   * \brief Raises a dictionary-scoped error.
   *
   * \param[in] message Diagnostic message without dictionary context.
   * \warning Always throws `std::runtime_error`.
   */
  [[noreturn]] void ErrorMessage(std::string_view message) const;

  /**
   * \brief Raises an error about the value of one keyword entry.
   *
   * \param[in] index Entry index.
   * \param[in] message Diagnostic message.
   * \warning Always throws `std::runtime_error`.
   */
  [[noreturn]] void EntryError(std::size_t index,
                               std::string_view message) const;

  /**
   * \brief Determines the dimension used to convert a measure keyword.
   *
   * \param[in] name_of_keyword Keyword name including leading `@`.
   * \param[in] expected Explicit dimension requested by the caller, or
   * `nullptr` to use the grammar's.
   * \return The dimension to use.
   * \warning Throws `std::runtime_error` if neither is available or if they
   * differ.
   */
  [[nodiscard]] auto ResolveDimension(std::string_view name_of_keyword,
                                      const units::Dimension *expected) const
      -> units::Dimension;

  /**
   * \brief Validates the units of measure keywords declaring a dimension.
   *
   * \return One message per invalid entry.
   */
  [[nodiscard]] auto CollectMeasureErrors() const -> std::vector<std::string>;

  /**
   * \brief Diagnoses a failed typed lookup without throwing.
   *
   * \param[in] name_of_keyword Keyword name including leading `@`.
   * \param[in] expected_type Expected grammar value type.
   * \param[out] index Entry index on success.
   * \return Empty optional on success, otherwise the error.
   */
  [[nodiscard]] auto Lookup(std::string_view name_of_keyword,
                            DictionaryKeyWordTypes expected_type,
                            std::size_t &index) const
      -> std::optional<ParseError>;
};
} // namespace OpenSMOKEpp

#endif // OpenSMOKEpp_Dictionary_H
