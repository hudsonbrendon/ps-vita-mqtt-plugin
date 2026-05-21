#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

char log_host_buffer[1024];

void log_host_reset(void) { log_host_buffer[0] = '\0'; }

void log_write(const char *level, const char *fmt, ...) {
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    size_t len = strlen(log_host_buffer);
    snprintf(log_host_buffer + len, sizeof log_host_buffer - len,
             "[%s] %s\n", level, line);
}
