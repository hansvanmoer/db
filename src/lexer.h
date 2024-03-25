/* 
 * This file is part of DB.
 * DB is free software: you can redistribute it and/or modify it under the terms of 
 * the GNU General Public License as published by the Free Software Foundation, 
 * either version 3 of the License, or (at your option) any later version.
 * DB is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with DB. 
 * If not, see <https://www.gnu.org/licenses/>. 
 */

#ifndef LEXER_H
#define LEXER_H

#include <stdlib.h>

/**
 * The type of a lexer token
 */
enum lexer_token_type {
  /**
   * The select keyword
   */
  LEXER_TOKEN_TYPE_SELECT,

  /**
   * The from keyword
   */
  LEXER_TOKEN_TYPE_FROM,

  /**
   * An identifier
   */
  LEXER_TOKEN_TYPE_IDENTIFIER,

  /**
   * The where keyword
   */
  LEXER_TOKEN_TYPE_WHERE,

  /**
   * The equals operator
   */
  LEXER_TOKEN_TYPE_EQUALS,

  /**
   * A string literal
   */
  LEXER_TOKEN_TYPE_STRING_LITERAL
};

/**
 * A lexer token
 */
struct lexer_token {

  /**
   * The type of token
   */
  enum lexer_token_type type;

  /**
   * The 0 terminated text buffer
   */
  const char * text;

  /**
   * The length of the text
   */
  size_t len;
};



#endif
