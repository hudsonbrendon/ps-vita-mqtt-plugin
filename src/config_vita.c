#include "config.h"
#include <psp2/io/fcntl.h>
#include <string.h>

int config_load_from_path(const char *path, mqtt_config *out) {
    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) return -1;
    char buf[4096];
    int n = sceIoRead(fd, buf, sizeof buf - 1);
    sceIoClose(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return config_parse_json(buf, out);
}
