#include "collectors.h"
#include <psp2/power.h>

int collector_battery(battery_state *out) {
    int pct  = scePowerGetBatteryLifePercent();
    int chg  = scePowerIsBatteryCharging();
    int tmpx = scePowerGetBatteryTemp();     /* deci-Celsius */
    int rem  = scePowerGetBatteryLifeTime();

    out->level_pct         = pct  >= 0 ? pct  : -1;
    out->charging          = chg  >= 0 ? chg  : -1;
    out->temp_celsius_x10  = tmpx >= 0 ? tmpx : -1;
    out->remaining_minutes = rem  >= 0 ? rem  : -1;
    return 0;
}
