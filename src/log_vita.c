#include "log.h"
#include <psp2kern/kernel/debug.h>
#include <stdarg.h>
#include <stdio.h>

void log_write(const char *level, const char *fmt, ...) {
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    ksceDebugPrintf("[psvita-mqtt][%s] %s\n", level, line);
}
