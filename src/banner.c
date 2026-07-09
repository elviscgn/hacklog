#include "banner.h"
#include <string.h>

static const char *banner_big =
    "                      _       __                \n"
    "  /\\  /\\  __ _   ___ | | __  / /    ___    __ _ \n"
    " / /_/ / / _` | / __|| |/ / / /    / _ \\  / _` |\n"
    "/ __  / | (_| || (__ |   < / /___ | (_) || (_| |\n"
    "\\/ /_/   \\__,_| \\___||_|\\_\\\\____/  \\___/  \\__, |\n"
    "                                          |___/ \n";

static const char *banner_small = "[ hacklog ]";

const char *banner_get(void) { return banner_big; }

const char *banner_get_small(void) { return banner_small; }

int banner_height(void) { return 6; }

int banner_width(void) { return 48; }
