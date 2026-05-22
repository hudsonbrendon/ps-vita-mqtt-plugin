/* ps-vita-mqtt-tag — tiny companion plugin loaded into every process
 * via taiHEN's *ALL section. On module_start it reads its own process
 * TitleID and writes it to ux0:/data/ps-vita-mqtt/current_title.txt
 * so the main publisher plugin (which lives in SceShell) can publish
 * the foreground game's TitleID even while a game is running.
 *
 * Skips system processes (NPXS*, SceShell etc.) — only real game/app
 * TitleIDs (PCS{A,B,E,F}…) are written. */

#include <psp2/appmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h>

#define CURRENT_TITLE_PATH "ux0:/data/ps-vita-mqtt/current_title.txt"
#define COMPANION_LOG_PATH "ux0:/data/ps-vita-mqtt/companion.log"
#define PARAM_TITLE_ID     12     /* SCE_APPMGR_APP_PARAM_TITLE_ID */

static void log_append(const char *s) {
    SceUID fd = sceIoOpen(COMPANION_LOG_PATH,
                          SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0666);
    if (fd >= 0) {
        int n = 0; while (s[n]) n++;
        sceIoWrite(fd, s, n);
        sceIoWrite(fd, "\n", 1);
        sceIoClose(fd);
    }
}

static int strn_starts_with(const char *s, const char *prefix, int n) {
    for (int i = 0; i < n; i++) {
        if (s[i] != prefix[i]) return 0;
    }
    return 1;
}

int module_start(unsigned int argc, const void *args) {
    (void)argc; (void)args;

    log_append("companion: module_start entered");

    int pid = sceKernelGetCurrentProcess();
    char buf[64];
    sceClibSnprintf(buf, sizeof buf, "companion: pid=0x%X", pid);
    log_append(buf);

    char titleid[16];
    sceClibMemset(titleid, 0, sizeof titleid);

    int rc = sceAppMgrAppParamGetString(pid, PARAM_TITLE_ID,
                                        titleid, sizeof titleid - 1);
    sceClibSnprintf(buf, sizeof buf,
                    "companion: AppParamGetString rc=0x%X titleid='%s'",
                    rc, titleid);
    log_append(buf);

    if (rc < 0 || titleid[0] == '\0') {
        log_append("companion: skip — no titleid");
        return 0;
    }

    /* Skip only well-known system shells; write everything else
     * (commercial games, homebrew, PSP via Adrenaline, demos…). */
    if (strn_starts_with(titleid, "NPXS", 4)
        || strn_starts_with(titleid, "VITASHELL", 9)
        || strn_starts_with(titleid, "MLCL", 4)) {
        sceClibSnprintf(buf, sizeof buf,
                        "companion: skip — system: '%s'", titleid);
        log_append(buf);
        return 0;
    }

    SceUID fd = sceIoOpen(CURRENT_TITLE_PATH,
                          SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (fd < 0) {
        sceClibSnprintf(buf, sizeof buf,
                        "companion: open current_title failed 0x%X", fd);
        log_append(buf);
        return 0;
    }
    int n = 0; while (titleid[n]) n++;
    sceIoWrite(fd, titleid, n);
    sceIoClose(fd);
    sceClibSnprintf(buf, sizeof buf,
                    "companion: WROTE titleid='%s'", titleid);
    log_append(buf);
    return 0;
}

int module_stop(unsigned int argc, const void *args) {
    (void)argc; (void)args;

    int pid = sceKernelGetCurrentProcess();
    char titleid[16];
    sceClibMemset(titleid, 0, sizeof titleid);
    if (sceAppMgrAppParamGetString(pid, PARAM_TITLE_ID,
                                   titleid, sizeof titleid - 1) < 0
        || strn_starts_with(titleid, "NPXS", 4)
        || strn_starts_with(titleid, "VITASHELL", 9)
        || strn_starts_with(titleid, "MLCL", 4)) {
        return 0;
    }

    char buf[64];
    sceClibSnprintf(buf, sizeof buf,
                    "companion: module_stop clearing for '%s'", titleid);
    log_append(buf);

    SceUID fd = sceIoOpen(CURRENT_TITLE_PATH,
                          SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (fd >= 0) sceIoClose(fd);
    return 0;
}
