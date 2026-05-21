#include "collectors.h"
#include <string.h>

int collector_system(system_state *out, uint64_t plugin_started_at_sec) {
    (void)plugin_started_at_sec;
    strcpy(out->firmware, "3.74");
    strcpy(out->model, "PCH-2000");
    out->system_uptime_sec = 12345;
    out->plugin_uptime_sec = 1234;
    return 0;
}
