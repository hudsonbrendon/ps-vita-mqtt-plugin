#include "collectors.h"

int collector_battery(battery_state *out) {
    out->level_pct = 87;
    out->charging = 0;
    out->temp_celsius_x10 = 305;
    out->remaining_minutes = 240;
    return 0;
}
