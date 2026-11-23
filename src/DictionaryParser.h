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

#ifndef OpenSMOKEpp_DictionaryParser_H
#define OpenSMOKEpp_DictionaryParser_H

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "DictionaryDiagnostics.h"
#include "DictionaryLexer.h"

namespace OpenSMOKEpp::detail {
/**
 * \brief One `@Keyword values... ;` entry produced by the parser.
 */
struct ParsedEntry {
  /** \brief Keyword name including the leading `@`. */
  std::string keyword;
  /** \brief Value tokens (unquoted text). */
  std::vector<std::string> values;
  /** \brief Value tokens with their positions (same order as `values`). */
  std::vector<Lexer::Token> tokens;
  /** \brief Value tokens as written, joined by single blanks. */
  std::string option;
  /** \brief 1-based line of the keyword. */
  unsigned int first_line = 0;
  /** \brief 1-based line of the terminating `;` (or closing `}`). */
  unsigned int last_line = 0;
  /** \brief 1-based column of the keyword. */
  unsigned int column = 0;
};

/**
 * \brief One dictionary block (top-level or inline) produced by the parser.
 */
struct ParsedDictionary {
  /** \brief Dictionary name; inline blocks are named `parent/Keyword`. */
  std::string name;
  /** \brief 1-based line of the `Dictionary` header (or inline keyword). */
  unsigned int line = 0;
  /** \brief 1-based column of the dictionary name (or inline keyword). */
  unsigned int column = 0;
  /** \brief Length of the highlighted name. */
  unsigned int length = 0;
  /** \brief Entries in source order. */
  std::vector<ParsedEntry> entries;
};

/**
 * \brief One `#include "file"` directive.
 */
struct IncludeDirective {
  /** \brief Path exactly as written (unresolved). */
  std::string path;
  /** \brief 1-based line of the directive. */
  unsigned int line = 0;
  /** \brief 1-based column of the directive. */
  unsigned int column = 0;
};

/**
 * \brief Result of parsing one source buffer.
 */
struct ParsedSource {
  /** \brief All dictionaries, inline ones included. */
  std::vector<ParsedDictionary> dictionaries;
  /** \brief Include directives in source order (not yet resolved). */
  std::vector<IncludeDirective> includes;
};

/**
 * \brief Parses dictionary source text.
 *
 * Grammar (token level, see `Lexer::tokenize`):
 * \code
 * source     := ( include | dictionary )*
 * include    := "#include" PATH [";"]
 * dictionary := "Dictionary" NAME block
 * block      := "{" entry+ "}"
 * entry      := KEYWORD value* ";"
 *             | KEYWORD block [";"]          // inline dictionary
 * \endcode
 * An inline block attached to keyword `@Key` inside dictionary `parent`
 * becomes a dictionary named `parent/Key`, and the entry value is that
 * name, so it is read like any reference to a dictionary.
 *
 * \param[in] source Shared source file (name and text).
 * \return Parsed dictionaries and include directives.
 * \warning Throws `Lexer::SyntaxError` on any lexical or syntax error.
 */
[[nodiscard]] auto parse_source(const std::shared_ptr<const SourceFile> &source)
    -> ParsedSource;
} // namespace OpenSMOKEpp::detail

#endif // OpenSMOKEpp_DictionaryParser_H
