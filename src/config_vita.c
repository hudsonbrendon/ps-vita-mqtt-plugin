#include "config.h"
#include <psp2kern/kernel/iofilemgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <string.h>

int config_load_from_path(const char *path, mqtt_config *out) {
    SceUID fd = ksceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) return -1;
    char buf[4096];
    int n = ksceIoRead(fd, buf, sizeof buf - 1);
    ksceIoClose(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return config_parse_json(buf, out);
}
