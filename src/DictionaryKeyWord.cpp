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

#include "DictionaryKeyWord.h"
#include "DictionaryLexer.h"

#include <algorithm>
#include <ostream>
#include <stdexcept>

namespace OpenSMOKEpp
{
	void DictionaryKeyWord::ErrorMessage(const std::string message) const
	{
		throw std::runtime_error("Keyword " + name_ + ": " + message);
	}
	
	DictionaryKeyWord::DictionaryKeyWord(const std::string& name, const DictionaryKeyWordTypes type, 
															 const std::string& short_comment, const bool is_compulsory) :
		name_(name), 
		type_(type),
		comment_short_(short_comment),
		is_compulsory_(is_compulsory)
	{
		SetTypeASCII();
	}
	
	DictionaryKeyWord::DictionaryKeyWord(const std::string& name, const DictionaryKeyWordTypes type, 
															 const std::string& short_comment, const bool is_compulsory, 
															 const std::string& compulsory_alternatives, const std::string& required_keywords, 
															 const std::string& conflicting_keywords) :
		name_(name), 
		type_(type),
		comment_short_(short_comment),
		is_compulsory_(is_compulsory)
	{
		
		std::string line1 = "compulsory_alternatives: " + compulsory_alternatives;
		SetCompulsoryAlternatives(line1);

		std::string line2 = "required_keywords: " + required_keywords;
		SetRequiredKeyWords(line2);

		std::string line3 = "conflicting_keywords: " + conflicting_keywords;
		SetConflictingKeywords(line3);

		SetTypeASCII();
	}

	void DictionaryKeyWord::SetTypeASCII()
	{
		     if (type_ == NONE )					type_ascii_ = "none";
		else if (type_ == SINGLE_INT )				type_ascii_ = "single_int";
		else if (type_ == SINGLE_DOUBLE )			type_ascii_ = "single_double";
		else if (type_ == SINGLE_CHAR )				type_ascii_ = "single_char";
		else if (type_ == SINGLE_STRING )			type_ascii_ = "single_string";
		else if (type_ == SINGLE_BOOL )				type_ascii_ = "single_bool";
		else if (type_ == SINGLE_MEASURE )			type_ascii_ = "single_measure";
		else if (type_ == SINGLE_PATH )				type_ascii_ = "single_path";
		else if (type_ == SINGLE_DICTIONARY )		type_ascii_ = "single_dictionary";
		else if (type_ == VECTOR_INT )				type_ascii_ = "vector_int";
		else if (type_ == VECTOR_DOUBLE )			type_ascii_ = "vector_double";
		else if (type_ == VECTOR_CHAR )				type_ascii_ = "vector_char";
		else if (type_ == VECTOR_STRING )			type_ascii_ = "vector_string";
		else if (type_ == VECTOR_BOOL )				type_ascii_ = "vector_bool";
		else if (type_ == VECTOR_MEASURE )			type_ascii_ = "vector_measure";
		else if (type_ == VECTOR_STRING_DOUBLE)		type_ascii_ = "vector_string_double";
		else if (type_ == VECTOR_INT_STRING)		type_ascii_ = "vector_int_string";
		else if (type_ == SEQUENCE_STRING )			type_ascii_ = "sequence_string";
		else ErrorMessage("The keyword type is not specified correctly.");
	}

	void DictionaryKeyWord::SetKeyword(const std::string& line)
	{
		const auto tokens = Lexer::split_whitespace(line);

		if (tokens.size() != 2 || tokens[0] != "keyword:")
				ErrorMessage("The keyword name is not specified correctly.");
		name_ = std::string(tokens[1]);
	}

	void DictionaryKeyWord::SetType(const std::string& line)
	{
		const auto tokens = Lexer::split_whitespace(line);

		if (tokens.size() != 2 || tokens[0] != "type:")
				ErrorMessage("The keyword type is not specified correctly.");
		if (tokens[1] == "none")						type_ = NONE; 
		else if (tokens[1] == "single_int")				type_ = SINGLE_INT;
		else if (tokens[1] == "single_double")			type_ = SINGLE_DOUBLE;
		else if (tokens[1] == "single_string")			type_ = SINGLE_STRING;
		else if (tokens[1] == "single_char")			type_ = SINGLE_CHAR;
		else if (tokens[1] == "single_bool")			type_ = SINGLE_BOOL;
		else if (tokens[1] == "single_measure")			type_ = SINGLE_MEASURE;
		else if (tokens[1] == "single_path")			type_ = SINGLE_PATH;
		else if (tokens[1] == "single_dictionary")		type_ = SINGLE_DICTIONARY;
		else if (tokens[1] == "vector_int")				type_ = VECTOR_INT;
		else if (tokens[1] == "vector_double")			type_ = VECTOR_DOUBLE;
		else if (tokens[1] == "vector_string")			type_ = VECTOR_STRING;
		else if (tokens[1] == "vector_char")			type_ = VECTOR_CHAR;
		else if (tokens[1] == "vector_bool")			type_ = VECTOR_BOOL;
		else if (tokens[1] == "vector_measure")			type_ = VECTOR_MEASURE;
		else if (tokens[1] == "vector_string_double")	type_ = VECTOR_STRING_DOUBLE;
		else if (tokens[1] == "vector_int_string")		type_ = VECTOR_INT_STRING;
		else if (tokens[1] == "sequence_string")	    type_ = SEQUENCE_STRING;
		else ErrorMessage("The keyword type is not specified correctly.");

		type_ascii_ = std::string(tokens[1]);
	}

	void DictionaryKeyWord::SetComment(const std::string& line)
	{
		const auto tokens = Lexer::split_whitespace(line);

		if (tokens.empty() || tokens[0] != "short_comment:")
				ErrorMessage("The keyword short comment is not specified correctly.");

		comment_short_.clear();
		for (std::size_t i = 1; i < tokens.size(); ++i)
			comment_short_ += std::string(tokens[i]) + " ";
	}

	void DictionaryKeyWord::SetIsCompulsory(const std::string& line)
	{
		const auto tokens = Lexer::split_whitespace(line);

		if (tokens.size() != 2 || tokens[0] != "is_compulsory:")
				ErrorMessage("The keyword is_compulsory is not specified correctly.");
		if (tokens[1] == "true")	is_compulsory_ = true;
		else if (tokens[1] == "false")	is_compulsory_ = false;
		else ErrorMessage("The keyword is_compulsory is not specified correctly.");
	}

	void DictionaryKeyWord::SetCompulsoryAlternatives(const std::string& line)
	{
		const auto tokens = Lexer::split_whitespace(line);

		if (tokens.empty() || tokens[0] != "compulsory_alternatives:")
				ErrorMessage("The keyword compulsory_alternatives are not specified correctly.");

		compulsory_alternatives_.clear();
		if (tokens.size() == 2 && tokens[1] == "none")
			return;

		if (std::find(tokens.begin() + 1, tokens.end(), "none") != tokens.end())
			ErrorMessage("The keyword compulsory_alternatives are not specified correctly.");

		if (is_compulsory_ == false && tokens.size() > 1)
			ErrorMessage("The keyword compulsory_alternatives are not specified correctly.");

		compulsory_alternatives_.reserve(tokens.size() > 0 ? tokens.size() - 1 : 0);
		for (std::size_t i = 1; i < tokens.size(); ++i)
			compulsory_alternatives_.push_back(std::string(tokens[i]));
	}

	void DictionaryKeyWord::SetRequiredKeyWords(const std::string& line)
	{
		const auto tokens = Lexer::split_whitespace(line);

		if (tokens.empty() || tokens[0] != "required_keywords:")
				ErrorMessage("The keyword required_keywords are not specified correctly.");

		required_keywords_.clear();
		if (tokens.size() == 2 && tokens[1] == "none")
			return;

		if (std::find(tokens.begin() + 1, tokens.end(), "none") != tokens.end())
			ErrorMessage("The keyword required_keywords are not specified correctly.");

		required_keywords_.reserve(tokens.size() > 0 ? tokens.size() - 1 : 0);
		for (std::size_t i = 1; i < tokens.size(); ++i)
			required_keywords_.push_back(std::string(tokens[i]));
	}

	void DictionaryKeyWord::SetConflictingKeywords(const std::string& line)
	{
		const auto tokens = Lexer::split_whitespace(line);

		if (tokens.empty() || tokens[0] != "conflicting_keywords:")
				ErrorMessage("The keyword conflicting_keywords are not specified correctly.");

		conflicting_keywords_.clear();
		if (tokens.size() == 2 && tokens[1] == "none")
			return;

		if (std::find(tokens.begin() + 1, tokens.end(), "none") != tokens.end())
			ErrorMessage("The keyword conflicting_keywords are not specified correctly.");

		conflicting_keywords_.reserve(tokens.size() > 0 ? tokens.size() - 1 : 0);
		for (std::size_t i = 1; i < tokens.size(); ++i)
			conflicting_keywords_.push_back(std::string(tokens[i]));
	}

	DictionaryKeyWord::DictionaryKeyWord(std::vector<std::string>& lines)
	{
		if (lines.size() != 7)
			ErrorMessage("Wrong numer of lines defining the keyword");

		for (auto& line : lines)
			line = Lexer::trim_copy(line);

		SetKeyword(lines[0]);
		SetType(lines[1]);
		SetComment(lines[2]);
		SetIsCompulsory(lines[3]);
		SetCompulsoryAlternatives(lines[4]);
		SetRequiredKeyWords(lines[5]);
		SetConflictingKeywords(lines[6]);
	}

	void DictionaryKeyWord::ShortSummary(std::ostream& fout) const
	{
		fout << name_ << " (" << type_ascii_ << ")" << std::endl;
		fout << " * " << comment_short_ << std::endl;
	}

}
