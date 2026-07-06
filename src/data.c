#include "data.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* Status string lookup table */
static const char *status_strings[] = {
    "applied",
    "active",
    "submitted",
    "won",
    "lost",
    "rejected",
    "cancelled"
};

const char *status_to_str(HackStatus s) {
    if (s >= 0 && s < STATUS_COUNT) {
        return status_strings[s];
    }
    return "unknown";
}

HackStatus str_to_status(const char *s) {
    if (!s) return STATUS_APPLIED;
    for (int i = 0; i < STATUS_COUNT; i++) {
        if (strcasecmp(s, status_strings[i]) == 0) {
            return (HackStatus)i;
        }
    }
    return STATUS_APPLIED; /* default fallback */
}

double win_rate(const HackLog *log) {
    if (!log) return -1.0;
    int won = 0, lost = 0;
    for (int i = 0; i < log->count; i++) {
        if (log->entries[i].status == STATUS_WON) won++;
        else if (log->entries[i].status == STATUS_LOST) lost++;
    }
    if (won + lost == 0) return -1.0; /* no resolved entries */
    return (double)won / (double)(won + lost);
}

double total_won_zar(const HackLog *log) {
    if (!log) return 0.0;
    double total = 0.0;
    for (int i = 0; i < log->count; i++) {
        if (log->entries[i].status == STATUS_WON) {
            total += log->entries[i].prize_zar;
        }
    }
    return total;
}

int streak_count(const HackLog *log) {
    if (!log || log->count == 0) return 0;

    /* We need a sorted copy of indices by deadline to walk in order */
    int indices[MAX_ENTRIES];
    for (int i = 0; i < log->count; i++) indices[i] = i;

    /* Simple insertion sort by deadline string (YYYY-MM-DD sorts lexically) */
    for (int i = 1; i < log->count; i++) {
        int key = indices[i];
        int j = i - 1;
        while (j >= 0 && strcmp(log->entries[indices[j]].deadline,
                                log->entries[key].deadline) > 0) {
            indices[j + 1] = indices[j];
            j--;
        }
        indices[j + 1] = key;
    }

    /* Walk from the most recent backward, counting consecutive
       submitted/won/lost entries. Stop on cancelled or rejected. */
    int streak = 0;
    for (int i = log->count - 1; i >= 0; i--) {
        HackStatus s = log->entries[indices[i]].status;
        if (s == STATUS_SUBMITTED || s == STATUS_WON || s == STATUS_LOST) {
            streak++;
        } else if (s == STATUS_CANCELLED || s == STATUS_REJECTED) {
            break; /* chain broken */
        }
        /* applied and active don't break or count — skip them */
    }
    return streak;
}

/* Comparison function for qsort — sorts by deadline ascending */
static int cmp_deadline(const void *a, const void *b) {
    const HackEntry *ea = (const HackEntry *)a;
    const HackEntry *eb = (const HackEntry *)b;
    return strcmp(ea->deadline, eb->deadline);
}

void sort_entries_by_deadline(HackLog *log) {
    if (!log || log->count <= 1) return;
    qsort(log->entries, log->count, sizeof(HackEntry), cmp_deadline);
}

int find_entry_by_name(const HackLog *log, const char *name) {
    if (!log || !name) return -1;
    for (int i = 0; i < log->count; i++) {
        if (strcasecmp(log->entries[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void hacklog_init(HackLog *log) {
    if (!log) return;
    memset(log, 0, sizeof(HackLog));
    strncpy(log->profile_name, "default", sizeof(log->profile_name) - 1);
}

int parse_date(const char *str, struct tm *out) {
    if (!str || !out || strlen(str) != 10) return 0;
    memset(out, 0, sizeof(struct tm));
    if (sscanf(str, "%d-%d-%d", &out->tm_year, &out->tm_mon, &out->tm_mday) != 3) {
        return 0;
    }
    out->tm_year -= 1900;
    out->tm_mon -= 1;
    return 1;
}

int days_until_deadline(const char *deadline) {
    struct tm dl;
    if (!parse_date(deadline, &dl)) return 9999;

    time_t now = time(NULL);
    struct tm *today = localtime(&now);

    /* Zero out time components for date-only comparison */
    struct tm today_date = *today;
    today_date.tm_hour = 0;
    today_date.tm_min = 0;
    today_date.tm_sec = 0;

    time_t t_now = mktime(&today_date);
    time_t t_dl = mktime(&dl);

    double diff = difftime(t_dl, t_now);
    return (int)(diff / 86400.0);
}

int is_valid_date(const char *str) {
    if (!str || strlen(str) != 10) return 0;
    if (str[4] != '-' || str[7] != '-') return 0;
    struct tm tmp;
    return parse_date(str, &tmp);
}
