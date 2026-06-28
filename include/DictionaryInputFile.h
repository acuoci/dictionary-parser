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

#ifndef OpenSMOKEpp_DictionaryInputFile_H
#define OpenSMOKEpp_DictionaryInputFile_H

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace OpenSMOKEpp {
/**
 * \brief Stores a preprocessed dictionary input file.
 *
 * The class reads a physical input file, strips `//` comments, replaces tabs
 * with spaces, classifies blank and nonblank lines, and preserves 1-based
 * source-line indices for diagnostics.
 */
class DictionaryInputFile {
public:
  /**
   * \brief Constructs an empty input-file container.
   *
   * \note Line counters are initialized to zero.
   */
  DictionaryInputFile();

  /**
   * \brief Reads and preprocesses a dictionary input file.
   *
   * \param[in] file_name Path to the input file.
   * \warning Throws `std::runtime_error` if the file cannot be opened or
   * read successfully.
   */
  explicit DictionaryInputFile(const std::string &file_name);

  /**
   * \brief Copies all preprocessed file state from another object.
   *
   * \param[in] orig Source object.
   */
  DictionaryInputFile(const DictionaryInputFile &orig) = default;

  /**
   * \brief Destroys the input-file container.
   */
  virtual ~DictionaryInputFile() = default;

public:
  /**
   * \brief Returns the total number of classified lines.
   *
   * \return Number of blank plus nonblank lines.
   * \note This function is `[[nodiscard]]` and `noexcept`.
   */
  [[nodiscard]] int number_of_lines() const noexcept {
    return number_of_lines_;
  }

  /**
   * \brief Returns the number of blank or comment-only lines.
   *
   * \return Number of entries in `blank_lines()`.
   * \note This function is `[[nodiscard]]` and `noexcept`.
   */
  [[nodiscard]] int number_of_blank_lines() const noexcept {
    return number_of_blank_lines_;
  }

  /**
   * \brief Returns the number of nonblank lines available to the parser.
   *
   * \return Number of entries in `good_lines()`.
   * \note This function is `[[nodiscard]]` and `noexcept`.
   */
  [[nodiscard]] int number_of_good_lines() const noexcept {
    return number_of_good_lines_;
  }

  /**
   * \brief Returns original line indices for nonblank lines.
   *
   * \return Constant reference to 1-based source-line indices.
   * \note This function is `[[nodiscard]]` and `noexcept`.
   */
  [[nodiscard]] const std::vector<int> &indices_of_good_lines() const noexcept {
    return indices_of_good_lines_;
  }

  /**
   * \brief Returns original line indices for blank lines.
   *
   * \return Constant reference to 1-based source-line indices.
   * \note This function is `[[nodiscard]]` and `noexcept`.
   */
  [[nodiscard]] const std::vector<int> &
  indices_of_blank_lines() const noexcept {
    return indices_of_blank_lines_;
  }

  /**
   * \brief Returns nonblank preprocessed lines.
   *
   * \return Constant reference to lines after comment stripping and tab
   * normalization.
   * \note This function is `[[nodiscard]]` and `noexcept`.
   */
  [[nodiscard]] const std::vector<std::string> &good_lines() const noexcept {
    return good_lines_;
  }

  /**
   * \brief Returns all preprocessed lines.
   *
   * \return Constant reference to all physical lines after preprocessing.
   * \note This function is `[[nodiscard]]` and `noexcept`.
   */
  [[nodiscard]] const std::vector<std::string> &clean_lines() const noexcept {
    return clean_lines_;
  }

  /**
   * \brief Returns mutable access to all preprocessed lines.
   *
   * \return Mutable reference to all physical lines after preprocessing.
   * \warning Mutating this vector can invalidate parser assumptions about
   * line ordering and diagnostics.
   * \note This function is `[[nodiscard]]` and `noexcept`.
   */
  [[nodiscard]] std::vector<std::string> &clean_lines() noexcept {
    return clean_lines_;
  }

  /**
   * \brief Returns blank or comment-only preprocessed lines.
   *
   * \return Constant reference to blank-line storage.
   * \note This function is `[[nodiscard]]` and `noexcept`.
   */
  [[nodiscard]] const std::vector<std::string> &blank_lines() const noexcept {
    return blank_lines_;
  }

  /**
   * \brief Returns the input file name without its parent path.
   *
   * \return File-name component of the stored path.
   * \note This function is `[[nodiscard]]`.
   */
  [[nodiscard]] std::filesystem::path file_name() const {
    return file_name_.filename();
  }

  /**
   * \brief Returns the parent directory of the input file.
   *
   * \return Parent path component of the stored path.
   * \note This function is `[[nodiscard]]`.
   */
  [[nodiscard]] std::filesystem::path folder_path() const {
    return file_name_.parent_path();
  }

  /**
   * \brief Returns the current size of the input file in bytes.
   *
   * \return File size in bytes as reported by `std::filesystem`.
   * \warning Throws `std::filesystem::filesystem_error` if the file cannot
   * be queried.
   * \note This function is `[[nodiscard]]`.
   */
  [[nodiscard]] std::uintmax_t size() const {
    return std::filesystem::file_size(file_name_);
  }

  /**
   * \brief Writes file status information to an output stream.
   *
   * \param[out] fOut Stream receiving file name, path, size, and line
   * counters.
   */
  void Status(std::ostream &fOut) const;

private:
  /** \brief Total number of classified physical lines. */
  int number_of_lines_ = 0;

  /** \brief Number of nonblank preprocessed lines. */
  int number_of_good_lines_ = 0;

  /** \brief Number of blank or comment-only preprocessed lines. */
  int number_of_blank_lines_ = 0;

  /** \brief 1-based original indices for `good_lines_`. */
  std::vector<int> indices_of_good_lines_;

  /** \brief 1-based original indices for `blank_lines_`. */
  std::vector<int> indices_of_blank_lines_;

  /** \brief Nonblank preprocessed lines. */
  std::vector<std::string> good_lines_;

  /** \brief Blank or comment-only preprocessed lines. */
  std::vector<std::string> blank_lines_;

  /** \brief All physical lines after lexical preprocessing. */
  std::vector<std::string> clean_lines_;

  /** \brief Full path of the input file. */
  std::filesystem::path file_name_;
};
} // namespace OpenSMOKEpp

#endif /* OpenSMOKEpp_DictionaryInputFile_H */
