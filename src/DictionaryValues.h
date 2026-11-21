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

#ifndef OpenSMOKEpp_DictionaryValues_H
#define OpenSMOKEpp_DictionaryValues_H

#include <charconv>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

#include "DictionaryDiagnostics.h"
#include "DictionaryFormats.h"
#include "DictionaryKeyWord.h"
#include "DictionaryUnits.h"

namespace OpenSMOKEpp::detail {
/** \brief Index meaning "the keyword itself" rather than a value token. */
inline constexpr std::size_t keyword_token = static_cast<std::size_t>(-1);

/** \brief Strict number parsing (whole token, finite for floating point). */
template <typename Number>
[[nodiscard]] auto parse_number(const std::string_view token,
                                Number &value) noexcept -> bool {
  const auto *begin = token.data();
  const auto *end = token.data() + token.size();
  const auto [ptr, ec] = std::from_chars(begin, end, value);
  if (ec != std::errc{} || ptr != end)
    return false;
  if constexpr (std::is_floating_point_v<Number>)
    return std::isfinite(value);
  else
    return true;
}

/** \brief Returns `true` if the token is a finite number. */
[[nodiscard]] auto is_number(std::string_view token) noexcept -> bool;

/** \brief Parses `true`/`false`/`on`/`off`, case-insensitively. */
[[nodiscard]] auto parse_bool_token(std::string_view token, bool &value) noexcept
    -> bool;

/** \brief Wraps a token in single quotes for messages. */
[[nodiscard]] auto quote(std::string_view token) -> std::string;

/**
 * \brief Splits the tokens of a list of measures: pairs (`1 m 20 cm`) or
 * numbers followed by one shared unit (`1 2 3 m`).
 *
 * \return Empty on success, otherwise the reason.
 */
[[nodiscard]] auto split_measures(const std::vector<std::string> &tokens,
                                  std::vector<double> &values,
                                  std::vector<std::string> &units)
    -> std::optional<std::string>;

/** \brief One problem found in the value of a keyword. */
struct ValueProblem {
  /** \brief Category. */
  DiagnosticCode code = DiagnosticCode::InvalidValue;
  /** \brief Description (without the keyword name). */
  std::string message;
  /** \brief Offending token, or `keyword_token`. */
  std::size_t token = keyword_token;
  /** \brief Optional suggestion. */
  std::string hint;
};

/**
 * \brief Validates the value tokens of a keyword against its declaration:
 * type, number of tokens, units and dimension, allowed values, numeric
 * bounds, list size and sum.
 *
 * \param[in] keyword Keyword declaration.
 * \param[in] values Value tokens (quotes removed).
 * \param[in] system Unit system used to compare measures with their bounds.
 * \return All problems found (empty if the value is valid).
 */
[[nodiscard]] auto validate_value(const DictionaryKeyWord &keyword,
                                  const std::vector<std::string> &values,
                                  const units::UnitSystem &system)
    -> std::vector<ValueProblem>;
/** \brief Result of parsing a value with a format. */
struct ParseOutcome {
  /** \brief Parsed value (meaningful only without problems). */
  Value value;
  /** \brief Problems found. */
  std::vector<ValueProblem> problems;
};

/**
 * \brief Parses value tokens with a format.
 *
 * \param[in] format Format of the value.
 * \param[in] values Value tokens (quotes removed).
 * \param[in] system Unit system of the converted quantities.
 * \return The value and the problems found.
 */
[[nodiscard]] auto parse_value(const ValueFormat &format,
                               const std::vector<std::string> &values,
                               const units::UnitSystem &system) -> ParseOutcome;

/**
 * \brief Returns the format equivalent to the type of a keyword (the format
 * itself for `STRUCTURED` keywords).
 */
[[nodiscard]] auto format_of(const DictionaryKeyWord &keyword) -> ValueFormat;
} // namespace OpenSMOKEpp::detail

#endif // OpenSMOKEpp_DictionaryValues_H
