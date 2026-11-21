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

#ifndef OpenSMOKEpp_DictionaryUtilities_H
#define OpenSMOKEpp_DictionaryUtilities_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace OpenSMOKEpp::detail {
/**
 * \brief Weighted edit distance used for spelling suggestions.
 *
 * Insertions, deletions and substitutions cost 2; a substitution that only
 * changes the letter case costs 1, so `Kg` is closer to `kg` than to `g`.
 */
[[nodiscard]] auto edit_distance(std::string_view a, std::string_view b)
    -> std::size_t;

/**
 * \brief Returns the candidate closest to `word`, if close enough.
 *
 * \param[in] word Misspelled word.
 * \param[in] candidates Valid words.
 * \return The best candidate, or an empty string if none is within about
 * one edit per four characters (at least one edit).
 */
[[nodiscard]] auto closest_match(std::string_view word,
                                 const std::vector<std::string> &candidates)
    -> std::string;
} // namespace OpenSMOKEpp::detail

#endif // OpenSMOKEpp_DictionaryUtilities_H
