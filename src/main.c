#include <stdio.h>
#include <string.h>
#include "data.h"
#include "storage.h"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Initialize hacklog directory structure */
    hacklog_dir_init();

    printf("hacklog — run with no args for TUI (coming soon)\n");
    return 0;
}
