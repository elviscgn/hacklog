#ifndef HACKLOG_COMMANDS_H
#define HACKLOG_COMMANDS_H

#include "data.h"
#include "currency.h"

/* Result of a command execution */
typedef struct {
    int success;           /* 0 = success, -1 = error */
    char message[512];     /* feedback message for the user */
} CmdResult;

/* Undo state — stores one previous snapshot */
typedef struct {
    HackLog snapshot;
    int has_snapshot;
    char description[256]; /* what the last action was, for undo feedback */
} UndoState;

/* Command context — everything a command needs */
typedef struct {
    HackLog   *log;
    RateTable *rates;
    UndoState *undo;
    char      *profile_name;  /* current profile name (mutable for /profile switch) */
    int        needs_save;     /* set to 1 if the log was modified */
    int        needs_reload;   /* set to 1 if profile was switched */
    int        confirm_delete; /* internal: 1 if waiting for delete confirmation */
    char       pending_delete[MAX_NAME_LEN]; /* name pending deletion */
} CmdContext;

/* Main dispatcher — parses command string and executes.
   Works for both shell args (argc/argv) and TUI input (parsed to tokens).
   Returns result with success/failure and feedback message. */
CmdResult cmd_dispatch(int argc, char **argv, CmdContext *ctx);

/* Parse a raw input string into argc/argv tokens (handles quoted strings).
   Caller must free the returned argv array and its strings. */
int tokenize_command(const char *input, char ***argv_out);

/* Individual command handlers (also callable directly) */
CmdResult cmd_add(int argc, char **argv, CmdContext *ctx);
CmdResult cmd_win(int argc, char **argv, CmdContext *ctx);
CmdResult cmd_lose(int argc, char **argv, CmdContext *ctx);
CmdResult cmd_status(int argc, char **argv, CmdContext *ctx);
CmdResult cmd_edit(int argc, char **argv, CmdContext *ctx);
CmdResult cmd_delete(int argc, char **argv, CmdContext *ctx);
CmdResult cmd_undo(CmdContext *ctx);
CmdResult cmd_list(CmdContext *ctx);
CmdResult cmd_rate(int argc, char **argv, CmdContext *ctx);
CmdResult cmd_profile(int argc, char **argv, CmdContext *ctx);

/* Free tokenized argv */
void free_tokens(int argc, char **argv);

#endif /* HACKLOG_COMMANDS_H */
