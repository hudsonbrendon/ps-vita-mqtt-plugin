#include "collectors.h"
#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>
#include <string.h>

/* The SceShell-side publisher cannot ask the kernel for "which app is
 * running" — sceAppMgrGetRunningAppIdListForShell needs SceAppMgrUser
 * which silently breaks plugin load in the SceShell context. Instead
 * we read the TitleID written by the companion *ALL plugin
 * (see src/companion/companion_main.c). */

#define CURRENT_TITLE_PATH "ux0:/data/ps-vita-mqtt/current_title.txt"
#define TITLES_PATH        "ux0:/data/ps-vita-mqtt/titles.txt"

static void lookup_game_name(const char *title_id, char *out, size_t cap) {
    out[0] = '\0';
    SceUID fd = sceIoOpen(TITLES_PATH, SCE_O_RDONLY, 0);
    if (fd < 0) return;
    char buf[4096];
    int n = sceIoRead(fd, buf, sizeof buf - 1);
    sceIoClose(fd);
    if (n <= 0) return;
    buf[n] = '\0';

    size_t tid_len = 0; while (title_id[tid_len]) tid_len++;
    char *line = buf;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *comma = strchr(line, ',');
        if (comma && (size_t)(comma - line) == tid_len
            && strncmp(line, title_id, tid_len) == 0) {
            size_t i = 0;
            for (const char *p = comma + 1; *p && i < cap - 1; p++, i++) {
                out[i] = *p;
            }
            out[i] = '\0';
            return;
        }
        line = nl ? nl + 1 : NULL;
    }
}

int collector_app(app_state *out) {
    out->in_game = 0;
    out->title_id[0] = '\0';
    out->game_name[0] = '\0';

    SceUID fd = sceIoOpen(CURRENT_TITLE_PATH, SCE_O_RDONLY, 0);
    if (fd < 0) return 0;
    char buf[16];
    sceClibMemset(buf, 0, sizeof buf);
    int n = sceIoRead(fd, buf, sizeof buf - 1);
    sceIoClose(fd);
    if (n <= 0) return 0;

    /* Trim trailing whitespace */
    for (int i = n - 1; i >= 0; i--) {
        if (buf[i] == '\n' || buf[i] == '\r' || buf[i] == ' '
            || buf[i] == '\t' || buf[i] == '\0') {
            buf[i] = '\0';
        } else {
            break;
        }
    }
    if (buf[0] == '\0') return 0;

    out->in_game = 1;
    strncpy(out->title_id, buf, sizeof out->title_id - 1);
    out->title_id[sizeof out->title_id - 1] = '\0';

    lookup_game_name(out->title_id, out->game_name, sizeof out->game_name);
    return 0;
}
