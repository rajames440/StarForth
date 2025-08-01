#ifndef LOG_H
#define LOG_H

#include <stdio.h>

/* Logging levels */
typedef enum {
    LOG_ERROR = 0,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG
} LogLevel;

/* Set the global logging level (default LOG_INFO) */
void log_set_level(LogLevel level);

/* Log a formatted message at the specified level */
void log_message(LogLevel level, const char *fmt, ...);

#endif /* LOG_H */
