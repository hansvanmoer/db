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

#ifndef REGEX_H
#define REGEX_H

#include "regex_nfa.h"

#include <stdbool.h>
#include <stdio.h>

#define MAX_REGEX_SYMBOL_NAME_LENGTH 128

/**
 * The type of regex node
 */
enum regex_type {
  /**
   * A sequence of nodes
   */
  REGEX_TYPE_SEQUENCE,

  /**
   * A branch
   */
  REGEX_TYPE_BRANCH,

  /**
   * A range of characters
   */
  REGEX_TYPE_RANGE,

  /**
   * A loop
   */
  REGEX_TYPE_LOOP,

  /**
   * A reference to a symbol
   */
  REGEX_TYPE_REFERENCE
};

/**
 * Node children
 */
struct regex_children {
  /**
   * The left branch
   */
  struct regex_node * left;

  /**
   * The right branch
   */
  struct regex_node * right;
};

/**
 * A range of characters
 */
struct regex_range {
  /**
   * The first character code point
   */
  int start;

  /**
   * The last character code point
   */
  int end;
};

/**
 * A loop
 */
struct regex_loop {
  /**
   * The node to be repeated
   */
  struct regex_node * body;
};

struct regex_symbol;

/**
 * A reference to a symbol
 */
struct regex_reference {
  /**
   * The symbol
   */
  struct regex_symbol * symbol;
};

/**
 * The data of a regex node
 */
union regex_data {
  struct regex_children children;
  struct regex_loop loop;
  struct regex_range range;
  struct regex_reference reference;
};

/**
 * A regex node
 */
struct regex_node {
  /**
   * The type of node
   */
  enum regex_type type;

  /**
   * The data of the node
   */
  union regex_data data;

  /**
   * A pointer to the parent node
   */
  struct regex_node * parent;
};

/**
 * A symbol
 */
struct regex_symbol { 
  /**
   * The name
   */
  char name[MAX_REGEX_SYMBOL_NAME_LENGTH];

  /**
   * Whether this symbol is a lexeme
   */
  bool lexeme;
  
  /**
   * The root node of the symbol
   */
  struct regex_node * expression;

  /**
   * A link to the previous symbol
   */
  struct regex_symbol * prev;

  /**
   * A link to the next symbol
   */
  struct regex_symbol * next;
};

/**
 * A set of symbols
 * TODO: add a hash map
 */
struct regex_symbols {
  /**
   * The first symbol
   */
  struct regex_symbol * head;

  /**
   * The last symbol
   */
  struct regex_symbol * tail;
};

/**
 * Parses a symbol file
 * \param file the file to parse
 * \return the symbols or NULL if an error occurs
 */
struct regex_symbols * parse_regex_symbols(FILE * file);

/**
 * Destroys a set of symbols
 * \param symbols the symbols
 */
void destroy_regex_symbols(struct regex_symbols * symbols);

/**
 * A regex
 */
struct regex {
  /**
   * The symbols
   */
  struct regex_symbol * symbols;

  /**
   * The number of symbols
   */
  size_t len;

  /**
   * The non deterministic automaton
   */
  struct regex_nfa nfa;
};

/**
 * Creates a regex from the specified file
 * \param regex the regex
 * \param file the input file
 * \return 0 on success, -1 on failure
 */
int init_regex_from_file(struct regex * regex, FILE * file);

/**
 * Disposes of all the resources associated with this regex
 * \param regex the regex
 */
void dispose_regex(struct regex * regex);

#endif
