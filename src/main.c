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

#include <stdlib.h>

static int read_regex_file() {
  FILE * file = fopen("../config/syntax.sym", "r");
  if(file == NULL) {
    LOG_ERROR("could not open syntax file");
    return -1;
  }

  struct regex_symbols * symbols = parse_regex_symbols(file);
  
  if(fclose(file) != 0) {
    LOG_ERROR("could not close syntax file");
    destroy_regex_symbols(symbols);
    return -1;
  }
  
  destroy_regex_symbols(symbols);
  return 0;
}

/**
 * The main entry point of the application
 */
int main(int arg_count, const char * args[]) {

  int result;
  
  if(start_logger(stdout, LOG_LEVEL_DEBUG) != 0) {
    fputs("could not start logger", stdout);
    return EXIT_FAILURE;
  }

  result = read_regex_file();
  
  if(stop_logger() != 0) {
    fputs("could not stop logger", stdout);
    result = -1;
  }
  
  if(result == 0) {
    return EXIT_SUCCESS;
  } else {
    return EXIT_FAILURE;
  }
}
