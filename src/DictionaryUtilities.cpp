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

#include "DictionaryUtilities.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace OpenSMOKEpp::detail {
namespace {
[[nodiscard]] auto lower(const char c) noexcept -> char {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}
} // namespace

auto edit_distance(const std::string_view a, const std::string_view b)
    -> std::size_t {
  std::vector<std::size_t> previous(b.size() + 1);
  std::vector<std::size_t> current(b.size() + 1);
  for (std::size_t j = 0; j <= b.size(); ++j)
    previous[j] = 2 * j;
  for (std::size_t i = 1; i <= a.size(); ++i) {
    current[0] = 2 * i;
    for (std::size_t j = 1; j <= b.size(); ++j) {
      const std::size_t cost = a[i - 1] == b[j - 1]               ? 0
                               : lower(a[i - 1]) == lower(b[j - 1]) ? 1
                                                                    : 2;
      current[j] = std::min({previous[j] + 2, current[j - 1] + 2,
                             previous[j - 1] + cost});
    }
    std::swap(previous, current);
  }
  return previous[b.size()];
}

auto closest_match(const std::string_view word,
                   const std::vector<std::string> &candidates) -> std::string {
  std::string best;
  std::size_t best_distance = std::numeric_limits<std::size_t>::max();
  for (const auto &candidate : candidates) {
    const std::size_t distance = edit_distance(word, candidate);
    if (distance < best_distance) {
      best_distance = distance;
      best = candidate;
    }
  }
  const std::size_t edits = std::max<std::size_t>(1, word.size() / 4);
  return best_distance <= 2 * edits ? best : std::string();
}
} // namespace OpenSMOKEpp::detail
