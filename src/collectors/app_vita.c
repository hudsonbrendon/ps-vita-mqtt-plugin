#include "collectors.h"
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/iofilemgr.h>
#include <string.h>
#include <stdio.h>

/* Walks /sce_pfs/... and /sce_module to figure out the currently-foreground
 * SceShellSvc-managed application. The standard kernel-side way is
 * ksceKernelGetProcessTitleId(SceUID pid, char *titleid). We discover the
 * foreground PID via ksceKernelGetProcessForPid(SCE_KERNEL_PROCESS_FOREGROUND).
 * Some VitaSDK versions expose ksceAppMgrGetForegroundPid in addition. */

extern int ksceKernelGetProcessTitleId(int pid, char *titleid);
extern int ksceKernelGetForegroundPid(void);

static int load_title_name(const char *title_id, char *out, size_t cap) {
    /* titles.txt is bundled inside the .skprx via vita-pack-vpk's romfs
     * mechanism — at runtime we read it from a known path on ux0:. */
    int fd = ksceIoOpen("ux0:data/ps-vita-mqtt/titles.txt", SCE_O_RDONLY, 0);
    if (fd < 0) return -1;
    char buf[4096];
    int n = ksceIoRead(fd, buf, sizeof buf - 1);
    ksceIoClose(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    char *line = buf;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *comma = strchr(line, ',');
        if (comma && strncmp(line, title_id, comma - line) == 0
                  && (size_t)(comma - line) == strlen(title_id)) {
            strncpy(out, comma + 1, cap - 1);
            out[cap - 1] = '\0';
            return 0;
        }
        line = nl ? nl + 1 : NULL;
    }
    return -1;
}

int collector_app(app_state *out) {
    out->in_game = 0;
    out->title_id[0] = '\0';
    out->game_name[0] = '\0';

    int pid = ksceKernelGetForegroundPid();
    if (pid <= 0) return 0;

    char tid[16] = {0};
    if (ksceKernelGetProcessTitleId(pid, tid) < 0) return 0;
    /* PS Vita shell process is NPXS10015; ignore it. */
    if (strncmp(tid, "NPXS", 4) == 0) return 0;

    strncpy(out->title_id, tid, sizeof out->title_id - 1);
    out->in_game = 1;
    (void)load_title_name(out->title_id, out->game_name, sizeof out->game_name);
    return 0;
}
