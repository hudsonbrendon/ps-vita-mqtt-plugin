#include "config.h"
#include "publisher.h"
#include "log.h"
#include "../third_party/cJSON/cJSON.h"
#include <psp2/io/fcntl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/clib.h>
#include <taihen.h>

#define CONFIG_PATH         "ux0:/data/ps-vita-mqtt/config.json"
#define CURRENT_TITLE_PATH  "ux0:/data/ps-vita-mqtt/current_title.txt"

/* Bump allocator for cJSON. */
static char cjson_pool[16 * 1024];
static size_t cjson_pool_pos = 0;
static void *cjson_malloc(size_t n) {
    cjson_pool_pos = (cjson_pool_pos + 7) & ~(size_t)7;
    if (cjson_pool_pos + n > sizeof cjson_pool) return 0;
    void *p = &cjson_pool[cjson_pool_pos];
    cjson_pool_pos += n;
    return p;
}
static void cjson_free(void *p) { (void)p; }

static volatile int g_stop = 0;
static SceUID       g_thid = -1;
static mqtt_config  g_cfg;

/* ---- taiHEN hooks on SceShell game-launch entry points -------------- */
static SceUID         g_launch_hook_id    = -1;
static tai_hook_ref_t g_launch_hook_ref   = 0;
static SceUID         g_launch2_hook_id   = -1;
static tai_hook_ref_t g_launch2_hook_ref  = 0;
static SceUID         g_launchsh_hook_id  = -1;
static tai_hook_ref_t g_launchsh_hook_ref = 0;

static void write_current_title(const char *tid, int len) {
    SceUID fd = sceIoOpen(CURRENT_TITLE_PATH,
                          SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (fd >= 0) {
        sceIoWrite(fd, tid, len);
        sceIoClose(fd);
    }
}

static void capture_titleid_from_uri(const char *uri) {
    if (!uri) return;
    LOGF("hook URI=%s", uri);
    const char *p = uri;
    while (*p) {
        if (p[0] == 't' && p[1] == 'i' && p[2] == 't' && p[3] == 'l'
         && p[4] == 'e' && p[5] == 'i' && p[6] == 'd' && p[7] == '=') {
            p += 8;
            const char *start = p;
            while (*p && *p != '&') p++;
            int len = (int)(p - start);
            if (len > 0 && len < 16) {
                LOGF("hook captured titleid=%.*s", len, start);
                write_current_title(start, len);
            }
            return;
        }
        p++;
    }
}

static void capture_titleid_direct(const char *name) {
    if (!name || !name[0]) return;
    LOGF("hook NAME=%s", name);
    int len = 0;
    while (name[len] && name[len] != '&' && len < 15) len++;
    if (len > 0) write_current_title(name, len);
}

static int hook_launch_app_by_uri(int flags, const char *uri) {
    capture_titleid_from_uri(uri);
    return ((int (*)(int, const char *))g_launch_hook_ref)(flags, uri);
}

static int hook_launch_app_by_uri2(int flags, const char *uri, void *opt) {
    capture_titleid_from_uri(uri);
    return ((int (*)(int, const char *, void *))g_launch2_hook_ref)(flags, uri, opt);
}

static int hook_launch_app_by_name_for_shell(const char *name, const char *param, void *opt) {
    capture_titleid_direct(name);
    return ((int (*)(const char *, const char *, void *))g_launchsh_hook_ref)(name, param, opt);
}

static void install_hooks(void) {
    g_launch_hook_id = taiHookFunctionExport(
        &g_launch_hook_ref,
        "SceDriverUser", 0xA6605D6F,
        0x003C634F,                                  /* LaunchAppByUri */
        hook_launch_app_by_uri);
    g_launch2_hook_id = taiHookFunctionExport(
        &g_launch2_hook_ref,
        "SceDriverUser", 0xA6605D6F,
        0x0ED6AF54,                                  /* LaunchAppByUri2 */
        hook_launch_app_by_uri2);
    g_launchsh_hook_id = taiHookFunctionExport(
        &g_launchsh_hook_ref,
        "SceDriverUser", 0xA6605D6F,
        0xA1D43805,                                  /* LaunchAppByNameForShell */
        hook_launch_app_by_name_for_shell);
}

static int worker(SceSize args, void *argp) {
    (void)args; (void)argp;

    /* Wait so SceShell finishes its own startup before we hook anything. */
    sceKernelDelayThread(5 * 1000000);
    LOGF("worker thread entered (post-boot delay)");

    /* Install the launch hooks now that SceShell is past its init. */
    install_hooks();
    LOGF("worker: hooks uri=0x%X uri2=0x%X namesh=0x%X",
         g_launch_hook_id, g_launch2_hook_id, g_launchsh_hook_id);

    cJSON_Hooks hooks = { cjson_malloc, cjson_free };
    cJSON_InitHooks(&hooks);
    cjson_pool_pos = 0;

    int rc = config_load_from_path(CONFIG_PATH, &g_cfg);
    LOGF("worker: config rc=%d", rc);
    if (rc < 0) {
        LOGE("config load failed -- plugin idle");
        return 0;
    }
    LOGF("worker: cfg broker=%s:%u client=%s",
         g_cfg.broker_host, g_cfg.broker_port, g_cfg.client_id);

    publisher_run(&g_cfg, &g_stop);
    LOGF("worker thread exiting");
    return 0;
}

int module_start(SceSize argc, const void *args) {
    (void)argc; (void)args;
    g_thid = sceKernelCreateThread("ps-vita-mqtt", worker,
                                   0x60, 0x10000, 0, 0, NULL);
    if (g_thid < 0) return 0;
    sceKernelStartThread(g_thid, 0, NULL);
    return 0;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc; (void)args;
    g_stop = 1;
    if (g_launch_hook_id    >= 0) taiHookRelease(g_launch_hook_id,    g_launch_hook_ref);
    if (g_launch2_hook_id   >= 0) taiHookRelease(g_launch2_hook_id,   g_launch2_hook_ref);
    if (g_launchsh_hook_id  >= 0) taiHookRelease(g_launchsh_hook_id,  g_launchsh_hook_ref);
    g_launch_hook_id = g_launch2_hook_id = g_launchsh_hook_id = -1;
    if (g_thid >= 0) {
        sceKernelWaitThreadEnd(g_thid, NULL, NULL);
        sceKernelDeleteThread(g_thid);
        g_thid = -1;
    }
    return 0;
}
