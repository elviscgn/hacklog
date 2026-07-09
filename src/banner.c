#include "banner.h"
#include <string.h>

/*
 * Block-letter "hacklog" using █▓░ characters.
 * Baked in as a string constant — no external figlet dependency.
 */

static const char *banner_big =
    "    __                  __      __               \n"
    "   / /_   ____ _ _____ / /__   / /____   ____ _  \n"
    "  / __ \\ / __ `// ___// //_/  / // __ \\ / __ `/  \n"
    " / / / // /_/ // /__ / ,<    / // /_/ // /_/ /   \n"
    "/_/ /_/ \\__,_/ \\___//_/|_|  /_/ \\____/ \\__, /    \n"
    "                                      /____/     \n";

static const char *banner_small = "[ hacklog ]";

const char *banner_get(void) {
    return banner_big;
}

const char *banner_get_small(void) {
    return banner_small;
}

int banner_height(void) {
    return 6;
}

int banner_width(void) {
    return 49;
}
