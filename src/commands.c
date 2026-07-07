#include "commands.h"
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <ctype.h>

/* ── helpers ──────────────────────────────────────────────── */

static CmdResult make_result(int success, const char *fmt, ...) {
    CmdResult r;
    r.success = success;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r.message, sizeof(r.message), fmt, ap);
    va_end(ap);
    return r;
}

static void save_undo_snapshot(CmdContext *ctx, const char *desc) {
    if (!ctx || !ctx->undo || !ctx->log) return;
    ctx->undo->snapshot = *ctx->log;
    ctx->undo->has_snapshot = 1;
    strncpy(ctx->undo->description, desc, sizeof(ctx->undo->description) - 1);
}

/* Find a --flag value in argv. Returns the value string or NULL. */
static const char *find_flag(int argc, char **argv, const char *flag) {
    for (int i = 0; i < argc - 1; i++) {
        if (strcmp(argv[i], flag) == 0) {
            return argv[i + 1];
        }
    }
    return NULL;
}

/* ── tokenizer ────────────────────────────────────────────── */

int tokenize_command(const char *input, char ***argv_out) {
    if (!input || !argv_out) return 0;

    /* Allocate space for up to 32 tokens */
    char **tokens = calloc(32, sizeof(char *));
    if (!tokens) return 0;

    int count = 0;
    const char *p = input;

    /* Skip leading slash if present (TUI command) */
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '/') p++;

    while (*p && count < 32) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        char token[512];
        int ti = 0;

        if (*p == '"') {
            /* Quoted string — read until closing quote */
            p++;
            while (*p && *p != '"' && ti < 511) {
                token[ti++] = *p++;
            }
            if (*p == '"') p++;
        } else {
            /* Unquoted — read until whitespace */
            while (*p && !isspace((unsigned char)*p) && ti < 511) {
                token[ti++] = *p++;
            }
        }
        token[ti] = '\0';

        tokens[count] = strdup(token);
        count++;
    }

    *argv_out = tokens;
    return count;
}

void free_tokens(int argc, char **argv) {
    if (!argv) return;
    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
}

/* ── individual commands ──────────────────────────────────── */

CmdResult cmd_add(int argc, char **argv, CmdContext *ctx) {
    if (argc < 2) {
        return make_result(-1, "Usage: add \"name\" --deadline YYYY-MM-DD [--notes \"...\"]");
    }

    const char *name = argv[1];

    /* Check if entry already exists */
    if (find_entry_by_name(ctx->log, name) >= 0) {
        return make_result(-1, "Entry \"%s\" already exists", name);
    }

    if (ctx->log->count >= MAX_ENTRIES) {
        return make_result(-1, "Maximum entries reached");
    }

    save_undo_snapshot(ctx, "add entry");

    HackEntry *e = &ctx->log->entries[ctx->log->count];
    memset(e, 0, sizeof(HackEntry));
    strncpy(e->name, name, MAX_NAME_LEN - 1);
    e->status = STATUS_APPLIED;

    /* Look for deadline — either as --deadline flag or positional arg 2 */
    const char *deadline = find_flag(argc, argv, "--deadline");
    if (!deadline && argc > 2 && is_valid_date(argv[2])) {
        deadline = argv[2]; /* positional shorthand */
    }
    if (deadline && is_valid_date(deadline)) {
        strncpy(e->deadline, deadline, 10);
    } else if (deadline) {
        return make_result(-1, "Invalid date format: %s (use YYYY-MM-DD)", deadline);
    }

    /* Optional notes */
    const char *notes = find_flag(argc, argv, "--notes");
    if (notes) {
        strncpy(e->notes, notes, MAX_NOTES_LEN - 1);
    }

    ctx->log->count++;
    ctx->needs_save = 1;
    sort_entries_by_deadline(ctx->log);

    return make_result(0, "Added \"%s\" (applied)", name);
}

CmdResult cmd_win(int argc, char **argv, CmdContext *ctx) {
    if (argc < 2) {
        return make_result(-1, "Usage: win \"name\" [--prize $2500]");
    }

    const char *name = argv[1];
    int idx = find_entry_by_name(ctx->log, name);
    if (idx < 0) {
        return make_result(-1, "No entry found: \"%s\"", name);
    }

    save_undo_snapshot(ctx, "mark win");

    ctx->log->entries[idx].status = STATUS_WON;

    /* Parse prize if provided */
    const char *prize_str = find_flag(argc, argv, "--prize");
    if (prize_str) {
        double amount;
        char currency[MAX_CURRENCY_LEN];
        if (parse_prize(prize_str, &amount, currency) == 0) {
            ctx->log->entries[idx].prize_amount = amount;
            strncpy(ctx->log->entries[idx].prize_currency, currency, MAX_CURRENCY_LEN - 1);
            ctx->log->entries[idx].prize_zar = convert_to_zar(amount, currency, ctx->rates);
        } else {
            return make_result(-1, "Could not parse prize: \"%s\"", prize_str);
        }
    }

    ctx->needs_save = 1;
    return make_result(0, "Marked \"%s\" as won", name);
}

CmdResult cmd_lose(int argc, char **argv, CmdContext *ctx) {
    if (argc < 2) {
        return make_result(-1, "Usage: lose \"name\"");
    }

    const char *name = argv[1];
    int idx = find_entry_by_name(ctx->log, name);
    if (idx < 0) {
        return make_result(-1, "No entry found: \"%s\"", name);
    }

    save_undo_snapshot(ctx, "mark loss");
    ctx->log->entries[idx].status = STATUS_LOST;
    ctx->needs_save = 1;
    return make_result(0, "Marked \"%s\" as lost", name);
}

CmdResult cmd_status(int argc, char **argv, CmdContext *ctx) {
    if (argc < 3) {
        return make_result(-1, "Usage: status \"name\" <status>");
    }

    const char *name = argv[1];
    const char *new_status = argv[2];

    int idx = find_entry_by_name(ctx->log, name);
    if (idx < 0) {
        return make_result(-1, "No entry found: \"%s\"", name);
    }

    HackStatus s = str_to_status(new_status);
    /* Validate it's not the default fallback for an invalid string */
    if (strcasecmp(new_status, "applied") != 0 && s == STATUS_APPLIED) {
        return make_result(-1, "Unknown status: \"%s\"", new_status);
    }

    save_undo_snapshot(ctx, "change status");
    ctx->log->entries[idx].status = s;
    ctx->needs_save = 1;
    return make_result(0, "Set \"%s\" to %s", name, status_to_str(s));
}

CmdResult cmd_edit(int argc, char **argv, CmdContext *ctx) {
    if (argc < 2) {
        return make_result(-1, "Usage: edit \"name\" [--deadline YYYY-MM-DD] [--notes \"...\"] [--name \"new name\"]");
    }

    const char *name = argv[1];
    int idx = find_entry_by_name(ctx->log, name);
    if (idx < 0) {
        return make_result(-1, "No entry found: \"%s\"", name);
    }

    save_undo_snapshot(ctx, "edit entry");
    HackEntry *e = &ctx->log->entries[idx];
    int changed = 0;

    const char *new_deadline = find_flag(argc, argv, "--deadline");
    if (new_deadline) {
        if (!is_valid_date(new_deadline)) {
            return make_result(-1, "Invalid date: %s", new_deadline);
        }
        strncpy(e->deadline, new_deadline, 10);
        changed = 1;
    }

    const char *new_notes = find_flag(argc, argv, "--notes");
    if (new_notes) {
        strncpy(e->notes, new_notes, MAX_NOTES_LEN - 1);
        changed = 1;
    }

    const char *new_name = find_flag(argc, argv, "--name");
    if (new_name) {
        strncpy(e->name, new_name, MAX_NAME_LEN - 1);
        changed = 1;
    }

    const char *new_status = find_flag(argc, argv, "--status");
    if (new_status) {
        HackStatus s = str_to_status(new_status);
        e->status = s;
        changed = 1;
    }

    const char *prize_str = find_flag(argc, argv, "--prize");
    if (prize_str) {
        double amount;
        char currency[MAX_CURRENCY_LEN];
        if (parse_prize(prize_str, &amount, currency) == 0) {
            e->prize_amount = amount;
            strncpy(e->prize_currency, currency, MAX_CURRENCY_LEN - 1);
            e->prize_zar = convert_to_zar(amount, currency, ctx->rates);
            changed = 1;
        }
    }

    if (!changed) {
        return make_result(-1, "Nothing to edit — provide at least one flag");
    }

    ctx->needs_save = 1;
    sort_entries_by_deadline(ctx->log);
    return make_result(0, "Updated \"%s\"", e->name);
}

CmdResult cmd_delete(int argc, char **argv, CmdContext *ctx) {
    if (argc < 2) {
        return make_result(-1, "Usage: delete \"name\"");
    }

    const char *name = argv[1];
    int idx = find_entry_by_name(ctx->log, name);
    if (idx < 0) {
        return make_result(-1, "No entry found: \"%s\"", name);
    }

    /* Check for confirmation (--confirm flag or pending state) */
    const char *confirm = find_flag(argc, argv, "--confirm");
    if (!confirm && !ctx->confirm_delete) {
        /* First call — set pending state */
        ctx->confirm_delete = 1;
        strncpy(ctx->pending_delete, name, MAX_NAME_LEN - 1);
        return make_result(0, "Delete \"%s\"? Run delete again or pass --confirm to confirm", name);
    }

    /* If pending, check it's the same entry */
    if (ctx->confirm_delete && strcasecmp(ctx->pending_delete, name) != 0) {
        ctx->confirm_delete = 0;
        ctx->pending_delete[0] = '\0';
        return make_result(-1, "Delete cancelled (different entry specified)");
    }

    save_undo_snapshot(ctx, "delete entry");

    /* Remove by shifting entries down */
    for (int i = idx; i < ctx->log->count - 1; i++) {
        ctx->log->entries[i] = ctx->log->entries[i + 1];
    }
    ctx->log->count--;

    ctx->confirm_delete = 0;
    ctx->pending_delete[0] = '\0';
    ctx->needs_save = 1;
    return make_result(0, "Deleted \"%s\"", name);
}

CmdResult cmd_undo(CmdContext *ctx) {
    if (!ctx->undo || !ctx->undo->has_snapshot) {
        return make_result(-1, "Nothing to undo");
    }

    *ctx->log = ctx->undo->snapshot;
    ctx->undo->has_snapshot = 0;
    ctx->needs_save = 1;

    char desc[256];
    strncpy(desc, ctx->undo->description, sizeof(desc) - 1);
    ctx->undo->description[0] = '\0';

    return make_result(0, "Undid: %s", desc);
}

CmdResult cmd_list(CmdContext *ctx) {
    /* Print entries to stdout in a script-friendly format */
    if (ctx->log->count == 0) {
        return make_result(0, "No entries in profile \"%s\"", ctx->log->profile_name);
    }

    sort_entries_by_deadline(ctx->log);

    printf("%-30s %-12s %-12s %s\n", "NAME", "DEADLINE", "STATUS", "PRIZE");
    printf("%-30s %-12s %-12s %s\n", "----", "--------", "------", "-----");

    for (int i = 0; i < ctx->log->count; i++) {
        const HackEntry *e = &ctx->log->entries[i];
        char prize_str[64] = "";
        if (e->prize_amount > 0 && e->prize_currency[0]) {
            snprintf(prize_str, sizeof(prize_str), "%s %.0f (R%.0f)",
                     e->prize_currency, e->prize_amount, e->prize_zar);
        }
        printf("%-30s %-12s %-12s %s\n",
               e->name, e->deadline, status_to_str(e->status), prize_str);
    }

    return make_result(0, "Listed %d entries", ctx->log->count);
}

CmdResult cmd_rate(int argc, char **argv, CmdContext *ctx) {
    if (argc < 3) {
        return make_result(-1, "Usage: rate <CURRENCY_CODE> <rate_value>");
    }

    char code[4];
    strncpy(code, argv[1], 3);
    code[3] = '\0';
    for (int i = 0; code[i]; i++) code[i] = toupper((unsigned char)code[i]);

    double rate = atof(argv[2]);
    if (rate <= 0) {
        return make_result(-1, "Rate must be a positive number");
    }

    set_rate(ctx->rates, code, rate);
    save_rates(ctx->rates);

    return make_result(0, "Set %s rate to %.2f ZAR", code, rate);
}

CmdResult cmd_profile(int argc, char **argv, CmdContext *ctx) {
    if (argc < 2) {
        return make_result(-1, "Usage: profile <name>");
    }

    const char *name = argv[1];

    /* If profile doesn't exist, create it */
    if (!hacklog_profile_exists(name)) {
        HackLog empty;
        hacklog_init(&empty);
        strncpy(empty.profile_name, name, sizeof(empty.profile_name) - 1);
        hacklog_save(name, &empty);
    }

    strncpy(ctx->profile_name, name, 63);
    ctx->needs_reload = 1;

    return make_result(0, "Switched to profile \"%s\"", name);
}

/* ── dispatcher ───────────────────────────────────────────── */

CmdResult cmd_dispatch(int argc, char **argv, CmdContext *ctx) {
    if (argc < 1 || !argv || !argv[0]) {
        return make_result(-1, "No command given");
    }

    const char *cmd = argv[0];

    /* Command aliases */
    if (strcmp(cmd, "a") == 0) cmd = "add";
    if (strcmp(cmd, "w") == 0) cmd = "win";
    if (strcmp(cmd, "l") == 0) cmd = "lose";
    if (strcmp(cmd, "d") == 0) cmd = "delete";
    if (strcmp(cmd, "e") == 0) cmd = "edit";
    if (strcmp(cmd, "s") == 0) cmd = "status";

    if (strcasecmp(cmd, "add") == 0) {
        return cmd_add(argc, argv, ctx);
    } else if (strcasecmp(cmd, "win") == 0) {
        return cmd_win(argc, argv, ctx);
    } else if (strcasecmp(cmd, "lose") == 0) {
        return cmd_lose(argc, argv, ctx);
    } else if (strcasecmp(cmd, "status") == 0) {
        return cmd_status(argc, argv, ctx);
    } else if (strcasecmp(cmd, "edit") == 0) {
        return cmd_edit(argc, argv, ctx);
    } else if (strcasecmp(cmd, "delete") == 0) {
        return cmd_delete(argc, argv, ctx);
    } else if (strcasecmp(cmd, "undo") == 0) {
        return cmd_undo(ctx);
    } else if (strcasecmp(cmd, "list") == 0) {
        return cmd_list(ctx);
    } else if (strcasecmp(cmd, "rate") == 0) {
        return cmd_rate(argc, argv, ctx);
    } else if (strcasecmp(cmd, "profile") == 0) {
        return cmd_profile(argc, argv, ctx);
    } else if (strcasecmp(cmd, "cal") == 0) {
        /* Calendar is handled by the TUI layer or main.c directly */
        return make_result(0, "__cal__");
    }

    return make_result(-1, "Unknown command: \"%s\"", cmd);
}
