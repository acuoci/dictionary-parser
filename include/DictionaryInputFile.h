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
#define	OpenSMOKEpp_DictionaryInputFile_H

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace OpenSMOKEpp
{
	//!  A class for storing input files in DICTIONARY format
	/*!
			This class provides the tools to read and manage the files containing the definition of one
			or more dictionaries used by the different OpenSMOKE++ applications
	*/

	class DictionaryInputFile {
	public:

		/**
		*@brief Constructor: default
		*/
		DictionaryInputFile();

		/**
		*@brief Constructor: from a file
		*/
		DictionaryInputFile(const std::string& file_name);

		/**
		*@brief Default copy constructor
		*/
		DictionaryInputFile(const DictionaryInputFile& orig) = default;

		/**
		*@brief Default destructor
		*/
		virtual ~DictionaryInputFile() = default;

	public:
    
		/**
		*@brief Returns the number of lines in the file
		*/
		[[nodiscard]] int number_of_lines() const noexcept { return number_of_lines_; }

		/**
		*@brief Returns the number of blank lines in the file
		*/
		[[nodiscard]] int number_of_blank_lines() const noexcept { return number_of_blank_lines_; }

		/**
		*@brief Returns the number of useful linse in the file (i.e. lines which
				have to be interpreted). Of course blank lines and comment lines are
				automatically excluded from this list
		*/
		[[nodiscard]] int number_of_good_lines() const noexcept { return number_of_good_lines_; }
    
		/**
		*@brief Returns the indices of good lines (i.e. lines to be interpreted)
		*/
		[[nodiscard]] const std::vector<int>& indices_of_good_lines() const noexcept { return indices_of_good_lines_; }

		/**
		*@brief Returns the indices of blanck lines
		*/
		[[nodiscard]] const std::vector<int>& indices_of_blank_lines() const noexcept { return indices_of_blank_lines_; }
    
		/**
		*@brief Returns the good lines (i.e. lines to be interpreted)
		*/
		[[nodiscard]] const std::vector<std::string>& good_lines() const noexcept { return good_lines_; }

		/**
		*@brief Returns the clean lines (without comments, without tab, etc)
		*/
		[[nodiscard]] const std::vector<std::string>& clean_lines() const noexcept { return clean_lines_; }
		[[nodiscard]] std::vector<std::string>& clean_lines() noexcept { return clean_lines_; }

		/**
		*@brief Returns the blank lines (i.e. lines to be interpreted)
		*/
		[[nodiscard]] const std::vector<std::string>& blank_lines() const noexcept { return blank_lines_; }
    
		/**
		*@brief Returns the name of the file
		*/
		[[nodiscard]] std::filesystem::path file_name() const { return file_name_.filename(); }

		/**
		*@brief Returns the path of the file (without the name of the file)
		*/
		[[nodiscard]] std::filesystem::path folder_path() const { return file_name_.parent_path(); }

		/**
		*@brief Returns the size of the file in bytes
		*/
		[[nodiscard]] std::uintmax_t size() const { return std::filesystem::file_size(file_name_); }

		/**
		*@brief Writes information about the status of the file on output stream
		*/
		void Status(std::ostream &fOut) const;
    
	private:
    
		int number_of_lines_ = 0;						//!< total number of lines
		int number_of_good_lines_ = 0;					//!< total number of good lines
		int number_of_blank_lines_ = 0;					//!< total number of blank lines
		std::vector<int> indices_of_good_lines_;		//!< indices of good lines
		std::vector<int> indices_of_blank_lines_;		//!< indices of blank lines
		std::vector<std::string> good_lines_;			//!< good lines
		std::vector<std::string> blank_lines_;			//!< blank lines
		std::vector<std::string> clean_lines_;			//!< clean lines
    
		std::filesystem::path file_name_;			//!< full name (path + file name)
	};
}

#endif	/* OpenSMOKEpp_DictionaryInputFile_H */

