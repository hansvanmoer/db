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

#ifndef REGEX_NFA_H
#define REGEX_NFA_H

#include <stdlib.h>

typedef size_t regex_nfa_id;

/**
 * A state in the regex NFA
 */
struct regex_nfa_state {
  /**
   * The predicate character or '\0' if this is an epsilon transition
   */
  int predicate;
  
  /**
   * The index of the next state if the predicate is accepted, or 0 if this is an end state
   */
  regex_nfa_id next;

  /**
   * The index of the next state if the predicate is rejected, or 0 if this indicates failure
   */
  regex_nfa_id otherwise;

  /**
   * When set to something other than 0, this is a finite state with the specified result
   */
  int result;
};

/**
 * A non deterministic finite automaton for pattern matching
 *
 */
struct regex_nfa {
  /**
   * The set of states for this state machine
   */
  struct regex_nfa_state * states;

  /**
   * The size of the state buffer
   */
  size_t size;

  /**
   * The number of states
   */
  size_t len;
};

/**
 * Stores the result of a match
 */
struct regex_nfa_matcher {
  /**
   * The stack
   */
  int * stack;

  /**
   * Max size of the stack
   */
  size_t stack_size;

  /**
   * The current size of the stack
   */
  size_t stack_len;

  /**
   * The end result value
   */
  int result;

  /**
   * The length of the match, 0 if there was no match
   */
  size_t len;
};

/**
 * Initializes the automaton
 * \param n a pointer to the nfa
 * \return 0 on success, -1 on failure
 */
int init_regex_nfa(struct regex_nfa * n);

/**
 * Adds a new state to the automaton
 * \param n the automaton
 * \param result a pointer to store the resulting ID in
 * \return 0 on success, -1 of the new state on success
 */
int add_regex_nfa_state(struct regex_nfa * n, regex_nfa_id * result);

/**
 * Defines the transition predicate and next state for an nfa state
 * \param n the automaton
 * \param from the ID of the state from which to transition
 * \param predicate the predicate character
 */
void set_regex_nfa_predicate(struct regex_nfa * n, regex_nfa_id id, int predicate);

/**
 * Defines the next transition on an nfa state
 * \param n the automaton
 * \param from the state from which to define a transition
 */
void set_regex_nfa_next(struct regex_nfa * n, regex_nfa_id from, regex_nfa_id to);

/**
 * Defines the otherwise transition on an nfa state
 * \param n the automaton
 * \param from the state from which to define a transition
 */
void set_regex_nfa_otherwise(struct regex_nfa * n, regex_nfa_id from, regex_nfa_id to);

/**
 * Sets the nfa start state
 * \param n the automaton
 * \param start the ID of the start state
 */
void set_regex_nfa_start(struct regex_nfa * n, regex_nfa_id start);

/**
 * Disposes of the nfa
 * \param n the automaton
 */
void dispose_regex_nfa(struct regex_nfa * n);


/**
 * Initializes a matcher
 * \param matcher a pointer to the matcher
 * \param max_len the maximum length of a symbol
 * \return 0 on success, -1 on failure
 */
int init_regex_nfa_matcher(struct regex_nfa_matcher * matcher, size_t max_len);

/**
 * Resets the matcher
 * \param matcher the matcher
 */
void reset_regex_nfa_matcher(struct regex_nfa_matcher * matcher);

/**
 * Runs the regex state machine
 * \param n the automaton
 * \param matcher a pointer to the regex matcher
 * \param str the input string
 * \return 0 on success, -1 on failure
 */
int match_regex_nfa(const struct regex_nfa * n, struct regex_nfa_matcher * matcher, const char * str);

/**
 * Disposes all resources associated with this matcher
 * \param matcher a pointer to the matcher
 */
void dispose_regex_nfa_matcher(struct regex_nfa_matcher * matcher);

#endif
