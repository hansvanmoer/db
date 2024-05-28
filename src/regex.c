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

#include "logger.h"
#include "regex.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REGEX_PARSER_COMMENT '#'
#define REGEX_PARSER_LEXEME '@'
#define REGEX_PARSER_BRANCH_SEPARATOR '|'
#define REGEX_PARSER_GROUP_START '('
#define REGEX_PARSER_GROUP_END ')'
#define REGEX_PARSER_STATEMENT_END ';'
#define REGEX_PARSER_LITERAL '\"'
#define REGEX_PARSER_ESCAPE '\\'
#define REGEX_PARSER_LOOP '*'
#define REGEX_PARSER_REFERENCE_PREFIX '$' 
#define REGEX_PARSER_RANGE_START '['
#define REGEX_PARSER_RANGE_SEPARATOR '-'
#define REGEX_PARSER_RANGE_END ']'

#define MAX_REGEX_PARSER_REFERENCE_DEPTH 256

/**
 * The regex parser
 */
struct regex_parser {
  /**
   * The buffer
   */
  char * buf;

  /**
   * The position of the parser
   */
  size_t pos;

  /**
   * The length of the buffer
   */
  size_t len;

  /**
   * Parser error, if any
   */
  const char * error;

  /**
   * Line number
   */
  int line;

  /**
   * Column number
   */
  int col;

  /**
   * Symbols
   */
  struct regex_symbols * symbols;
};

/**
 * The current (in progress) branch of the regex tree
 */
struct regex_tree {
  /**
   * The root of the branch
   */
  struct regex_node * root;

  /**
   * The current leaf node
   */
  struct regex_node * leaf;
};


/**
 * Copies a string from the parser buffer
 * \param dest the destination buffer
 * \param parser the parser
 * \param pos the start position
 * \param len the string length
 */
static void parser_copy_string(char * dest, struct regex_parser * parser, size_t pos, size_t len) {
  assert(dest != NULL);
  assert(parser != NULL);

  memcpy(dest, parser->buf + pos, len);
  dest[len] = '\0';
}

/**
 * Reads a regex file
 * \param parser the parser
 * \param file the symbol file
 * \return 0 on success, -1 on error
 */
static int read_regex_file(struct regex_parser * parser, FILE * file) {
  assert(parser != NULL);
  assert(file != NULL);
  
  if(fseek(file, 0, SEEK_END) != 0) {
    LOG_ERROR("could not seek the end of the symbol file");
    return -1;
  }
  long end = ftell(file);
  if(end < 0) {
    LOG_ERROR("could not tell end of the symbol file");
    return -1;
  }
  if(fseek(file, 0, SEEK_SET) != 0) {
    LOG_ERROR("could not rewind to the start of the symbol file");
    return -1;
  }
  size_t len = (size_t) end;
  char * buf = (char *) malloc(len);
  LOG_DEBUG("read %d characters", end);
  if(buf == NULL) {
    LOG_ERROR("unable to allocate the symbol input buffer");
    return -1;
  }
  
  size_t read = fread(buf, 1, len, file);
  if(read != len) {
    LOG_ERROR("unable to read symbol input buffer");
    free(buf);
    return -1;
  }
  
  parser->pos = 0;
  parser->len = len;
  parser->buf = buf;
  parser->error = NULL;
  parser->line = 1;
  parser->col = 1;
  parser->symbols = NULL;
  return 0;
}

/**
 * Disposes the regex parser, freeing the underlying buffer
 * \param parser the parser
 */
static void dispose_regex_parser(struct regex_parser * parser) {
  assert(parser != NULL);

  free(parser->buf);
  if(parser->symbols != NULL) {
    destroy_regex_symbols(parser->symbols);
  }
}


/**
 * Whether the parser has not encountered an error
 * \param parser the parser
 * \return true if everything is OK, false if an error has occurred
 */
static bool parser_ok(struct regex_parser * parser) {
  assert(parser != NULL);

  return parser->error == NULL;
}

/**
 * Sets the parser error
 * \param parser the parser
 * \param msg the error message
 */
static void parser_error(struct regex_parser * parser, const char * msg) {
  assert(parser != NULL);

  parser->error = msg;
}

/**
 * Checks whether the parser has more characters
 * \param parser the parser
 * \return true if more characters are present, false otherwise
 */
static bool parser_has_more(struct regex_parser * parser) {
  assert(parser != NULL);

  return parser->pos != parser->len;
}

/**
 * Whether a character is a whitespace character
 * \param c the character
 * \return true if the character is whitespace, false otherwise
 */
static bool is_whitespace(char c) {
  return c == ' ' || c == '\t' || c == '\n';
}

/**
 * Whether a character is a newline character
 * \param c the character
 * \return true if the character is a newline, false otherwise
 */
static bool is_newline(char c) {
  return c == '\n';
}

/**
 * Whether a character is an identifier character
 * \param c the character
 * \return true if the character is an identifier character, false otherwise
 */
static bool is_identifier(char c) {
  return isalpha(c) || isdigit(c) || c == '_';
}

/**
 * Whether a character is a literal delimiter
 * \param c the character
 * \return true if the character is a literal delimiter, false otherwise
 */
static bool is_literal(char c) {
  return c == REGEX_PARSER_LITERAL;
}

/**
 * Returns the current line of the parser
 * \param parser the parser
 * \return the line
 */
static int parser_line(struct regex_parser * parser) {
  assert(parser != NULL);
  return parser->line;
}

/**
 * Returns the current column of the parser
 * \param parser the parser
 * \return the column
 */
static int parser_column(struct regex_parser * parser) {
  assert(parser != NULL);
  return parser->col;
}

/**
 * Prints a debug log
 * \param parser the parser
 * \param msg the message
 */
static void parser_debug_log(struct regex_parser * parser, const char * msg) {
  assert(parser != NULL);
  assert(msg != NULL);

  LOG_DEBUG("%s at %d:%d", msg, parser_line(parser), parser_column(parser));
}

/**
 * Prints an error log
 * \param parser the parser
 */
static void parser_error_log(struct regex_parser * parser) {
  assert(parser != NULL);
 
  if(parser->error != NULL) {
    LOG_ERROR("%s at %d:%d", parser->error, parser_line(parser), parser_column(parser));
  }
}

/**
 * Peeks at the first character in the parser
 * \param parser the parser
 * \return the character
 */
static char parser_peek(struct regex_parser * parser) {
  assert(parser != NULL);
  assert(parser->pos != parser->len);  

  return parser->buf[parser->pos];
}

/**
 * Skips the next character in the parser
 * \param parser the parser
 */
static void parser_skip(struct regex_parser * parser) {
  assert(parser != NULL);
  assert(parser->pos != parser->len);
  if(is_newline(parser->buf[parser->pos])) {
    ++parser->line;
    parser->col = 1;
  } else {
    ++parser->col;
  }
  
  ++parser->pos;
}

/**
 * Skips the parser while the predicate holds
 * \param parser the parser
 * \param pred the predicate
 */
static void parser_skip_while(struct regex_parser * parser, bool (*pred)(char c)) {
  assert(parser != NULL);
  assert(pred != NULL);

  while(parser_has_more(parser)) {
    if(!(*pred)(parser_peek(parser))) {
      break;
    }
    parser_skip(parser);
  }
}

/**
 * Skips the parser while the predicate holds
 * \param parser the parser
 * \param pred the predicate
 */
static void parser_skip_until(struct regex_parser * parser, bool (*pred)(char c)) {
  assert(parser != NULL);
  assert(pred != NULL);

  while(parser_has_more(parser)) {
    if((*pred)(parser_peek(parser))) {
      break;
    }
    parser_skip(parser);
  }
}

/**
 * Skips the parser until the predicate holds while accommodating an escape char
 * \param parser the parser
 * \param pred the predicate
 */
static void parser_skip_until_with_escape(struct regex_parser * parser, bool (*pred)(char c)) {
  assert(parser != NULL);
  assert(pred != NULL);

  bool escaped = false;
  while(parser_has_more(parser)) {
    char c = parser_peek(parser);
    if(c == REGEX_PARSER_ESCAPE) {
      escaped = true;
    } else {
      if(!escaped && (*pred)(c)) {
	break;
      }
      escaped = false;
    }
    parser_skip(parser);
  }
}

/**
 * Skips the parser over the next bit of whitespace
 * \param parser the parser
 */
static void parser_skip_whitespace(struct regex_parser * parser) {
  assert(parser != NULL);

  parser_skip_while(parser, is_whitespace);
}

/**
 * Skips the parser over the comment
 * \param parser the parser
 */
static void parser_skip_comment(struct regex_parser * parser) {
  assert(parser != NULL);
  parser_debug_log(parser, "COMMENT");
  parser_skip(parser);
  parser_skip_until(parser, is_newline);
  if(parser_has_more(parser)) {
    parser_skip(parser);
  }
}

/**
 * Returns the parser position
 * \param parser the parser
 * \return the position
 */
static size_t parser_pos(struct regex_parser * parser) {
  assert(parser != NULL);

  return parser->pos;
}

/**
 * Returns a pointer to the underlying buffer at the requested location
 * \param parser the parser
 * \return a pointer to the buffer
 */
static const char * parser_at(struct regex_parser * parser, size_t pos) {
  assert(parser != NULL);
  return parser->buf + pos;
}

/**
 * Adds a regex symbol to the symbol set
 * \param symbols the symbol set
 * \param symbol the symbol
 */
static void add_regex_symbol(struct regex_symbols * symbols, struct regex_symbol * symbol) {
  assert(symbols != NULL);
  assert(symbol != NULL);

  if(symbols->tail == NULL) {
    symbols->head = symbol;
  } else {
    symbols->tail->next = symbol;
  }
  symbol->prev = symbols->tail;
  symbol->next = NULL;
  symbols->tail = symbol;
}

/**
 * Creates a regex node without setting the node data
 * \param type the node type
 * \return the node or NULL
 */
static struct regex_node * create_regex_node(enum regex_type type) {
  struct regex_node * node = (struct regex_node *) malloc(sizeof(struct regex_node));
  if(node == NULL) {
    LOG_ERROR("unable to allocate regex node");
    return NULL;
  }
  node->type = type;
  node->parent = NULL;
  return node;
}

/**
 * Node type labels
 */
static const char * node_type_labels[] = {
  "sequence",
  "branch",
  "range",
  "multiplier",
  "reference"
};

/**
 * Debugs a regex node
 * \param node the node
 * \param id the node ID
 */
static int debug_regex_node(struct regex_node * node, int id) {
  LOG_DEBUG("node %d with type %s", id, node_type_labels[node->type]);
  int next_id;
  switch(node->type) {
  case REGEX_TYPE_SEQUENCE:
    LOG_DEBUG("head of node %d:", id);
    next_id = debug_regex_node(node->data.children.left, id + 1);
    LOG_DEBUG("tail of node %d:", id);
    next_id = debug_regex_node(node->data.children.right, next_id);
    LOG_DEBUG("end of children %d", id);
    return next_id;
  case REGEX_TYPE_BRANCH:
    LOG_DEBUG("left branch of node %d:", id);
    next_id = debug_regex_node(node->data.children.left, id + 1);
    LOG_DEBUG("right branch of node %d:", id);
    next_id = debug_regex_node(node->data.children.right, next_id);
    LOG_DEBUG("end of children %d", id);
    return next_id;
  case REGEX_TYPE_LOOP:
    LOG_DEBUG("loop body of node %d:", id);
    next_id = debug_regex_node(node->data.loop.body, id + 1);
    LOG_DEBUG("end of loop body %d", id);
    return next_id;
  case REGEX_TYPE_RANGE:
    LOG_DEBUG("[%d, %d]", node->data.range.start, node->data.range.end);
    return id + 1;
  case REGEX_TYPE_REFERENCE:
    LOG_DEBUG("symbol '%s'", node->data.reference.symbol->name);
    return id + 1;
  default:
    LOG_ERROR("unknown node type");
    return id + 1;
  }
}

/**
 * Destroys a regex node
 * \param node the regex node
 */
static void destroy_regex_node(struct regex_node * node) {
  assert(node != NULL);
  if(node->type == REGEX_TYPE_SEQUENCE || node->type == REGEX_TYPE_BRANCH) {
    destroy_regex_node(node->data.children.left);
    destroy_regex_node(node->data.children.right);
  } else if(node->type == REGEX_TYPE_LOOP) {
    destroy_regex_node(node->data.loop.body);
  }
  free(node);
}

/**
 * Adds a node to a sequence or branch
 * \param root a pointer to the root pointer
 * \param parent a pointer to the parent pointer
 * \param node the new node that has to be added
 * \param 0 on success, -1 on failure
 */
static int add_to_regex_tree(struct regex_tree * tree, enum regex_type type, struct regex_node * node) {
  assert(tree != NULL);
  assert(node != NULL);
  assert(type == REGEX_TYPE_SEQUENCE || type == REGEX_TYPE_BRANCH);

  if(tree->leaf == NULL) {
    tree->root = node;
    tree->leaf = node;
    return 0;
  } else {
    struct regex_node * seq = create_regex_node(type);
    if(seq == NULL) {
      return -1;
    }
    if(tree->leaf == tree->root) {
      tree->root = seq;
    } else {
      seq->parent = tree->leaf->parent;
      tree->leaf->parent->data.children.right = seq;
    }
    seq->data.children.left = tree->leaf;
    tree->leaf->parent = seq;
    seq->data.children.right = node;
    node->parent = seq;
    tree->leaf = node;
    return 0;
  }
}

/**
 * Destroys all nodes in the regex tree
 * \param tree the tree
 */
static void dispose_regex_tree(struct regex_tree * tree) {
  assert(tree != NULL);
  if(tree->root != NULL) {
    destroy_regex_node(tree->root);
  }
}


/**
 * Destroys a regex symbol
 * \param symbol the symbol
 */
static void destroy_regex_symbol(struct regex_symbol * symbol) {
  assert(symbol != NULL);
}

/**
 * Compares two symbol names
 * \param left the name of a symbol in the symbol table
 * \param right the name of the reference
 * \param len the length of the reference name
 * \return true of reference matches, false otherwise 
 */
static bool regex_symbol_name_eq(const char * left, const char * right, size_t len) {
  size_t i = 0;
  while(*left != '\0' && i != len) {
    if(*left != right[i]) {
      return false;
    }
    ++left;
    ++i;
  }
  return i == len && *left == '\0';
}

/**
 * Gets the symbol with the specified name or creates a new (undefined) one, copying the name
 * \param symbol the symbol table
 * \param name_start the start position of the name
 * \param name_len the length of the name
 * \return an existing symbol, a new symbol or NULL if something went wrong
 */
static struct regex_symbol * get_or_create_regex_symbol(struct regex_parser * parser, struct regex_symbols * symbols, size_t name_start, size_t name_len) {
  assert(symbols != NULL);

  if(name_len + 1 > MAX_REGEX_SYMBOL_NAME_LENGTH) {
    LOG_ERROR("reference name too long");
    parser_error(parser, "reference name too long");
    return NULL;
  }

  const char * name = parser_at(parser, name_start);
  // TODO: replace this with hash map or red-black tree
  struct regex_symbol * symbol = symbols->head;
  while(symbol != NULL) {
    if(regex_symbol_name_eq(symbol->name, name, name_len)) {
      return symbol;
    }
    symbol = symbol->next;
  }  

  symbol = (struct regex_symbol *) malloc(sizeof(struct regex_symbol));
  if(symbol == NULL) {
    LOG_ERROR("could not allocate regex symbol");
    return NULL;
  }
  parser_copy_string(symbol->name, parser, name_start, name_len);
  symbol->lexeme = false;
  symbol->expression = NULL;
  if(symbols->head == NULL) {
    symbol->prev = NULL;
    symbols->head = symbol;
  } else {
    symbols->tail->next = symbol;
    symbol->prev = symbols->tail;
  }
  symbol->next = NULL;
  symbols->tail = symbol;
  return symbol;
}

/**
 * Check whether all regex symbols are defines
 * \param the regex symbols
 * \return 0 if all symbols are defined, -1 if not
 */
static int check_regex_symbols(struct regex_symbols * symbols) {
  struct regex_symbol * symbol = symbols->head;
  while(symbol != NULL) {
    if(symbol->expression == NULL) {
      LOG_ERROR("unefined regex symbol: %s", symbol->name);
      return -1;
    }
    symbol = symbol->next;
  }
  return 0;
}

void destroy_regex_symbols(struct regex_symbols * symbols) {
  assert(symbols != NULL);

  struct regex_symbol * symbol = symbols->head;
  while(symbol != NULL) {
    struct regex_symbol * next = symbol->next;
    destroy_regex_symbol(symbol);
    symbol = next;
  }
}

/**
 * Parses a literal
 * \param parser the parser
 * \return the literal on success, NULL on failure
 */
static struct regex_node * parse_literal(struct regex_parser * parser) {
  assert(parser != NULL);

  struct regex_tree tree = {NULL, NULL};
  
  parser_debug_log(parser, "start of literal");
  parser_skip(parser);
  bool escaped = false;
  while(true) {
    if(!parser_has_more(parser)) {
      parser_error(parser, "expected literal delimiter");
      dispose_regex_tree(&tree);
      return NULL;
    }
    char c = parser_peek(parser);
    if(c == REGEX_PARSER_ESCAPE) {
      escaped = true;
    } else if(!escaped && c == REGEX_PARSER_LITERAL) {
      parser_debug_log(parser, "end of literal");
      parser_skip(parser);
      return tree.root;
    } else {
      struct regex_node * node = create_regex_node(REGEX_TYPE_RANGE);
      if(node == NULL) {
        dispose_regex_tree(&tree);
	return NULL;
      }
      node->data.range.start = c;
      node->data.range.end = c + 1;
      if(add_to_regex_tree(&tree, REGEX_TYPE_SEQUENCE, node) != 0) {
	destroy_regex_node(node);
	dispose_regex_tree(&tree);
	return NULL;
      }
      parser_skip(parser);
      escaped = false;
    }
  }
}

/**
 * Parses a regex range bound
 * \param parser the parser
 * \param dest a pointer to the destination buffer
 * \return 0 on success, -1 on error
 */
static int parse_regex_range_bound(struct regex_parser * parser, char * dest) {
  if(!parser_has_more(parser)) {
    parser_error(parser, "unexpected end of file, expected character range bound");
    return -1;
  }
  char c = parser_peek(parser);
  parser_skip(parser);
  if(c == REGEX_PARSER_ESCAPE) {
    if(!parser_has_more(parser)) {
      parser_error(parser, "unexpected end of file, expected escaped character bound");
      return -1;
    }
    c = parser_peek(parser);
    parser_skip(parser);
  } else if(c == REGEX_PARSER_RANGE_END) {
    parser_error(parser, "invalid character, expected character range bound");
    return -1;
  }
  
  *dest = c;
  return 0;
}

/**
 * Parses a range of characters
 * \param parser the parser
 * \return the node representing the range or NULL on error
 */
static struct regex_node * parse_regex_range(struct regex_parser * parser) {
  assert(parser != NULL);

  parser_skip(parser);
  
  parser_skip_whitespace(parser);

  char start;
  if(parse_regex_range_bound(parser, &start) != 0) {
    return NULL;
  }
  
  parser_skip_whitespace(parser);
  
  if(!parser_has_more(parser)) {
    parser_error(parser, "unexpected end of file, expected range separator");
    return NULL;
  }
  char c = parser_peek(parser);
  if(c != REGEX_PARSER_RANGE_SEPARATOR) {
    parser_error(parser, "unexpected character, expected range separator");
    return NULL;
  }
  parser_skip(parser);
  
  parser_skip_whitespace(parser);

  char end;
  if(parse_regex_range_bound(parser, &end) != 0) {
    return NULL;
  }

  parser_skip_whitespace(parser);

  if(!parser_has_more(parser)) {
    parser_error(parser, "unexpected end of file, expected range separator");
    return NULL;
  }
  c = parser_peek(parser);
  if(c != REGEX_PARSER_RANGE_END) {
    parser_error(parser, "unexpected character, expected range separator");
    return NULL;
  }
  parser_skip(parser);

  struct regex_node * node = create_regex_node(REGEX_TYPE_RANGE);
  if(node == NULL) {
    parser_error(parser, "could not create range node");
    return NULL;
  }
  node->data.range.start = start;
  node->data.range.end = end;
  return node;
}

/**
 * Parses a reference
 * \param parser the parser
 * \param symbols the symbol table
 * \return the node representing the reference or NULL on error
 */
static struct regex_node * parse_regex_reference(struct regex_parser * parser, struct regex_symbols * symbols) {
  assert(parser != NULL);

  parser_skip(parser);
  if(!parser_has_more(parser)) {
    parser_error(parser, "unexpected end of file, expected literal or expression end");
    return NULL;
  }

  size_t start = parser_pos(parser);
  parser_skip_while(parser, is_identifier);
  size_t end = parser_pos(parser);
  size_t len = end - start;

  struct regex_symbol * symbol = get_or_create_regex_symbol(parser, symbols, start, len);
  if(symbol == NULL) {
    parser_error(parser, "symbol creation failed");
    return NULL;
  }

  struct regex_node * node = create_regex_node(REGEX_TYPE_REFERENCE);
  if(node == NULL) {
    parser_error(parser, "reference node creation failed");
    return NULL;
  }
  node->data.reference.symbol = symbol;
  return node;
}

static struct regex_node * parse_regex_branch(struct regex_parser * parser, struct regex_symbols * symbols);

/**
 * Parses a group
 * \param parser the parser
 * \param symbols the symbol table
 * \return the node representing the group or NULL on error
 */
static struct regex_node * parse_regex_group(struct regex_parser * parser, struct regex_symbols * symbols) {
  assert(parser != NULL);
  assert(symbols != NULL);

  parser_skip(parser);
  
  struct regex_node * branch = parse_regex_branch(parser, symbols);
  if(branch == NULL) {
    return NULL;
  }
  parser_skip_whitespace(parser);
  if(!parser_has_more(parser)) {
    parser_error(parser, "unexpected end of file, expected statement end");
    destroy_regex_node(branch);
    return NULL;
  }
  if(parser_peek(parser) != REGEX_PARSER_GROUP_END) {
    parser_error(parser, "unexpected character, expected statement end");
    destroy_regex_node(branch);
    return NULL;
  }
  parser_skip(parser);
  return branch;
}

/**
 * Parses an expression
 * \param parser the parser
 * \param symbols the symbol table
 * \return the node representing the expression or NULL on failure
 */
static struct regex_node * parse_regex_expression(struct regex_parser * parser, struct regex_symbols * symbols) {
  assert(parser != NULL);
  assert(parser != NULL);

  if(!parser_has_more(parser)) {
    parser_error(parser, "unexpected end of file, expected literal or expression end");
    return NULL;
  }
  
  char c = parser_peek(parser);
  if(c == REGEX_PARSER_LITERAL) {
    return parse_literal(parser);
  } else if(c == REGEX_PARSER_REFERENCE_PREFIX) {
    return parse_regex_reference(parser, symbols);
  } else if(c == REGEX_PARSER_GROUP_START) {
    return parse_regex_group(parser, symbols);
  } else if(c == REGEX_PARSER_RANGE_START) {
    return parse_regex_range(parser);
  } else {
    parser_error(parser, "unexpected character, expected literal, group or statement end");
    return NULL;
  }
}

/**
 * Parses a regex loop
 * \param parser the parser
 * \param symbols the symbol table
 */
static struct regex_node * parse_regex_loop(struct regex_parser * parser, struct regex_symbols * symbols) {
  assert(parser != NULL);
  assert(symbols != NULL);

  struct regex_node * body = parse_regex_expression(parser, symbols);
  if(body == NULL) {
    return NULL;
  }
  
  if(!parser_has_more(parser)) {
    parser_error(parser, "unexpected end of file, expected expression or statement end");
    destroy_regex_node(body);
    return NULL;
  }

  parser_skip_whitespace(parser);
  
  char c = parser_peek(parser);
  if(c == REGEX_PARSER_LOOP) {
    parser_debug_log(parser, "loop");

    parser_skip(parser);
    struct regex_node * loop = create_regex_node(REGEX_TYPE_LOOP);
    if(loop == NULL) {
      destroy_regex_node(body);
      return NULL;
    }
    loop->data.loop.body = body;
    return loop;
  } else {
    return body;
  }
}

/**
 * Parses a regex sequence
 * \param parser the parser
 * \param symbols the symbol table
 */
static struct regex_node * parse_regex_sequence(struct regex_parser * parser, struct regex_symbols * symbols) {
  assert(parser != NULL);
  assert(symbols != NULL);
  
  struct regex_tree tree = {NULL, NULL};
  
  while(true) {
    if(!parser_has_more(parser)) {
      parser_error(parser, "unexpected end of file, expected loop, expression or statement end");
      dispose_regex_tree(&tree);
      return NULL;
    }
    char c = parser_peek(parser);
    if(c == REGEX_PARSER_STATEMENT_END || c == REGEX_PARSER_GROUP_END || c == REGEX_PARSER_BRANCH_SEPARATOR) {
      return tree.root;
    } else {
      struct regex_node * node = parse_regex_loop(parser, symbols);
      if(node == NULL) {
        dispose_regex_tree(&tree);
	return NULL;
      }
      if(add_to_regex_tree(&tree, REGEX_TYPE_SEQUENCE, node) != 0) {
	parser_error(parser, "could not add element to sequence");
	destroy_regex_node(node);
        dispose_regex_tree(&tree);
	return NULL;
      }
      
      parser_skip_whitespace(parser);
    }
  }
}

/**
 * Parses a regex branch
 * \param parser the parser
 * \param symbols the symbol table
 * \return the branch or NULL on failure
 */
static struct regex_node * parse_regex_branch(struct regex_parser * parser, struct regex_symbols * symbols) {
  assert(parser != NULL);
  assert(symbols != NULL);

  struct regex_tree tree = {NULL, NULL}; 
  
  while(true) {
    if(!parser_has_more(parser)) {
      parser_error(parser, "unexpected end of file, expected sequence, loop, expression or statement end");
      dispose_regex_tree(&tree);
      return NULL;
    }
    char c = parser_peek(parser);
    if(c == REGEX_PARSER_STATEMENT_END || c == REGEX_PARSER_GROUP_END) {
      return tree.root;
    } else {
      struct regex_node * node = parse_regex_sequence(parser, symbols);
      if(node == NULL) {
	dispose_regex_tree(&tree);
	return NULL;
      }
      if(add_to_regex_tree(&tree, REGEX_TYPE_BRANCH, node) != 0) {
	parser_error(parser, "could not add branch to tree");
	destroy_regex_node(node);
	dispose_regex_tree(&tree);
	return NULL;
      }
      if(!parser_has_more(parser)) {
	parser_error(parser, "unexpected end of file, expected branch delimiter or statement end");
	dispose_regex_tree(&tree);
	return NULL;
      }

      char c = parser_peek(parser);
      if(c != REGEX_PARSER_BRANCH_SEPARATOR) {
	return tree.root;
      }
      parser_skip(parser);
      parser_skip_whitespace(parser);
      
    }
  }
}

/**
 * Parses a statement
 * \param parser the parser
 * \param symbols the symbol table
 * \return the node representing the statement or NULL on error
 */
static struct regex_node * parse_regex_statement(struct regex_parser * parser, struct regex_symbols * symbols) {
  assert(parser != NULL);
  assert(symbols != NULL);
  
  struct regex_node * branch = parse_regex_branch(parser, symbols);
  if(branch == NULL) {
    return NULL;
  }
  parser_skip_whitespace(parser);
  if(!parser_has_more(parser)) {
    parser_error(parser, "unexpected end of file, expected statement end");
    destroy_regex_node(branch);
    return NULL;
  }
  if(parser_peek(parser) != REGEX_PARSER_STATEMENT_END) {
    parser_error(parser, "unexpected character, expected statement end");
    destroy_regex_node(branch);
    return NULL;
  }
  parser_skip(parser);
  return branch;
}

/**
 * Parses a symbol
 * \param parser the parser
 * \param symbols the symbol table
 * \return 0 on success, -1 on failure
 */
static int parse_symbol(struct regex_parser * parser, struct regex_symbols * symbols) {
  assert(parser != NULL);
  assert(symbols != NULL);
  
  char c = parser_peek(parser);
  bool lexeme;
  if(c == REGEX_PARSER_LEXEME) {
    lexeme = true;
    parser_skip(parser);
  } else {
    lexeme = false;
  }
  
  size_t name_start = parser_pos(parser);
  parser_skip_while(parser, is_identifier);
  size_t name_len = parser_pos(parser) - name_start;

  struct regex_symbol * symbol = get_or_create_regex_symbol(parser, symbols, name_start, name_len);
  if(symbol == NULL) {
    return -1;
  }
  if(symbol->expression != NULL) {
    LOG_ERROR("multiple definitions for symbol '%s'", symbol->name);
    parser_error(parser, "multiple definitions for symbol");
    return -1;
  }
  symbol->lexeme = lexeme;
  
  parser_skip_whitespace(parser);
  
  if(!parser_has_more(parser)) {
    parser_error(parser, "unexpected end, expected symbol definition or ';'");
    return -1;
  }

  c = parser_peek(parser);
  if(c == REGEX_PARSER_COMMENT) {
    parser_skip_comment(parser);
    parser_skip_whitespace(parser);
  }
  
  if(!parser_has_more(parser)) {
    parser_error(parser, "unexpected end, expected symbol definition or ';'");
    return -1;
  }

  struct regex_node * expr = parse_regex_statement(parser, symbols);
  if(expr == NULL) {
    return -1;
  } else {
    symbol->expression = expr;
    debug_regex_node(expr, 0);
    return 0;
  }
}

struct regex_symbols * parse_regex_symbols(FILE * file) {
  assert(file != NULL);
  
  LOG_DEBUG("parsing symbol file...");
  
  struct regex_parser parser;
  if(read_regex_file(&parser, file) != 0) {
    return NULL;
  }

  struct regex_symbols * symbols = (struct regex_symbols *) malloc(sizeof(struct regex_symbols));
  symbols->head = NULL;
  symbols->tail = NULL;

  if(symbols == NULL) {
    destroy_regex_symbols(symbols);
    dispose_regex_parser(&parser);
    LOG_ERROR("could not allocate symbols");
    return NULL;
  }
  
  while(parser_has_more(&parser)) {
    parser_skip_whitespace(&parser);
    char c = parser_peek(&parser);
    if(c == REGEX_PARSER_COMMENT){
      parser_skip_comment(&parser);
    } else {
      LOG_DEBUG("parsing symbol");
      if(parse_symbol(&parser, symbols) != 0) {
	LOG_ERROR("parser stopped after encountering an error");
	destroy_regex_symbols(symbols);
	break;
      }
    }
  }

  if(!parser_ok(&parser)) {
    parser_error_log(&parser);
    destroy_regex_symbols(symbols);
    return NULL;
  }

  dispose_regex_parser(&parser);
  
  LOG_DEBUG("parsing symbol file done.");

  if(check_regex_symbols(symbols) != 0) {
    destroy_regex_symbols(symbols);
    return NULL;
  }

  LOG_DEBUG("regex symbols constructed");
  return symbols;
}

/**
 * Copies the regex symbol names into a buffer
 * \param dest the destination buffer to be created
 * \param symbol_count set to the number of symbols
 * \param symbols the original set of symbols
 * \return 0 on success, -1 on error
 */
static int copy_regex_symbol_names(const char *** dest, size_t * symbol_count, struct regex_symbols * symbols) {

  size_t count = 0;
  struct regex_symbol * s = symbols->head;
  while(s->next != NULL) {
    ++count;
    s = s->next;
  }

  const char ** names = NULL;
  
  if(count != 0) {
    names = malloc(sizeof(char *) * count);
    if(names == NULL) {
      return -1;
    }
    
    s = symbols->head;
    for(int i = 0; i < count; ++i) {
      size_t len = strlen(s->name);
      char * name = malloc(len + 1);
      if(name == NULL) {
	free(names);
	return -1;
      }
      strcpy(name, s->name);
      names[i] = name;
      s = s->next;
    }
  }
  *dest = names;
  *symbol_count = count;
  return 0;
}

#define INITIAL_REGEX_NFA_BUFFER_SIZE 32

static int add_regex_state(struct regex_nfa * nfa, size_t * result) {
  assert(nfa != NULL);
  assert(result != NULL);
  
  if(nfa->len == nfa->size) {
    size_t nsize;
    if(nfa->size == 0) {
      nsize = INITIAL_REGEX_NFA_BUFFER_SIZE;
    } else {
      nsize = nfa->size * 2;
    }
    struct regex_state * nstates = realloc(nfa->states, nsize * sizeof(struct regex_state));
    if(nstates == NULL) {
      return -1;
    } else {
      nfa->states = nstates;
      nfa->size = nsize;
    }
  }
  *result = nfa->len;
  ++nfa->len;
  return 0;
}


static int build_regex_nfa_from_node(struct regex_nfa * nfa, struct regex_node * node, size_t * first, size_t * last);

static int build_regex_sequence_nfa(struct regex_nfa * nfa, struct regex_node * node, size_t * first, size_t * last) {
  return 0;
}

static int build_regex_branch_nfa(struct regex_nfa * nfa, struct regex_node * node, size_t * first, size_t * last) {
  return 0;
}

static int build_regex_range_nfa(struct regex_nfa * nfa, struct regex_node * node, size_t * first, size_t * last) {
  size_t id;
  if(add_regex_state(nfa, &id) == -1) {
    return -1;
  }
  struct regex_state * state = nfa->states + id;
  state->lower = node->data.range.start;
  state->upper = node->data.range.end + 1;
  state->end = -1;
  *first = id;
  *last = id;
  return 0;
}

/**
 *
 */
static int build_regex_loop_nfa(struct regex_nfa * nfa, struct regex_node * node, size_t * first, size_t * last) {

  // State machine:
  // ? -> (Body start) -> ... -> (Body end) -> (End) -> ?
  //      <-----------------------------------
  //

  size_t body_first;
  size_t body_last;
  if(build_regex_nfa_from_node(nfa, node->data.loop.body, &body_first, &body_last) == -1) {
    return -1;
  }

  start->then = body_first;

  size_t end_id;
  if(add_regex_state(nfa, &end_id) == -1) {
    return -1;
  }
  struct regex_state * end = nfa->states + end_id;
  end->lower = 0;
  end->upper = 0;
  end->end = -1;

  nfa->states[body_last].then = end_id;

  *first = body_first;
  *last = end_id;
  
  return 0;
}

static int build_regex_nfa_from_node(struct regex_nfa * nfa, struct regex_node * node, size_t * first, size_t * last) {
  assert(nfa != NULL);
  assert(node != NULL);
  assert(first != NULL);
  assert(last != NULL);
  switch(node->type) {
  case REGEX_TYPE_SEQUENCE:
    return build_regex_sequence_nfa(nfa, node, first, last);
  case REGEX_TYPE_BRANCH:
    return build_regex_branch_nfa(nfa, node, first, last);
  case REGEX_TYPE_RANGE:
    return build_regex_range_nfa(nfa, node, first, last);
  case REGEX_TYPE_LOOP:
    return build_regex_loop_nfa(nfa, node, first, last);
  default:
    return -1;
  }
}

/**
 * Builds a regex NFA, one symbol at a time
 * \param the state machine
 * \param the start state to be connected to the new state machine
 * \param symbol the symbol to express
 * \param id the index of the symbol, to be set at the end state
 * \return 0 on success, -1 on error
 */
static int build_regex_nfa(struct regex_nfa * nfa, size_t start, struct regex_symbol * symbol, int id) {
  assert(nfa != NULL);
  assert(symbol != NULL);
  assert(symbol->expression != NULL);
  assert(id >= 0);
  size_t first;
  size_t last;
  int result = build_regex_nfa_from_node(nfa, symbol->expression, &first, &last);
  if(result == 0) {
    nfa->states[start].then = first;
    nfa->states[last].then = 0;
    nfa->states[last].otherwise = 0;
    nfa->states[last].end = id;
  }
  return result;
}

int parse_regex_nfa(FILE * file, struct regex_nfa * nfa) {
  assert(file != NULL);
  assert(nfa != NULL);
  
  struct regex_symbols * symbols = parse_regex_symbols(file);
  if(symbols == NULL) {
    return -1;
  }

  nfa->states = NULL;
  nfa->size = 0;
  nfa->len = 0;
  if(copy_regex_symbol_names(&nfa->symbols, &nfa->symbols_len, symbols) == -1) {
    destroy_regex_symbols(symbols);
    return -1;
  }

  struct regex_symbol * s = symbols->head;
  int index = 0;
  size_t start;
  
  if(add_regex_state(nfa, &start) == -1) {
    dispose_regex_nfa(nfa);
    destroy_regex_symbols(symbols);
    return -1;
  }

  while(s->next != NULL) {
    struct regex_state * state = nfa->states + start;
    state->lower = 0;
    state->upper = 0;
    state->end = -1;
    
    if(build_regex_nfa(nfa, start, s, index) == -1) {
      dispose_regex_nfa(nfa);
      destroy_regex_symbols(symbols);
      return -1;
    }
    if(s->next == NULL) {
      state->otherwise = state->then;
    } else {
      size_t next_state;
      if(add_regex_state(nfa, &next_state) == -1) {
	dispose_regex_nfa(nfa);
	destroy_regex_symbols(symbols);
	return -1;
      }
      state->otherwise = next_state;
      start = next_state;
    }
    s = s->next;
    ++index;
  }

  destroy_regex_symbols(symbols);
  return 0;
}

void dispose_regex_nfa(struct regex_nfa * nfa) {
  
}

int init_regex_matcher(struct regex_matcher * m, const struct regex_nfa * nfa) {
  return 0;
}

int match_regex(struct regex_matcher * m, const char * input) {
  return 0;
}

void reset_regex_matcher(struct regex_matcher * m) {

}

void dispose_regex_matcher(struct regex_matcher * m) {

}
