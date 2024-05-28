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
 * A regex state
 */
struct regex_state {
  /**
   * The inclusive lower bound for a match
   */
  char lower;
  /**
   * The exclusive upper bound for a match
   */
  char upper;

  /**
   * If it's a match, transition to this state
   * If there is no transition, set to zero
   */
  size_t then;

  /**
   * If no match, transition to this state
   * If there is no second transition, set to the same value as the first transition
   */
  size_t otherwise;

  /**
   * If this is an end state, this represents an index into the symbol table
   * If not a valid end state, set to -1;
   */
  int end;
};

/**
 * A nfa representing a regex
 */
struct regex_nfa {
  /**
   * The state buffer
   */
  struct regex_state * states;

  /**
   * The length of the state buffer
   */
  size_t len;

  /**
   * The current size of the state buffer
   */
  size_t size;

  /**
   * The symbol table
   */
  const char ** symbols;

  /**
   * The number of symbols
   */
  size_t symbols_len;
};

/**
 * A regex matcher
 */
struct regex_matcher {
  /**
   * The state machine representing the regex
   */
  const struct regex_nfa * nfa;

  /**
   * The stack buffer
   */
  size_t * stack;

  /**
   * The size of the stack buffer
   */
  size_t size;
  
  /**
   * The length of the match
   * If no match was found, len is set to 0
   */
  size_t len;

  /**
   * An index into the symbol table representing the symbol found
   * If no symbol is found, this value is undefined
   */
  size_t symbol;
};

/**
 * Parses a regex state machine from a symbol file
 * \param file the symbol file
 * \param nfa a pointer to the state machine
 * \return 0 on success, -1 on failure
 */
int parse_regex_nfa(FILE * file, struct regex_nfa * nfa);

/**
 * Destroys a regex state machine
 * \param nfa a pointer to the state machine
 */
void dispose_regex_nfa(struct regex_nfa * nfa);

/**
 * Initializes a matcher
 * \param m the matcher
 * \param nfa the regex state machine
 * \return 0 on success, -1 on failure 
 */
int init_regex_matcher(struct regex_matcher * m, const struct regex_nfa * nfa);

/**
 * Maches an input string with a regex
 * Match length and symbol is stored on the matcher on success
 * \param m the matcher
 * \param input the input string
 * \return 0 on success, -1 when no match was found
 */
int match_regex(struct regex_matcher * m, const char * input);

/**
 * Resets a matcher
 * \param m the matcher
 */
void reset_regex_matcher(struct regex_matcher * m);

/**
 * Disposes of a matcher, but does not destroy the underlying state machine
 * \param m the matcher
 */
void dispose_regex_matcher(struct regex_matcher * m);

#endif
