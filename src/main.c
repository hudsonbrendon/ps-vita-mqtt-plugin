#include "config.h"
#include "publisher.h"
#include "log.h"
#include <psp2/kernel/threadmgr.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>

#define CONFIG_PATH "ux0:data/ps-vita-mqtt/config.json"
#define NET_HEAP_SIZE (256 * 1024)

static volatile int g_stop = 0;
static SceUID       g_thid = -1;
static mqtt_config  g_cfg;
static char         g_net_heap[NET_HEAP_SIZE];

static int worker(SceSize args, void *argp) {
    (void)args; (void)argp;
    LOGF("worker thread started");
    publisher_run(&g_cfg, &g_stop);
    LOGF("worker thread exiting");
    return 0;
}

int module_start(SceSize argc, const void *args) {
    (void)argc; (void)args;
    LOGF("ps-vita-mqtt module_start");
    if (config_load_from_path(CONFIG_PATH, &g_cfg) < 0) {
        LOGE("config load failed: %s — plugin idle", CONFIG_PATH);
        return 0;
    }
    LOGF("config ok: broker=%s:%u client=%s",
         g_cfg.broker_host, g_cfg.broker_port, g_cfg.client_id);

    /* sceNet user-mode requires an init with a memory pool. SceShell
     * has already initialised the stack, but plugins loaded into a
     * fresh process need their own pool. Try to init; ignore EALREADY. */
    SceNetInitParam net_init = {
        .memory = g_net_heap,
        .size   = NET_HEAP_SIZE,
        .flags  = 0,
    };
    int ni = sceNetInit(&net_init);
    if (ni < 0 && ni != (int)0x80410101 /* SCE_NET_ERROR_ENOTINIT alias */) {
        LOGE("sceNetInit warn: 0x%X (continuing)", ni);
    }
    sceNetCtlInit();

    g_thid = sceKernelCreateThread("ps-vita-mqtt", worker,
                                   0x40, 0x4000, 0, 0, NULL);
    if (g_thid < 0) { LOGE("create thread failed: 0x%X", g_thid); return 0; }
    sceKernelStartThread(g_thid, 0, NULL);
    return 0;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc; (void)args;
    LOGF("module_stop");
    g_stop = 1;
    if (g_thid >= 0) {
        sceKernelWaitThreadEnd(g_thid, NULL, NULL);
        sceKernelDeleteThread(g_thid);
        g_thid = -1;
    }
    sceNetCtlTerm();
    sceNetTerm();
    return 0;
}
