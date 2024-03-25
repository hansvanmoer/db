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
#include "regex_nfa.h"

#include <assert.h>
#include <stdlib.h>

#define INITIAL_NFA_SIZE 32

int init_regex_nfa(struct regex_nfa * n) {
  assert(n != NULL);

  struct regex_nfa_state * buffer = (struct regex_nfa_state *) malloc(sizeof(struct regex_nfa_state) * INITIAL_NFA_SIZE);
  if(buffer == NULL) {
    LOG_ERROR("could not allocate initial NFA buffer");
    return -1;
  }
  n->states = buffer;
  n->states[0].next = 0; // set the initial start at 0 to indicate an empty automaton
  n->size = INITIAL_NFA_SIZE;
  n->len = 1;
  return 0;
}

int add_regex_nfa_state(struct regex_nfa * n, regex_nfa_id * dest) {
  assert(n != NULL);
  assert(dest != NULL);
  
  regex_nfa_id id = n->len;
  if(n->size == id) {
    size_t nsize = 2 * n->size;
    struct regex_nfa_state * nstates = (struct regex_nfa_state *) realloc(n->states, sizeof(struct regex_nfa_state) * nsize);
    if(nstates == NULL) {
      LOG_ERROR("could not allocate NFA state buffer");
      return -1;
    }
    n->states = nstates;
    n->size = nsize;
  }
  n->states[id].predicate = 0;
  n->states[id].next = 0;
  n->states[id].branch = 0;
  ++n->len;
  *dest = id;
  return 0;
}

void set_regex_nfa_predicate(struct regex_nfa * n, regex_nfa_id from, int predicate) {
  assert(n != NULL);
  assert(from < n->len);
  
  struct regex_nfa_state * state = n->states + from;
  
  state->predicate = predicate;
}

void set_regex_nfa_otherwise(struct regex_nfa * n, regex_nfa_id from, regex_nfa_id to) {
  assert(n != NULL);
  assert(from < n->len);
  assert(to < n->len);

  n->states[from].branch = to;
}

void set_regex_nfa_next(struct regex_nfa * n, regex_nfa_id from, regex_nfa_id to) {
  assert(n != NULL);
  assert(from < n->len);
  assert(to < n->len);
  n->states[from].next = to;
}

void set_regex_nfa_start(struct regex_nfa * n, regex_nfa_id start_id) {
  assert(n != NULL);
  assert(start_id != 0);
  assert(start_id < n->len);
  n->states[0].next = start_id;
}

void dispose_regex_nfa(struct regex_nfa * n) {
  assert(n != NULL);

  free(n->states);
}

int init_regex_nfa_matcher(struct regex_nfa_matcher * matcher, size_t max_len) {
  assert(matcher != NULL);
  assert(max_len != 0);

  int * states = (int *) malloc(sizeof(int) * max_len);
  if(states == NULL) {
    LOG_ERROR("unable to allocate matcher stack");
    return NULL;
  }
  matcher->stack = states;
  matcher->stack_len = 0;
  matcher->stack_size = max_len;
  matcher->result = 0;
  matcher->len = 0;
  return 0;
}

void reset_regex_nfa_matcher(struct regex_nfa_matcher * matcher) {
  assert(matcher != NULL);
  matcher->stack_len = 0;
  matcher->result = 0;
  matcher->len = 0;
}

int match_regex_nfa(const struct regex_nfa * n, struct regex_nfa_matcher * matcher, const char * str) {
  assert(n != NULL);
  assert(matcher != NULL);
  assert(str != NULL);

  size_t len = 0;
  size_t size = matcher->stack_size;
  struct regex_nfa_state * states = n->states;
  int pos = 0;
  while(true) {
    if(*str == '\0') {
      // set result to 0 and length to the entire length of the input => incomplete symbol
      matcher->result = 0;
      matcher->len = len;
      return 0;
    }
    if(len == stack_size) {
      // set result to 0 and length to the scanned length => stack overflow
      matcher->result = 0;
      matcher->len = len;
      return 0;
    }
    int c = (int) *str;
    int pred = states[pos].predicate;
    if(pred == 0 || ) {
      pos = states[pos].next;
    } else if(pred == c) {
      pos = states[pos].next;
    } else {
      pos = states[pos].otherwise;
      if(pos == 0) {
	
      }
    }
  }
}

void dispose_regex_nfa_matcher(struct regex_nfa_matcher * matcher) {
  assert(matcher != NULL);

  free(matcher->states);
}
