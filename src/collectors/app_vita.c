#include "collectors.h"
#include <string.h>

/* MVP: SceShell-side detection of "is a game running?" needs either
 * sceAppMgrGetRunningAppIdListForShell (lib SceAppMgrUser, not always
 * resolvable in plugin context) or a taiHEN hook into shell internals.
 * Stubbed for now — always reports "no app". */
int collector_app(app_state *out) {
    out->in_game = 0;
    out->title_id[0] = '\0';
    out->game_name[0] = '\0';
    (void)out;
    return 0;
}
