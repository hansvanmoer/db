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

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

/**
 * Buffer size for the API error messages
 */
#define ERROR_BUFFER_SIZE 128

/**
 * A log message
 */
struct log_msg {
  /**
   * Log level
   */
  enum log_level level;

  /**
   * A statically allocated file name
   */
  const char * file;

  /**
   * The line
   */
  int line;

  /**
   * The content buffer
   */
  char * buffer;

  /**
   * Content buffer size in bytes
   */
  size_t size;

  /**
   * A pointer to the previous message in the queue
   */
  struct log_msg * prev;

  /**
   * A pointer to the next message in the queue
   */
  struct log_msg * next;
};

/**
 * A queue of log message
 */
struct log_queue {
  /**
   * The head of the queue
   */
  struct log_msg * head;

  /**
   * The tail of the queue
   */
  struct log_msg * tail;
};

/**
 * The status of the log worker
 */
enum log_status {
  /**
   * The worker has exited successfully
   */
  LOG_STATUS_OK,

  /**
   * The worker failed to wait for the condition variable
   */
  LOG_STATUS_WAIT,

  /**
   * The worker failed to lock the waiting mutex
   */
  LOG_STATUS_WAITING_LOCK,

  /**
   * The worker failed to unlock the waiting mutex
   */
  LOG_STATUS_WAITING_UNLOCK,

  /**
   * The worker failed to lock the waiting mutex
   */
  LOG_STATUS_READY_LOCK,

  /**
   * The worker failed to unlock the waiting mutex
   */
  LOG_STATUS_READY_UNLOCK,

  /**
   * The worker failed to print a log message
   */
  LOG_STATUS_PRINT
};

static const char * log_status_labels[] = {
  "ok",
  "error waiting for log signal",
  "failed to lock waiting log queue mutex",
  "failed to unlock waiting log queue mutex",
  "failed to lock ready log queue mutex",
  "failed to unlock ready log queue mutex",
  "failed to print log message"
};

/**
 * Whether the system is running, protected by the waiting mutex
 */
static bool running;

/**
 * The minimum log level, protected by the waiting queue mutex
 */
static enum log_level min_log_level;

/**
 * The queue for messages waiting to be written, protected by a mutex
 */
static struct log_queue waiting;

/**
 * The mutex protecting the waiting queue, the min_log_level and the running flag
 */
static pthread_mutex_t waiting_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * The condition variable used to signal new messages or that the worker may stop
 */
static pthread_cond_t waiting_cond = PTHREAD_COND_INITIALIZER;

/**
 * The queue for messages ready to be reused, protected by a mutex
 */
static struct log_queue ready;

/**
 * The mutex protecting the ready queue
 */
static pthread_mutex_t ready_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * The worker thread handle
 */
static pthread_t worker;

/**
 * The output file
 */
static FILE * output;

/**
 * Log level labels
 */
static const char * log_level_labels[] =  {
  "DEBUG:  ",
  "INFO:   ",
  "WARNING:",
  "ERROR:  "
};

/**
 * Creates a new log message
 * \return a newly allocated message on the stack
 */
static struct log_msg * create_log_msg() {
  struct log_msg * msg = (struct log_msg *) malloc(sizeof(struct log_msg));
  if(msg != NULL) {
    msg->level = LOG_LEVEL_DEBUG;
    msg->file = NULL;
    msg->line = 0;
    msg->buffer = NULL;
    msg->size = 0;
    msg->prev = NULL;
    msg->next = NULL;
  }
  return msg;
}

/**
 * Destroys a log message
 * \param msg the message
 */
static void destroy_log_msg(struct log_msg * msg) {
  assert(msg != NULL);

  free(msg->buffer);
  free(msg);
}

/**
 * Initializes a log queue
 * \param q the queue
 */
static void init_log_queue(struct log_queue * q) {
  assert(q != NULL);

  q->head = NULL;
  q->tail = NULL;
}

/**
 * Pushes a log message onto the queue
 * \param q the queue
 * \param msg the message
 */
static void push_log_msg(struct log_queue * q, struct log_msg * msg) {
  assert(q != NULL);
  assert(msg != NULL);

  if(q->tail == NULL) {
    q->head = msg;
  } else {
    q->tail->next = msg;
  }
  msg->prev = q->tail;
  msg->next = NULL;
  q->tail = msg;
}

/**
 * Pops a message off the queue
 * \param q the queue
 * \return the message or NULL if the queue was empty
 */
static struct log_msg * pop_log_msg(struct log_queue * q) {
  assert(q != NULL);

  if(q->head == NULL) {
    return NULL;
  } else {
    struct log_msg * msg = q->head;
    q->head = q->head->next;
    if(q->head == NULL) {
      q->tail = NULL;
    }
    msg->next = NULL;
    return msg;
  }
}

/**
 * Moves all messages from one queue to the other
 * \param dest the destination queue
 * \param src the source queue
 */
static void move_log_msgs(struct log_queue * dest, struct log_queue * src) {
  assert(dest != NULL);
  assert(src != NULL);

  if(src->head != NULL) {
    if(dest->head == NULL) {
      dest->head = src->head;
    } else {
      dest->tail = src->head;
      src->head->prev = dest->tail;
    }
    dest->tail = src->tail;
    src->head = NULL;
    src->tail = NULL;
  }
}

/**
 * Disposes of all resources associated with this log queue
 * \param q the log queue
 */
static void dispose_log_queue(struct log_queue * q) {
  assert(q != NULL);

  struct log_msg * msg = q->head;
  while(msg != NULL) {
    struct log_msg * next = msg->next;
    destroy_log_msg(msg);
    msg = next;
  }
}

/**
 * Attempts to print an error log to stderr
 * \param message the message
 * \param error the error code
 */
static void log_errno(const char * message, int error) {
  assert(message != NULL);

  fputs(message, stderr);
  fputs(": ", stderr);
    
  char buffer[ERROR_BUFFER_SIZE];
  if(strerror_r(error, buffer, ERROR_BUFFER_SIZE) == 0) {
    fputs(buffer, stderr);
  } else {
    fputs("unknown error", stderr);
  }
  fputc('\n', stderr);
}

/**
 * Recycles the messages on this queue
 * \param q the queue
 * \return LOG_STATUS_OK or error code
 */
static enum log_status recycle_log_msgs(struct log_queue * q) {
  assert(q != NULL);

  if(pthread_mutex_lock(&ready_mutex) != 0) {
    return LOG_STATUS_READY_LOCK;
  }

  move_log_msgs(&ready, q);

  if(pthread_mutex_unlock(&ready_mutex) != 0) {
    return LOG_STATUS_READY_UNLOCK;
  }
  
  return LOG_STATUS_OK;
}

/**
 * Either recycles or creates a log message
 */
static struct log_msg * get_log_msg(size_t min_size) {
  struct log_msg * msg;

  if(pthread_mutex_lock(&ready_mutex) != 0) {
    return NULL;
  }

  msg = pop_log_msg(&ready);

  if(pthread_mutex_unlock(&ready_mutex) != 0) {
    return NULL;
  }

  if(msg == NULL) {
    msg = create_log_msg();
    if(msg != NULL) {
      char * nbuffer = malloc(min_size);
      if(nbuffer == NULL) {
	destroy_log_msg(msg);
	return NULL;
      }
      msg->buffer = nbuffer;
      msg->size = min_size;
    }
  } else {
    if(min_size > msg->size) {
      char * nbuffer = realloc(msg->buffer, min_size);
      if(nbuffer == NULL) {
	destroy_log_msg(msg);
	return NULL;
      }
      msg->buffer = nbuffer;
      msg->size = min_size;
    }
  }
  return msg;
}

/**
 * Prints a message
 * \param msg the message
 * \return LOG_STATUS_OK or an error code
 */
static enum log_status print_log_msg(struct log_msg * msg) {
  int result;
  if(msg->file != NULL) {
    result = fprintf(stderr, "%s%s:%d\t%s\n", log_level_labels[msg->level], msg->file, msg->line, msg->buffer);
  } else {
    result = fprintf(stderr, "%s:\t%s\n", log_level_labels[msg->level], msg->buffer);
  }
  if(result < 0) {
    return LOG_STATUS_PRINT;
  } else {
    return LOG_STATUS_OK;
  }
}

/**
 * Prints the log messages
 * \param q the queue
 * \return LOG_STATUS_OK or an error code
 */
static enum log_status print_log_msgs(struct log_queue * q) {
  assert(q != NULL);

  enum log_status status = LOG_STATUS_OK;
  struct log_msg * msg = q->head;
  while(msg != NULL && status == LOG_STATUS_OK) {
    if(min_log_level <= msg->level) {
      status = print_log_msg(msg);
    }
    msg = msg->next;
  }
}

/**
 * Runs in the worker thread
 * \param arg always NULL
 * \return the status of the worker thread, cast to void
 */
static void * run_worker(void * arg) {
  enum log_status * status = malloc(sizeof(enum log_status));
  if(status == NULL) {
    return NULL;
  }
  
  *status = LOG_STATUS_OK;
  struct log_queue q;
  init_log_queue(&q);

  if(pthread_mutex_lock(&waiting_mutex) != 0) {
    *status = LOG_STATUS_WAITING_LOCK;
    return status;
  }
  if(!running) {

    if(pthread_mutex_unlock(&waiting_mutex) != 0) {
      *status = LOG_STATUS_WAITING_LOCK;
    }
    return status;
  }
  while(true) {
    if(pthread_cond_wait(&waiting_cond, &waiting_mutex) != 0) {
      *status = LOG_STATUS_WAIT;
      pthread_mutex_unlock(&waiting_mutex);
      break;
    }
    
    move_log_msgs(&q, &waiting);
    if(pthread_mutex_unlock(&waiting_mutex) != 0) {
      *status = LOG_STATUS_WAITING_LOCK;
      break;
    }
    *status = print_log_msgs(&q);
    if(*status != LOG_STATUS_OK) {
      pthread_mutex_unlock(&waiting_mutex);
      break;
    }
    *status = recycle_log_msgs(&q);
    if(*status != LOG_STATUS_OK) {
      break;
    }
    
    if(pthread_mutex_lock(&waiting_mutex) != 0) {
      *status = LOG_STATUS_WAITING_LOCK;
      break;
    }
    
    if(!running) {
      if(pthread_mutex_unlock(&waiting_mutex) != 0) {
	*status = LOG_STATUS_WAITING_LOCK;
      }
      break;
    }
  }
  
  dispose_log_queue(&q);
  return status;
}

int start_logger(FILE * output_, enum log_level min_log_level_) {
  assert(output_ != NULL);

  min_log_level = min_log_level_;
  output = output_;
  running = true;
  
  init_log_queue(&waiting);
  init_log_queue(&ready);
  
  int result = pthread_create(&worker, NULL, run_worker, NULL);
  if(result != 0) {
    log_errno("could not start worker thread", result);
    return -1;
  }
  return 0;
}

int log_message(enum log_level level, const char * file, int line, const char * format, ...) {
  if(format == NULL || level < LOG_LEVEL_DEBUG || level > LOG_LEVEL_ERROR) {
    return -1;
  }

  va_list args;
  va_start(args, format);
  int min_size = vsnprintf(NULL, 0, format, args);
  va_end(args);
  
  if(min_size < 0) {
    return -1;
  }
  ++min_size;

  struct log_msg * msg = get_log_msg(min_size);
  if(msg == NULL) {
    return -1;
  }
  msg->level = level;
  msg->file = file;
  msg->line = line;

  va_list args2;
  va_start(args2, format);
  int result = vsnprintf(msg->buffer, msg->size, format, args2);
  va_end(args2);

  if(result < 0) {
    destroy_log_msg(msg);
    return -1;
  }

  if(pthread_mutex_lock(&waiting_mutex) != 0) {
    destroy_log_msg(msg);
    return -1;
  }
  push_log_msg(&waiting, msg);
  if(pthread_mutex_unlock(&waiting_mutex) != 0) {
    return -1;
  }
  if(pthread_cond_signal(&waiting_cond) != 0) {
    return -1;
  }
  
  return 0;
}

enum log_level get_min_log_level() {
  return min_log_level;
}

int stop_logger() {
  int result = pthread_mutex_unlock(&waiting_mutex);
  if(result != 0) {
    log_errno("could not lock the ready mutex to signal shutdown", result);
    return -1;
  }

  running = false;
  result = pthread_cond_signal(&waiting_cond);
  if(result != 0) {
    log_errno("could not send signal to shut down worker", result);
    return -1;
  }

  result = pthread_mutex_unlock(&waiting_mutex);
  if(result != 0) {
    log_errno("could not unlock ready mutex after sending shutdown signal", result);
    return -1;
  }

  enum log_status * worker_status = NULL;
  result = pthread_join(worker, (void **)&worker_status);
  if(result != 0 && result != EINVAL) {
    log_errno("could not join worker thread", result);
    return -1;
  }

  if(worker_status == NULL) {
    fputs("thread could not allocate result buffer\n", stderr);
  }else if(*worker_status != LOG_STATUS_OK) {
    fputs(log_status_labels[*worker_status], stderr);
    fputc('\n', stderr);
  }
  free(worker_status);

  // In case there are messages that have not been handled by the worker thread
  print_log_msgs(&waiting);
  
  dispose_log_queue(&waiting);
  dispose_log_queue(&ready);

  return 0;
}
