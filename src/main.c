#include "config.h"
#include "publisher.h"
#include "log.h"
#include "../third_party/cJSON/cJSON.h"
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/clib.h>

#define CONFIG_PATH "ux0:/data/ps-vita-mqtt/config.json"

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

static int worker(SceSize args, void *argp) {
    (void)args; (void)argp;

    /* Give SceShell ~5 s to finish its own boot before we touch any
     * sub-system. module_start runs synchronously during shell init —
     * any blocking I/O there can race with shell startup and stall the
     * UI. The thread is async, so delays here are safe. */
    sceKernelDelayThread(5 * 1000000);

    LOGF("worker thread entered (post-boot delay)");

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
    /* Keep module_start trivial: just spawn the worker and return fast.
     * Don't touch FS / network here — SceShell is still booting. */
    g_thid = sceKernelCreateThread("ps-vita-mqtt", worker,
                                   0x60, 0x10000, 0, 0, NULL);
    if (g_thid < 0) return 0;
    sceKernelStartThread(g_thid, 0, NULL);
    return 0;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc; (void)args;
    g_stop = 1;
    if (g_thid >= 0) {
        sceKernelWaitThreadEnd(g_thid, NULL, NULL);
        sceKernelDeleteThread(g_thid);
        g_thid = -1;
    }
    return 0;
}
