/*-----------------------------------------------------------------------*\
|    ___                   ____  __  __  ___  _  _______                  |
|   / _ \ _ __   ___ _ __ / ___||  \/  |/ _ \| |/ / ____| _     _         |
|  | | | | '_ \ / _ \ '_ \\___ \| |\/| | | | | ' /|  _| |_ _| |_       |
|  | |_| | |_) |  __/ | | |___) | |  | | |_| | . \| |__|_   _|_   _      |
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

#ifndef OpenSMOKEpp_DictionaryManager_H
#define	OpenSMOKEpp_DictionaryManager_H

#include <map>
#include <string>
#include <vector>

#include "DictionaryInputFile.h"
#include "Dictionary.h"

namespace OpenSMOKEpp
{
	/**
	 * \brief Owns all dictionaries parsed from one or more input files.
	 *
	 * The manager parses top-level `Dictionary <name> { ... }` blocks and
	 * stores each block in a `std::map` keyed by dictionary name. It performs
	 * syntax-level parsing only; semantic grammar validation is performed on a
	 * retrieved `Dictionary`.
	 */
	class DictionaryManager
	{
	public:
		/**
		 * \brief Reads all dictionary blocks from a file.
		 *
		 * \param[in] file_name Path to an input file using dictionary syntax.
		 * \warning Throws `std::runtime_error` if the file cannot be read, if
		 * no dictionary blocks are present, or if a contained dictionary block is
		 * syntactically malformed.
		 * \note Duplicate dictionary names are reported to the standard output by
		 * the current implementation and are not inserted a second time.
		 */
		void ReadDictionariesFromFile(const std::string file_name);

		/**
		 * \brief Returns a parsed dictionary by name.
		 *
		 * \param[in] name Dictionary name exactly as declared after the
		 * `Dictionary` token in the input file.
		 * \return Mutable reference to the stored dictionary.
		 * \warning Throws `std::runtime_error` if no dictionary with `name` was
		 * loaded.
		 */
		Dictionary& operator() (const std::string& name);

	private:
		/** \brief Dictionaries indexed by their declared names. */
		std::map<std::string, Dictionary> map_of_dictionaries_;

		/**
		 * \brief Raises a manager-scoped parsing or lookup error.
		 *
		 * \param[in] message Diagnostic message without class prefix.
		 * \warning Always throws `std::runtime_error`.
		 */
		void ErrorMessage(const std::string message) const;
	};
}

#endif // OpenSMOKEpp_DictionaryManager_H
