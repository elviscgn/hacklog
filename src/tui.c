#include "tui.h"
#include "data.h"
#include "storage.h"
#include "currency.h"
#include "commands.h"
#include "banner.h"
#include "calendar.h"
#include <ncurses.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

/* ── color pair definitions ───────────────────────────────── */

enum {
    CP_NORMAL = 0,
    CP_ACCENT = 1,     /* green — banner, selected row, prompt */
    CP_WON,            /* bright green */
    CP_LOST,           /* red */
    CP_DIMMED,         /* grey — rejected, cancelled */
    CP_ACTIVE_STATUS,  /* cyan — applied, active, submitted */
    CP_URGENT_RED,     /* deadline < 48h */
    CP_URGENT_YELLOW,  /* deadline < 1 week */
    CP_HEADER,         /* bold header stats */
    CP_CMDLINE,        /* command line background */
    CP_ERROR,          /* error messages */
    CP_HINT,           /* hint text */
};

static void init_colors(void) {
    start_color();
    use_default_colors();

    init_pair(CP_ACCENT,        COLOR_GREEN,   -1);
    init_pair(CP_WON,           COLOR_GREEN,   -1);
    init_pair(CP_LOST,          COLOR_RED,     -1);
    init_pair(CP_DIMMED,        COLOR_WHITE,   -1);
    init_pair(CP_ACTIVE_STATUS, COLOR_CYAN,    -1);
    init_pair(CP_URGENT_RED,    COLOR_RED,     -1);
    init_pair(CP_URGENT_YELLOW, COLOR_YELLOW,  -1);
    init_pair(CP_HEADER,        COLOR_GREEN,   -1);
    init_pair(CP_CMDLINE,       COLOR_WHITE,   -1);
    init_pair(CP_ERROR,         COLOR_RED,     -1);
    init_pair(CP_HINT,          COLOR_WHITE,   -1);
}

/* ── TUI state ────────────────────────────────────────────── */

typedef struct {
    HackLog     log;
    RateTable   rates;
    UndoState   undo;
    CmdContext  cmd_ctx;
    char        profile_name[64];

    int         selected;       /* currently selected entry index */
    int         scroll_offset;  /* first visible entry index */

    /* Command line state */
    int         cmd_active;     /* 1 when command line is open */
    char        cmd_buf[512];   /* input buffer */
    int         cmd_len;        /* current input length */

    /* Message display */
    char        message[512];   /* status/error message */
    int         msg_is_error;   /* 1 if message is an error */

    /* Layout dimensions (recalculated on resize) */
    int         term_rows;
    int         term_cols;
    int         list_start_row; /* where entry list begins */
    int         list_height;    /* visible entry rows */

    /* Calendar mode */
    int         in_calendar;
} TuiState;

static void tui_load_data(TuiState *st) {
    hacklog_load(st->profile_name, &st->log);
    load_rates(&st->rates);
    sort_entries_by_deadline(&st->log);

    st->cmd_ctx.log = &st->log;
    st->cmd_ctx.rates = &st->rates;
    st->cmd_ctx.undo = &st->undo;
    st->cmd_ctx.profile_name = st->profile_name;
    st->cmd_ctx.needs_save = 0;
    st->cmd_ctx.needs_reload = 0;
    st->cmd_ctx.confirm_delete = 0;
}

/* ── drawing helpers ──────────────────────────────────────── */

static void draw_hline(int row, int col, int width) {
    mvhline(row, col, ACS_HLINE, width);
}

/* Format a number with thousands separators */
static void format_zar(double amount, char *buf, size_t bufsz) {
    int whole = (int)amount;
    if (whole < 1000) {
        snprintf(buf, bufsz, "R%d", whole);
        return;
    }
    /* Build with commas */
    char raw[32];
    snprintf(raw, sizeof(raw), "%d", whole);
    int len = strlen(raw);
    int commas = (len - 1) / 3;
    int total = len + commas;
    if ((size_t)(total + 2) > bufsz) {
        snprintf(buf, bufsz, "R%d", whole);
        return;
    }
    buf[0] = 'R';
    int bi = 1;
    int digits_before_comma = len % 3;
    if (digits_before_comma == 0) digits_before_comma = 3;
    for (int i = 0; i < len; i++) {
        if (i == digits_before_comma && i > 0) {
            buf[bi++] = ',';
            digits_before_comma += 3;
        }
        buf[bi++] = raw[i];
    }
    buf[bi] = '\0';
}

/* Get color pair for an entry's status */
static int status_color(HackStatus s) {
    switch (s) {
        case STATUS_WON:       return CP_WON;
        case STATUS_LOST:      return CP_LOST;
        case STATUS_REJECTED:
        case STATUS_CANCELLED: return CP_DIMMED;
        case STATUS_APPLIED:
        case STATUS_ACTIVE:
        case STATUS_SUBMITTED: return CP_ACTIVE_STATUS;
        default:               return CP_NORMAL;
    }
}

/* Get color pair for deadline urgency */
static int deadline_color(const char *deadline) {
    int days = days_until_deadline(deadline);
    if (days < 0) return CP_DIMMED; /* past */
    if (days < 2) return CP_URGENT_RED;
    if (days < 7) return CP_URGENT_YELLOW;
    return CP_NORMAL;
}

/* ── banner drawing ───────────────────────────────────────── */

static int draw_banner(TuiState *st) {
    int row = 0;
    int cols = st->term_cols;

    if (cols >= banner_width() + 4) {
        /* Big banner fits — render in green */
        const char *b = banner_get();
        const char *line = b;
        attron(COLOR_PAIR(CP_ACCENT) | A_BOLD);
        while (*line) {
            const char *eol = strchr(line, '\n');
            if (!eol) eol = line + strlen(line);
            int len = eol - line;

            /* Center the banner */
            int x = (cols - banner_width()) / 2;
            if (x < 0) x = 0;

            mvaddnstr(row, x, line, len);
            row++;

            if (*eol) line = eol + 1;
            else break;
        }
        attroff(COLOR_PAIR(CP_ACCENT) | A_BOLD);
    } else {
        /* Narrow terminal — small label */
        attron(COLOR_PAIR(CP_ACCENT) | A_BOLD);
        mvprintw(row, (cols - 12) / 2, "%s", banner_get_small());
        attroff(COLOR_PAIR(CP_ACCENT) | A_BOLD);
        row++;
    }

    return row;
}

/* ── stats banner ─────────────────────────────────────────── */

static int draw_stats(TuiState *st, int start_row) {
    int row = start_row;
    int cols = st->term_cols;

    draw_hline(row, 0, cols);
    row++;

    /* Win rate */
    double wr = win_rate(&st->log);
    char wr_str[32];
    if (wr < 0) {
        strcpy(wr_str, "N/A");
    } else {
        snprintf(wr_str, sizeof(wr_str), "%.0f%%", wr * 100);
    }

    /* Total won */
    double total = total_won_zar(&st->log);
    char total_str[32];
    format_zar(total, total_str, sizeof(total_str));

    /* Streak */
    int s = streak_count(&st->log);
    char streak_str[32];
    snprintf(streak_str, sizeof(streak_str), "%d", s);

    /* Count entries by status */
    int n_active = 0;
    for (int i = 0; i < st->log.count; i++) {
        HackStatus hs = st->log.entries[i].status;
        if (hs == STATUS_APPLIED || hs == STATUS_ACTIVE || hs == STATUS_SUBMITTED) {
            n_active++;
        }
    }

    /* Draw stats in a row */
    int x = 2;

    attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvprintw(row, x, "Win Rate: ");
    attroff(A_BOLD);
    printw("%s", wr_str);

    x = cols / 4;
    attron(A_BOLD);
    mvprintw(row, x, "Total Won: ");
    attroff(A_BOLD);
    printw("%s", total_str);

    x = cols / 2;
    attron(A_BOLD);
    mvprintw(row, x, "Streak: ");
    attroff(A_BOLD);
    printw("%s", streak_str);

    x = 3 * cols / 4;
    attron(A_BOLD);
    mvprintw(row, x, "In Progress: ");
    attroff(A_BOLD);
    printw("%d", n_active);

    attroff(COLOR_PAIR(CP_HEADER));

    row++;
    draw_hline(row, 0, cols);
    row++;

    return row;
}

/* ── entry list ───────────────────────────────────────────── */

static void draw_entry_list(TuiState *st, int start_row) {
    st->list_start_row = start_row;
    st->list_height = st->term_rows - start_row - 2; /* leave room for command line */

    int cols = st->term_cols;

    /* Column widths */
    int col_deadline = 2;
    int col_status   = 14;
    int col_name     = 26;
    int col_prize    = cols - 20;
    if (col_prize < col_name + 20) col_prize = col_name + 20;

    /* Header row */
    attron(A_BOLD | COLOR_PAIR(CP_ACCENT));
    mvprintw(start_row, col_deadline, "DEADLINE");
    mvprintw(start_row, col_status, "STATUS");
    mvprintw(start_row, col_name, "NAME");
    if (col_prize < cols - 5) {
        mvprintw(start_row, col_prize, "PRIZE");
    }
    attroff(A_BOLD | COLOR_PAIR(CP_ACCENT));

    int row = start_row + 1;

    if (st->log.count == 0) {
        /* Empty state */
        attron(COLOR_PAIR(CP_HINT));
        mvprintw(row + 1, (cols - 35) / 2, "no hackathons logged -- try /add");
        attroff(COLOR_PAIR(CP_HINT));
        return;
    }

    /* Adjust scroll offset so selected item is visible */
    if (st->selected < st->scroll_offset) {
        st->scroll_offset = st->selected;
    }
    if (st->selected >= st->scroll_offset + st->list_height) {
        st->scroll_offset = st->selected - st->list_height + 1;
    }

    int visible = st->log.count - st->scroll_offset;
    if (visible > st->list_height) visible = st->list_height;

    for (int i = 0; i < visible; i++) {
        int entry_idx = st->scroll_offset + i;
        const HackEntry *e = &st->log.entries[entry_idx];
        int is_selected = (entry_idx == st->selected);

        if (is_selected) {
            attron(COLOR_PAIR(CP_ACCENT) | A_REVERSE);
        }

        /* Clear the line */
        move(row + i, 0);
        clrtoeol();

        /* Deadline with urgency color */
        if (!is_selected) {
            int dl_color = deadline_color(e->deadline);
            if (dl_color != CP_NORMAL) attron(COLOR_PAIR(dl_color));
        }
        mvprintw(row + i, col_deadline, "%-10s", e->deadline);
        if (!is_selected) {
            int dl_color = deadline_color(e->deadline);
            if (dl_color != CP_NORMAL) attroff(COLOR_PAIR(dl_color));
        }

        /* Status with status color */
        if (!is_selected) {
            attron(COLOR_PAIR(status_color(e->status)));
        }
        mvprintw(row + i, col_status, "%-10s", status_to_str(e->status));
        if (!is_selected) {
            attroff(COLOR_PAIR(status_color(e->status)));
        }

        /* Name */
        int max_name = col_prize - col_name - 2;
        if (max_name > MAX_NAME_LEN) max_name = MAX_NAME_LEN;
        if (max_name < 10) max_name = 10;
        char name_trunc[MAX_NAME_LEN];
        strncpy(name_trunc, e->name, max_name);
        name_trunc[max_name] = '\0';
        if ((int)strlen(e->name) > max_name && max_name > 3) {
            name_trunc[max_name - 1] = '.';
            name_trunc[max_name - 2] = '.';
        }
        mvprintw(row + i, col_name, "%s", name_trunc);

        /* Prize (if any and fits) */
        if (col_prize < cols - 5 && e->prize_zar > 0) {
            char prize_buf[32];
            format_zar(e->prize_zar, prize_buf, sizeof(prize_buf));
            mvprintw(row + i, col_prize, "%s", prize_buf);
        }

        if (is_selected) {
            attroff(COLOR_PAIR(CP_ACCENT) | A_REVERSE);
        }
    }

    /* Scroll indicator */
    int remaining = st->log.count - st->scroll_offset - visible;
    if (remaining > 0) {
        attron(COLOR_PAIR(CP_HINT));
        mvprintw(row + visible, 2, "... %d more below", remaining);
        attroff(COLOR_PAIR(CP_HINT));
    }
    if (st->scroll_offset > 0) {
        attron(COLOR_PAIR(CP_HINT));
        mvprintw(start_row, cols - 16, "%d above ...", st->scroll_offset);
        attroff(COLOR_PAIR(CP_HINT));
    }
}

/* ── command line ─────────────────────────────────────────── */

static void draw_cmdline(TuiState *st) {
    int row = st->term_rows - 1;
    move(row, 0);
    clrtoeol();

    if (st->cmd_active) {
        attron(COLOR_PAIR(CP_ACCENT) | A_BOLD);
        mvaddch(row, 0, '/');
        attroff(A_BOLD);
        mvprintw(row, 1, "%s", st->cmd_buf);
        attroff(COLOR_PAIR(CP_ACCENT));
        /* Position cursor */
        move(row, 1 + st->cmd_len);
        curs_set(1);
    } else if (st->message[0]) {
        int cp = st->msg_is_error ? CP_ERROR : CP_ACCENT;
        attron(COLOR_PAIR(cp));
        mvprintw(row, 1, "%s", st->message);
        attroff(COLOR_PAIR(cp));
        curs_set(0);
    } else {
        attron(COLOR_PAIR(CP_HINT) | A_DIM);
        mvprintw(row, 1, "press / for commands  |  q to quit  |  j/k to navigate");
        attroff(COLOR_PAIR(CP_HINT) | A_DIM);
        curs_set(0);
    }
}

/* ── profile indicator ────────────────────────────────────── */

static void draw_profile_indicator(TuiState *st) {
    if (strcmp(st->profile_name, "default") != 0) {
        int row = 0;
        attron(COLOR_PAIR(CP_ACTIVE_STATUS) | A_DIM);
        mvprintw(row, st->term_cols - strlen(st->profile_name) - 3, "[%s]", st->profile_name);
        attroff(COLOR_PAIR(CP_ACTIVE_STATUS) | A_DIM);
    }
}

/* ── main draw ────────────────────────────────────────────── */

static void tui_draw(TuiState *st) {
    erase();

    getmaxyx(stdscr, st->term_rows, st->term_cols);

    int row = draw_banner(st);
    draw_profile_indicator(st);
    row = draw_stats(st, row);
    draw_entry_list(st, row);
    draw_cmdline(st);

    refresh();
}

/* ── command execution ────────────────────────────────────── */

static void execute_command(TuiState *st) {
    if (st->cmd_len == 0) return;

    /* Check for calendar command */
    if (strcasecmp(st->cmd_buf, "cal") == 0) {
        st->in_calendar = 1;
        st->cmd_active = 0;
        st->cmd_buf[0] = '\0';
        st->cmd_len = 0;
        return;
    }

    /* Tokenize and dispatch */
    char **argv = NULL;
    int argc = tokenize_command(st->cmd_buf, &argv);

    if (argc > 0) {
        CmdResult result = cmd_dispatch(argc, argv, &st->cmd_ctx);
        strncpy(st->message, result.message, sizeof(st->message) - 1);
        st->msg_is_error = (result.success != 0);

        /* Handle save */
        if (st->cmd_ctx.needs_save) {
            hacklog_save(st->profile_name, &st->log);
            st->cmd_ctx.needs_save = 0;
            sort_entries_by_deadline(&st->log);
        }

        /* Handle profile switch */
        if (st->cmd_ctx.needs_reload) {
            strncpy(st->profile_name, st->cmd_ctx.profile_name, sizeof(st->profile_name) - 1);
            tui_load_data(st);
            st->selected = 0;
            st->scroll_offset = 0;
            st->cmd_ctx.needs_reload = 0;
        }

        /* Bounds check selected after possible deletions */
        if (st->selected >= st->log.count && st->log.count > 0) {
            st->selected = st->log.count - 1;
        }
    }

    free_tokens(argc, argv);

    st->cmd_active = 0;
    st->cmd_buf[0] = '\0';
    st->cmd_len = 0;
}

/* ── calendar view ────────────────────────────────────────── */

/* Color palette for per-entry identity coloring */
static const int entry_colors[] = {
    COLOR_GREEN, COLOR_CYAN, COLOR_YELLOW, COLOR_MAGENTA,
    COLOR_BLUE, COLOR_RED, COLOR_WHITE
};
#define NUM_ENTRY_COLORS 7
#define CP_ENTRY_BASE 20 /* color pair offset for entry identity colors */

static void init_entry_colors(void) {
    for (int i = 0; i < NUM_ENTRY_COLORS; i++) {
        init_pair(CP_ENTRY_BASE + i, entry_colors[i], -1);
    }
}

/* Simple hash for consistent color assignment */
static int entry_color_idx(const char *name) {
    unsigned int hash = 0;
    while (*name) {
        hash = hash * 31 + (unsigned char)*name;
        name++;
    }
    return hash % NUM_ENTRY_COLORS;
}

static void draw_calendar_view(TuiState *st, int year, int month) {
    erase();
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    /* Title */
    char title[64];
    static const char *month_names[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    snprintf(title, sizeof(title), "%s %d", month_names[month - 1], year);

    attron(COLOR_PAIR(CP_ACCENT) | A_BOLD);
    mvprintw(0, (cols - strlen(title)) / 2, "%s", title);
    attroff(COLOR_PAIR(CP_ACCENT) | A_BOLD);

    /* Day headers (Mon-Sun) */
    static const char *day_headers[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    int cell_w = cols / 7;
    if (cell_w < 5) cell_w = 5;

    attron(A_BOLD);
    for (int d = 0; d < 7; d++) {
        mvprintw(2, d * cell_w + 1, "%s", day_headers[d]);
    }
    attroff(A_BOLD);

    /* Calculate first day of month (Monday = 0) */
    struct tm first = {0};
    first.tm_year = year - 1900;
    first.tm_mon = month - 1;
    first.tm_mday = 1;
    mktime(&first);
    int first_dow = (first.tm_wday + 6) % 7; /* Convert Sun=0 to Mon=0 */

    /* Days in month */
    struct tm last = {0};
    last.tm_year = year - 1900;
    last.tm_mon = month; /* next month */
    last.tm_mday = 0;    /* last day of prev month */
    mktime(&last);
    int days_in_month = last.tm_mday;

    /* Build map of deadlines per day */
    int day_entries[32][8]; /* day_entries[day][0..n] = entry indices */
    int day_entry_count[32];
    memset(day_entry_count, 0, sizeof(day_entry_count));

    for (int i = 0; i < st->log.count; i++) {
        int y, m, d;
        if (sscanf(st->log.entries[i].deadline, "%d-%d-%d", &y, &m, &d) == 3) {
            if (y == year && m == month && d >= 1 && d <= 31) {
                int c = day_entry_count[d];
                if (c < 8) {
                    day_entries[d][c] = i;
                    day_entry_count[d]++;
                }
            }
        }
    }

    /* Draw days */
    int grid_start = 3;
    for (int day = 1; day <= days_in_month; day++) {
        int pos = first_dow + day - 1;
        int col_idx = pos % 7;
        int row_idx = pos / 7;
        int x = col_idx * cell_w + 1;
        int y = grid_start + row_idx * 2;

        if (y >= rows - 6) break; /* don't overflow */

        /* Draw day number */
        if (day_entry_count[day] > 0) {
            /* Urgency color based on nearest deadline (which is this day) */
            char date_str[11];
            snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", year, month, day);
            int dl_cp = deadline_color(date_str);
            if (dl_cp != CP_NORMAL) {
                attron(COLOR_PAIR(dl_cp) | A_BOLD);
            } else {
                attron(A_BOLD);
            }
            mvprintw(y, x, "%2d", day);
            attroff(COLOR_PAIR(dl_cp) | A_BOLD);

            /* Entry count / identity markers */
            for (int e = 0; e < day_entry_count[day] && e < 3; e++) {
                int eidx = day_entries[day][e];
                int cidx = entry_color_idx(st->log.entries[eidx].name);
                attron(COLOR_PAIR(CP_ENTRY_BASE + cidx));
                mvaddch(y, x + 3 + e, ACS_BULLET);
                attroff(COLOR_PAIR(CP_ENTRY_BASE + cidx));
            }
            if (day_entry_count[day] > 3) {
                mvprintw(y, x + 6, "+%d", day_entry_count[day] - 3);
            }
        } else {
            attron(A_DIM);
            mvprintw(y, x, "%2d", day);
            attroff(A_DIM);
        }
    }

    /* Legend at bottom */
    int legend_row = grid_start + 14; /* after the grid */
    if (legend_row >= rows - 2) legend_row = rows - 4;

    draw_hline(legend_row - 1, 0, cols);
    attron(COLOR_PAIR(CP_ACCENT) | A_BOLD);
    mvprintw(legend_row, 1, "Legend:");
    attroff(COLOR_PAIR(CP_ACCENT) | A_BOLD);
    legend_row++;

    /* Show entries with deadlines in this month */
    int shown = 0;
    for (int i = 0; i < st->log.count && legend_row < rows - 1; i++) {
        int y, m, d;
        if (sscanf(st->log.entries[i].deadline, "%d-%d-%d", &y, &m, &d) == 3) {
            if (y == year && m == month) {
                int cidx = entry_color_idx(st->log.entries[i].name);
                attron(COLOR_PAIR(CP_ENTRY_BASE + cidx));
                mvaddch(legend_row, 2, ACS_BULLET);
                attroff(COLOR_PAIR(CP_ENTRY_BASE + cidx));
                mvprintw(legend_row, 4, "%s (%02d/%02d) [%s]",
                         st->log.entries[i].name, m, d,
                         status_to_str(st->log.entries[i].status));
                legend_row++;
                shown++;
            }
        }
    }
    if (shown == 0) {
        attron(COLOR_PAIR(CP_HINT) | A_DIM);
        mvprintw(legend_row, 2, "No deadlines this month");
        attroff(COLOR_PAIR(CP_HINT) | A_DIM);
    }

    /* Navigation hint */
    attron(COLOR_PAIR(CP_HINT) | A_DIM);
    mvprintw(rows - 1, 1, "h/l: prev/next month  |  Esc: back to dashboard");
    attroff(COLOR_PAIR(CP_HINT) | A_DIM);

    refresh();
}

static void run_calendar(TuiState *st) {
    /* Start with current month */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    int year = tm->tm_year + 1900;
    int month = tm->tm_mon + 1;

    while (1) {
        draw_calendar_view(st, year, month);

        int ch = getch();
        if (ch == 27 || ch == 'q') { /* Esc or q */
            break;
        } else if (ch == 'h' || ch == KEY_LEFT) {
            month--;
            if (month < 1) { month = 12; year--; }
        } else if (ch == 'l' || ch == KEY_RIGHT) {
            month++;
            if (month > 12) { month = 1; year++; }
        }
    }

    st->in_calendar = 0;
}

/* ── main TUI loop ────────────────────────────────────────── */

int tui_run(const char *profile_name) {
    setlocale(LC_ALL, "");

    /* Initialize state */
    TuiState st;
    memset(&st, 0, sizeof(st));
    strncpy(st.profile_name, profile_name, sizeof(st.profile_name) - 1);

    tui_load_data(&st);

    /* Initialize ncurses */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        init_colors();
        init_entry_colors();
    }

    /* Main loop */
    int running = 1;
    while (running) {
        if (st.in_calendar) {
            run_calendar(&st);
            continue;
        }

        tui_draw(&st);

        int ch = getch();

        if (st.cmd_active) {
            /* Command line mode */
            if (ch == 27) { /* Esc */
                st.cmd_active = 0;
                st.cmd_buf[0] = '\0';
                st.cmd_len = 0;
                st.message[0] = '\0';
            } else if (ch == '\n' || ch == KEY_ENTER) {
                execute_command(&st);
            } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
                if (st.cmd_len > 0) {
                    st.cmd_buf[--st.cmd_len] = '\0';
                }
            } else if (ch >= 32 && ch < 127 && st.cmd_len < 510) {
                st.cmd_buf[st.cmd_len++] = (char)ch;
                st.cmd_buf[st.cmd_len] = '\0';
            }
        } else {
            /* Normal mode */
            switch (ch) {
                case 'q':
                case 'Q':
                    running = 0;
                    break;
                case '/':
                    st.cmd_active = 1;
                    st.cmd_buf[0] = '\0';
                    st.cmd_len = 0;
                    st.message[0] = '\0';
                    break;
                case 'j':
                case KEY_DOWN:
                    if (st.selected < st.log.count - 1) {
                        st.selected++;
                        st.message[0] = '\0';
                    }
                    break;
                case 'k':
                case KEY_UP:
                    if (st.selected > 0) {
                        st.selected--;
                        st.message[0] = '\0';
                    }
                    break;
                case 'g': /* go to top */
                    st.selected = 0;
                    st.scroll_offset = 0;
                    break;
                case 'G': /* go to bottom */
                    if (st.log.count > 0) {
                        st.selected = st.log.count - 1;
                    }
                    break;
                case KEY_RESIZE:
                    /* Terminal was resized — just redraw */
                    break;
            }
        }
    }

    /* Cleanup */
    endwin();
    return 0;
}
