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

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

/**
 * Log level of a message
 */
enum log_level {
  /**
   * Debug messages
   */
  LOG_LEVEL_DEBUG,
  /**
   * Information messages
   */
  LOG_LEVEL_INFO,
  /**
   * Warning messages
   */
  LOG_LEVEL_WARNING,
  /**
   * Error messages
   */
  LOG_LEVEL_ERROR
};

/**
 * Starts the logging subsystem
 * \param output the log file
 * \param min_level the minimum log level for a message to be displayed
 * \return 0 on success, -1 otherwise
 */
int start_logging(FILE * output, enum log_level min_level);

/**
 * Logs a message
 * \param level the log level
 * \param file the file where the message originated or NULL
 * \param line the line where the message originated
 * \param format the message format
 * \param ... the arguments for the message
 * \return 0 on success, -1 on failure
 */
int log_message(enum log_level level, const char * file, int line, const char * format, ...);

/**
 * Gets the minimum log level
 * \return the minimum log level
 */
enum log_level get_min_log_level();

/**
 * Signals the logging system to stop and blocks until it does so
 * \return 0 on success, -1 otherwise
 */
int stop_logging();

/**
 * Convenience macro to log a message at the current line
 */
#define LOG_MESSAGE(level, ...) if(get_min_log_level() <= level) log_message(level, __FILE__, __LINE__, __VA_ARGS__)

/**
 * Convenience macro to log a debug message at the current line
 */
#define LOG_DEBUG(...) LOG_MESSAGE(LOG_LEVEL_DEBUG, __VA_ARGS__)

/**
 * Convenience macro to log an info message at the current line
 */
#define LOG_DEBUG(...) LOG_MESSAGE(LOG_LEVEL_INFO, __VA_ARGS__)

/**
 * Convenience macro to log a warning message at the current line
 */
#define LOG_WARNING(...) LOG_MESSAGE(LOG_LEVEL_WARNING, __VA_ARGS__)

/**
 * Convenience macro to log an error message at the current line
 */
#define LOG_ERROR(...) LOG_MESSAGE(LOG_LEVEL_ERROR, __VA_ARGS__)

#endif
