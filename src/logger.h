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
 * Log levels
 */
enum log_level {
  /**
   * The level for debug messages
   */
  LOG_LEVEL_DEBUG,

  /**
   * The level for info messages
   */
  LOG_LEVEL_INFO,

  /**
   * The level for warnings
   */
  LOG_LEVEL_WARNING,

  /**
   * The level for errors
   */
  LOG_LEVEL_ERROR
};

/**
 * Starts the logging subsystem
 * \param output the output file
 * \param min_log_level the minimum log level of messages to display
 * \return 0 on success, -1 on error
 */
int start_logger(FILE * output, enum log_level min_log_level);

/**
 * Logs a message
 * \param level the log level of the message
 * \param file the file where the message originates
 * \param line the line where the message originates
 * \param format the format string
 * \param ... the arguments
 * \return 0 on success, -1 on error
 */
int log_message(enum log_level level, const char * file, int line, const char * format, ...);

/**
 * Fetches the minimum log level for messages to display
 * \return the minimum log level
 */
enum log_level get_min_log_level();

/**
 * Stops the logging subsystem
 * \return 0 on success, -1 on error
 */
int stop_logger();

/**
 * A convenience macro to log a message on this line
 */
#define LOG_MESSAGE(level, ...) if(get_min_log_level() <= level) log_message(level, __FILE__, __LINE__, __VA_ARGS__)

/**
 * A convenience macro to log a debug message on this line
 */
#define LOG_DEBUG(...) LOG_MESSAGE(LOG_LEVEL_DEBUG, __VA_ARGS__)

/**
 * A convenience macro to log a debug message on this line
 */
#define LOG_INFO(...) LOG_MESSAGE(LOG_LEVEL_INFO, __VA_ARGS__)

/**
 * A convenience macro to log a warning message on this line
 */
#define LOG_WARNING(...) LOG_MESSAGE(LOG_LEVEL_WARNING, __VA_ARGS__)

/**
 * A convenience macro to log a error message on this line
 */
#define LOG_ERROR(...) LOG_MESSAGE(LOG_LEVEL_ERROR, __VA_ARGS__)


#endif
