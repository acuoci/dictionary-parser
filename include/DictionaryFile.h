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

#ifndef OpenSMOKEpp_DictionaryFile_H
#define	OpenSMOKEpp_DictionaryFile_H

#include <iosfwd>
#include <string>
#include <vector>

#include "DictionaryInputFile.h"
#include "Dictionary.h"

namespace OpenSMOKEpp
{
	/**
	 * \brief Parses and validates one raw dictionary block.
	 *
	 * `DictionaryFile` receives the preprocessed physical lines belonging to a
	 * single dictionary block, validates block syntax, compacts multiline
	 * keyword entries, and transfers keyword/value data into a `Dictionary`.
	 */
	class DictionaryFile
	{
		public:
			/**
			 * \brief Sets the source file name for diagnostics.
			 *
			 * \param[in] file_name File name or path from which this dictionary
			 * block was extracted.
			 */
			void SetFileName(const std::string file_name) { file_name_ = file_name; }

			/**
			 * \brief Sets the 1-based source line where the dictionary block begins.
			 *
			 * \param[in] index_first_line Original file line containing
			 * `Dictionary <name>`.
			 * \note This function is `noexcept`.
			 */
			void SetFirstLine(const unsigned int index_first_line) noexcept { index_first_line_ = index_first_line; }

			/**
			 * \brief Appends a preprocessed physical line to this block.
			 *
			 * \param[in] line Line after comment stripping and tab normalization.
			 */
			void AddLine(const std::string line) { lines_.push_back(line); }

			/**
			 * \brief Validates block syntax and compacts keyword entries.
			 *
			 * \warning Throws `std::runtime_error` if the dictionary name, curly
			 * braces, keyword markers, semicolon terminators, or nested-block
			 * structure are invalid.
			 * \note Multiline keyword values are joined until the next `@` marker.
			 */
			void Analyze();

			/**
			 * \brief Moves parsed keyword data into a dictionary object.
			 *
			 * \param[out] dictionary Destination dictionary receiving name,
			 * source-file name, keywords, options, and source line ranges.
			 * \warning Throws `std::runtime_error` if duplicate keywords are found
			 * in this dictionary block.
			 */
			void Transfer(Dictionary& dictionary);

			/**
			 * \brief Writes a diagnostic representation of this parsed block.
			 *
			 * \param[out] fout Stream receiving compacted dictionary data.
			 */
			void Write(std::ostream& fout) const;

		private:
			/** \brief Parsed dictionary name. */
			std::string name_;

			/** \brief Source file name used in diagnostics. */
			std::string file_name_;

			/** \brief 1-based original line index of the dictionary declaration. */
			unsigned int index_first_line_;

			/** \brief Preprocessed physical lines belonging to this block. */
			std::vector<std::string> lines_;

			/** \brief Nonblank lines between the opening and closing braces. */
			std::vector<std::string> good_lines_;

			/** \brief One compacted logical keyword entry per `@` marker. */
			std::vector<std::string> extended_lines_;

			/** \brief 1-based original line indices for `good_lines_`. */
			std::vector<unsigned int> indices_of_good_lines_;

			/** \brief Mapping from compacted entries to local `good_lines_` offsets. */
			std::vector<unsigned int> i_start_;

			/**
			 * \brief Raises a dictionary-block parsing error.
			 *
			 * \param[in] message Diagnostic message without block context.
			 * \warning Always throws `std::runtime_error`.
			 */
			void ErrorMessage(const std::string message) const;
	};
}

#endif // OpenSMOKEpp_DictionaryFile_H
