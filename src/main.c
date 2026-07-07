#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "data.h"
#include "storage.h"
#include "currency.h"
#include "commands.h"

static void print_usage(void) {
    printf("Usage: hack [command] [args]\n\n");
    printf("Commands:\n");
    printf("  add \"name\" --deadline YYYY-MM-DD   Add a new hackathon\n");
    printf("  win \"name\" [--prize $2500]          Mark as won with optional prize\n");
    printf("  lose \"name\"                         Mark as lost\n");
    printf("  status \"name\" <status>              Set status directly\n");
    printf("  edit \"name\" [--deadline] [--notes]  Edit an entry\n");
    printf("  delete \"name\"                       Delete an entry\n");
    printf("  undo                                Undo last action\n");
    printf("  list                                List all entries\n");
    printf("  rate <CODE> <value>                 Set currency rate\n");
    printf("  cal                                 Print calendar\n");
    printf("\nOptions:\n");
    printf("  --profile <name>                    Use a specific profile\n");
    printf("\nRun with no args to launch the TUI dashboard.\n");
}

/* Forward declaration — implemented in tui.c when it exists */
extern int tui_run(const char *profile_name);

/* Forward declaration — calendar printing */
extern void calendar_print_static(const HackLog *log);

int main(int argc, char *argv[]) {
    /* Initialize hacklog directory structure */
    hacklog_dir_init();

    /* Seed demo profile with fabricated data if it doesn't exist */
    hacklog_seed_demo();

    /* Parse --profile flag */
    char profile_name[64] = "default";
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--profile") == 0) {
            strncpy(profile_name, argv[i + 1], sizeof(profile_name) - 1);
            /* Remove the flag from args so command dispatch doesn't see it */
            for (int j = i; j < argc - 2; j++) {
                argv[j] = argv[j + 2];
            }
            argc -= 2;
            break;
        }
    }

    /* No command = launch TUI */
    if (argc <= 1) {
        return tui_run(profile_name);
    }

    /* Help */
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage();
        return 0;
    }

    /* Load data */
    HackLog log;
    RateTable rates;
    UndoState undo = { .has_snapshot = 0 };

    hacklog_load(profile_name, &log);
    load_rates(&rates);

    /* Set up command context */
    CmdContext ctx = {
        .log = &log,
        .rates = &rates,
        .undo = &undo,
        .profile_name = profile_name,
        .needs_save = 0,
        .needs_reload = 0,
        .confirm_delete = 0,
    };

    /* Special case: cal command */
    if (strcasecmp(argv[1], "cal") == 0) {
        calendar_print_static(&log);
        return 0;
    }

    /* Dispatch command (argv+1 skips program name) */
    CmdResult result = cmd_dispatch(argc - 1, argv + 1, &ctx);

    /* Print result message */
    if (result.message[0]) {
        printf("%s\n", result.message);
    }

    /* Save if modified */
    if (ctx.needs_save) {
        hacklog_save(profile_name, &log);
    }

    return (result.success == 0) ? 0 : 1;
}
