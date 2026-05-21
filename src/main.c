#include "config.h"
#include "publisher.h"
#include "log.h"
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/sysmem.h>

#define CONFIG_PATH "ux0:data/ps-vita-mqtt/config.json"

static volatile int g_stop = 0;
static SceUID       g_thid = -1;
static mqtt_config  g_cfg;

/* Heap shared with mqtt_socket_vita.c for its small wrapper allocations. */
SceUID g_mqtt_heap = -1;

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
        return SCE_KERNEL_START_SUCCESS;
    }
    LOGF("config ok: broker=%s:%u client=%s",
         g_cfg.broker_host, g_cfg.broker_port, g_cfg.client_id);

    g_mqtt_heap = ksceKernelCreateHeap("ps-vita-mqtt", 0x4000, NULL);
    if (g_mqtt_heap < 0) { LOGE("create heap failed: 0x%X", g_mqtt_heap); return SCE_KERNEL_START_SUCCESS; }

    g_thid = ksceKernelCreateThread("ps-vita-mqtt", worker,
                                    0x40, 0x4000, 0, 0, NULL);
    if (g_thid < 0) { LOGE("create thread failed: 0x%X", g_thid); return SCE_KERNEL_START_SUCCESS; }
    ksceKernelStartThread(g_thid, 0, NULL);
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc; (void)args;
    LOGF("module_stop");
    g_stop = 1;
    if (g_thid >= 0) {
        ksceKernelWaitThreadEnd(g_thid, NULL, NULL);
        ksceKernelDeleteThread(g_thid);
        g_thid = -1;
    }
    if (g_mqtt_heap >= 0) {
        ksceKernelDeleteHeap(g_mqtt_heap);
        g_mqtt_heap = -1;
    }
    return SCE_KERNEL_STOP_SUCCESS;
}
