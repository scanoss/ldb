#ifndef __LOGGER_H
#define __LOGGER_H

#include <pthread.h>

typedef enum {
    LOG_BASIC,
    LOG_INFO,
    LOG_DEBUG
} log_level_t;

void logger_init(char * db, int tnumber,  pthread_t * tlist);
void logger_offset_increase(int off);
void import_logger(char * basic, const char * fmt, ...);
void log_info(const char * fmt, ...);
void logger_set_level(log_level_t l);

#define LOG_INF(fmt,args...) import_logger(NULL, fmt, args)
#endif