#include "collectors.h"
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/utils.h>
#include <stdio.h>
#include <string.h>

/* External helper: vita-headers exposes ksceKernelGetSystemSwVersion
 * via SceSysmemForDriver. Different vita-headers versions name it
 * slightly differently; if your build complains, switch to the
 * sysctl-style read of SCE_SYSTEM_SW_VERSION described in the
 * vita-headers SCE_KERNEL_GET_SYSTEM_SW_VERSION_*. */
extern int ksceKernelGetSystemSwVersion(unsigned int *version);
extern int ksceKernelGetModelForCDialog(void);

int collector_system(system_state *out, uint64_t plugin_started_at_sec) {
    unsigned int v = 0;
    if (ksceKernelGetSystemSwVersion(&v) == 0) {
        /* v is BCD: 0x03740011 → "3.74" (high two nibbles + next two). */
        snprintf(out->firmware, sizeof out->firmware, "%X.%02X",
                 (v >> 24) & 0xFF, (v >> 16) & 0xFF);
    } else {
        strcpy(out->firmware, "unknown");
    }

    int model = ksceKernelGetModelForCDialog();
    /* 0x10000 = PCH-1000 family, 0x20000 = PCH-2000, 0x30000 = PSTV */
    switch (model >> 16) {
        case 0x01: strcpy(out->model, "PCH-1000"); break;
        case 0x02: strcpy(out->model, "PCH-2000"); break;
        case 0x03: strcpy(out->model, "PSTV");     break;
        default:   strcpy(out->model, "Unknown");  break;
    }

    SceUInt64 now = ksceKernelGetSystemTimeWide();      /* microseconds */
    out->system_uptime_sec = (uint64_t)(now / 1000000ULL);
    out->plugin_uptime_sec = out->system_uptime_sec >= plugin_started_at_sec
        ? out->system_uptime_sec - plugin_started_at_sec : 0;
    return 0;
}
