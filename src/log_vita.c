#include "log.h"
#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>
#include <stdarg.h>
#include <string.h>

/* Use sceClib variants exclusively in this file — never include <stdio.h>
 * because we don't link SceLibc. */
#define vsnprintf sceClibVsnprintf
#define snprintf  sceClibSnprintf

/* Debug build: also append to ux0:data/ps-vita-mqtt/plugin.log so we can
 * read it back over FTP after the plugin crashes/hangs. */
#define LOG_PATH "ux0:/data/ps-vita-mqtt/plugin.log"

void log_write(const char *level, const char *fmt, ...) {
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    sceClibPrintf("[psvita-mqtt][%s] %s\n", level, line);

    char out[600];
    int m = snprintf(out, sizeof out, "[%s] %s\n", level, line);
    if (m <= 0) return;
    SceUID fd = sceIoOpen(LOG_PATH,
                          SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0666);
    if (fd >= 0) {
        sceIoWrite(fd, out, m);
        sceIoClose(fd);
    }
}
