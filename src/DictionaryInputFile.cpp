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

#include "DictionaryLexer.h"
#include "DictionaryInputFile.h"

#include <filesystem>
#include <ostream>
#include <system_error>
#include <utility>

namespace OpenSMOKEpp {
DictionaryInputFile::DictionaryInputFile() = default;

DictionaryInputFile::DictionaryInputFile(const std::string &file_name) {
  auto preprocessed =
      Lexer::read_and_preprocess_file(std::filesystem::path(file_name));
  file_name_ = std::move(preprocessed.file_name);
  indices_of_good_lines_ = std::move(preprocessed.indices_of_good_lines);
  indices_of_blank_lines_ = std::move(preprocessed.indices_of_blank_lines);
  good_lines_ = std::move(preprocessed.good_lines);
  blank_lines_ = std::move(preprocessed.blank_lines);
  clean_lines_ = std::move(preprocessed.clean_lines);
  number_of_blank_lines_ = static_cast<int>(indices_of_blank_lines_.size());
  number_of_good_lines_ = static_cast<int>(indices_of_good_lines_.size());
  number_of_lines_ = number_of_blank_lines_ + number_of_good_lines_;
}

void DictionaryInputFile::Status(std::ostream &fOut) const {
  std::error_code file_size_error;
  const std::uintmax_t file_size =
      std::filesystem::file_size(file_name_, file_size_error);

  fOut << "Name:        " << file_name_.filename() << std::endl;
  fOut << "Path:        " << file_name_.parent_path() << std::endl;
  if (file_size_error)
    fOut << "Size:        unavailable (" << file_size_error.message() << ")"
         << std::endl;
  else
    fOut << "Size:        " << static_cast<double>(file_size) / 1000. << " kB"
         << std::endl;
  fOut << "Lines:       " << number_of_lines_ << std::endl;
  fOut << "Blank lines: " << number_of_blank_lines_ << std::endl;
}

} // namespace OpenSMOKEpp
