#include "collectors.h"
#include <psp2/appmgr.h>
#include <psp2/types.h>
#include <string.h>

/* Detect whether a user app (a game) is running alongside SceShell.
 *
 * sceAppMgrGetRunningAppIdListForShell returns the count of currently
 * running shell-managed apps and fills `app_ids` with their internal
 * IDs (NOT title IDs). Converting an app ID into a TitleID from the
 * SceShell user-mode context is not exposed by VitaSDK in a stable
 * way — known approaches require taiHEN hooks into SceShell internals
 * and are deferred to roadmap (see README "Limitations").
 *
 * MVP behavior: in_game = (count > 0). title_id and game_name stay
 * empty; the lookup against ux0:data/ps-vita-mqtt/titles.txt also
 * happens only when a titleid string is known. */

int collector_app(app_state *out) {
    out->in_game = 0;
    out->title_id[0] = '\0';
    out->game_name[0] = '\0';

    SceInt32 ids[8] = {0};
    int n = sceAppMgrGetRunningAppIdListForShell(ids, 8);
    if (n > 0) {
        out->in_game = 1;
    }
    return 0;
}
