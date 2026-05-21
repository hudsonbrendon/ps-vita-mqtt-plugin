#include "collectors.h"
#include <string.h>

int collector_app(app_state *out) {
    out->in_game = 1;
    strcpy(out->title_id, "PCSE00001");
    strcpy(out->game_name, "Test Game");
    return 0;
}
