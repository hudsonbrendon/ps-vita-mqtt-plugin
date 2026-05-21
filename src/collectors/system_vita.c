#include "collectors.h"
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <stdio.h>
#include <string.h>

/* Local shim that matches Sony's SceKernelSystemSwVersion layout.
 * VitaSDK's struct definition is partly hidden across releases, so
 * we cast our own through the SDK's pointer type. */
typedef struct {
    unsigned int size;             /* set to sizeof(self) before the call */
    char         versionString[28];
    unsigned int version;          /* BCD: e.g. 0x03740011 → "3.74" */
    unsigned int reserved;
} psvita_sw_version;

int collector_system(system_state *out, uint64_t plugin_started_at_sec) {
    psvita_sw_version info = { .size = sizeof info };
    if (sceKernelGetSystemSwVersion((SceKernelSystemSwVersion *)&info) == 0
        && info.versionString[0]) {
        snprintf(out->firmware, sizeof out->firmware, "%X.%02X",
                 (info.version >> 24) & 0xFF, (info.version >> 16) & 0xFF);
    } else {
        strcpy(out->firmware, "unknown");
    }

    int model = sceKernelGetModelForCDialog();
    switch (model >> 16) {
        case 0x01: strcpy(out->model, "PCH-1000"); break;
        case 0x02: strcpy(out->model, "PCH-2000"); break;
        case 0x03: strcpy(out->model, "PSTV");     break;
        default:   strcpy(out->model, "Unknown");  break;
    }

    SceUInt64 us = sceKernelGetProcessTimeWide();
    /* Process time is per-process; an absolute system uptime needs
     * sceRtcGetCurrentTick + boot-time delta. MVP uses process time
     * which approximates "time since SceShell started". */
    out->system_uptime_sec = (uint64_t)(us / 1000000ULL);
    out->plugin_uptime_sec = out->system_uptime_sec >= plugin_started_at_sec
        ? out->system_uptime_sec - plugin_started_at_sec : 0;
    return 0;
}
