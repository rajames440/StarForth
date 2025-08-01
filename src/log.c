#include "log.h"
#include <stdarg.h>
#include <time.h>

static LogLevel current_level = LOG_INFO;

/* Level name strings */
static const char *level_names[] = {
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG"
};

/* ANSI color codes */
static const char *level_colors[] = {
    "\x1b[31m", /* ERROR - Red */
    "\x1b[33m", /* WARN  - Yellow */
    "\x1b[32m", /* INFO  - Green */
    "\x1b[34m"  /* DEBUG - Blue */
};

static const char *color_reset = "\x1b[0m";

void log_set_level(LogLevel level) {
    current_level = level;
}

static void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%H:%M:%S", tm_info);
}

void log_message(LogLevel level, const char *fmt, ...) {
    if (level > current_level)
        return;

    char timestamp[16];
    get_timestamp(timestamp, sizeof(timestamp));

    fprintf(stderr, "%s[%s] %s: %s", level_colors[level], timestamp, level_names[level], color_reset);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}
