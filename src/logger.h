#ifndef __LOGGER_H
#define __LOGGER_H

#include <pthread.h>

typedef enum {
    LOG_BASIC = 0,
    LOG_INFO,
    LOG_DEBUG
} log_level_t;

void logger_init(char * db, int tnumber,  pthread_t * tlist);
void log_set_quiet(bool mode);
void logger_offset_increase(int off);
void import_logger(const char * fmt, ...);
void log_info(const char * fmt, ...);
void logger_set_level(log_level_t l);
void log_debug(const char * fmt, ...);
void logger_dbname_set(char * db);
void logger_set_path(const char * path);
void logger_basic(const char * fmt, ...);
void logger_clear_screen(void);
/* Enable the interactive import UI (screen clear, spinner, multi-thread
 * cursor positioning). Disabled by default so library consumers never
 * emit terminal control sequences. */
void logger_set_import_ui(bool enable);
#define LOG_INF(fmt,args...) import_logger(NULL, fmt, args)
#endif