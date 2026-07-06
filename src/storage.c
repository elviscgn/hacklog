#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

/*
 * Flat-file format: TSV (tab-separated values)
 *
 * First line is a comment header:
 *   # name\tdeadline\tstatus\tprize_amount\tprize_currency\tprize_zar\tnotes
 *
 * Each subsequent line is one entry:
 *   My Hackathon\t2026-08-01\tapplied\t0\t\t0\tsome notes here
 *
 * Empty fields are just empty between tabs.
 * Notes field is last so it can contain anything except tabs/newlines.
 */

#define TSV_HEADER "# name\tdeadline\tstatus\tprize_amount\tprize_currency\tprize_zar\tnotes\n"

int hacklog_base_path(char *buf, size_t bufsz) {
    const char *home = getenv("HOME");
    if (!home || !home[0]) return -1;
    int n = snprintf(buf, bufsz, "%s/.hacklog", home);
    if (n < 0 || (size_t)n >= bufsz) return -1;
    return 0;
}

int hacklog_profile_path(const char *profile_name, char *buf, size_t bufsz) {
    char base[512];
    if (hacklog_base_path(base, sizeof(base)) != 0) return -1;
    int n = snprintf(buf, bufsz, "%s/profiles/%s.db", base, profile_name);
    if (n < 0 || (size_t)n >= bufsz) return -1;
    return 0;
}

static int mkdir_p(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    if (mkdir(path, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

int hacklog_dir_init(void) {
    char base[512];
    if (hacklog_base_path(base, sizeof(base)) != 0) return -1;

    /* Create ~/.hacklog/ */
    if (mkdir_p(base) != 0) return -1;

    /* Create ~/.hacklog/profiles/ */
    char profiles[600];
    snprintf(profiles, sizeof(profiles), "%s/profiles", base);
    if (mkdir_p(profiles) != 0) return -1;

    /* Create empty default.db if it doesn't exist */
    char default_db[700];
    snprintf(default_db, sizeof(default_db), "%s/default.db", profiles);
    struct stat st;
    if (stat(default_db, &st) != 0) {
        FILE *f = fopen(default_db, "w");
        if (!f) return -1;
        fputs(TSV_HEADER, f);
        fclose(f);
    }

    return 0;
}

int hacklog_profile_exists(const char *profile_name) {
    char path[700];
    if (hacklog_profile_path(profile_name, path, sizeof(path)) != 0) return 0;
    struct stat st;
    return (stat(path, &st) == 0);
}

/* Parse a single TSV line into an entry. Returns 0 on success. */
static int parse_tsv_line(const char *line, HackEntry *entry) {
    if (!line || !entry || line[0] == '#' || line[0] == '\n' || line[0] == '\0') {
        return -1;
    }

    memset(entry, 0, sizeof(HackEntry));

    /* We need to split by tabs — up to 7 fields */
    const char *fields[7];
    int field_count = 0;
    const char *p = line;

    for (int i = 0; i < 7 && p; i++) {
        fields[i] = p;
        field_count++;
        const char *tab = strchr(p, '\t');
        if (tab) {
            p = tab + 1;
        } else {
            p = NULL;
        }
    }

    if (field_count < 3) return -1; /* need at least name, deadline, status */

    /* Field 0: name */
    {
        const char *end = strchr(fields[0], '\t');
        size_t len = end ? (size_t)(end - fields[0]) : strlen(fields[0]);
        if (len >= MAX_NAME_LEN) len = MAX_NAME_LEN - 1;
        strncpy(entry->name, fields[0], len);
        entry->name[len] = '\0';
    }

    /* Field 1: deadline */
    {
        const char *end = strchr(fields[1], '\t');
        size_t len = end ? (size_t)(end - fields[1]) : strlen(fields[1]);
        if (len >= sizeof(entry->deadline)) len = sizeof(entry->deadline) - 1;
        strncpy(entry->deadline, fields[1], len);
        entry->deadline[len] = '\0';
    }

    /* Field 2: status */
    {
        char status_buf[32] = {0};
        const char *end = strchr(fields[2], '\t');
        size_t len = end ? (size_t)(end - fields[2]) : strlen(fields[2]);
        if (len >= sizeof(status_buf)) len = sizeof(status_buf) - 1;
        strncpy(status_buf, fields[2], len);
        /* Strip trailing newline */
        while (len > 0 && (status_buf[len-1] == '\n' || status_buf[len-1] == '\r')) {
            status_buf[--len] = '\0';
        }
        entry->status = str_to_status(status_buf);
    }

    /* Field 3: prize_amount (optional) */
    if (field_count > 3 && fields[3][0] != '\t') {
        char buf[64] = {0};
        const char *end = strchr(fields[3], '\t');
        size_t len = end ? (size_t)(end - fields[3]) : strlen(fields[3]);
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        strncpy(buf, fields[3], len);
        entry->prize_amount = atof(buf);
    }

    /* Field 4: prize_currency (optional) */
    if (field_count > 4 && fields[4][0] != '\t') {
        const char *end = strchr(fields[4], '\t');
        size_t len = end ? (size_t)(end - fields[4]) : strlen(fields[4]);
        if (len >= MAX_CURRENCY_LEN) len = MAX_CURRENCY_LEN - 1;
        strncpy(entry->prize_currency, fields[4], len);
        entry->prize_currency[len] = '\0';
    }

    /* Field 5: prize_zar (optional) */
    if (field_count > 5 && fields[5][0] != '\t') {
        char buf[64] = {0};
        const char *end = strchr(fields[5], '\t');
        size_t len = end ? (size_t)(end - fields[5]) : strlen(fields[5]);
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        strncpy(buf, fields[5], len);
        entry->prize_zar = atof(buf);
    }

    /* Field 6: notes (optional, last field — can contain anything except tab/newline) */
    if (field_count > 6) {
        size_t len = strlen(fields[6]);
        /* Strip trailing newline */
        while (len > 0 && (fields[6][len-1] == '\n' || fields[6][len-1] == '\r')) {
            len--;
        }
        if (len >= MAX_NOTES_LEN) len = MAX_NOTES_LEN - 1;
        strncpy(entry->notes, fields[6], len);
        entry->notes[len] = '\0';
    }

    return 0;
}

int hacklog_load(const char *profile_name, HackLog *log) {
    if (!profile_name || !log) return -1;

    hacklog_init(log);
    strncpy(log->profile_name, profile_name, sizeof(log->profile_name) - 1);

    char path[700];
    if (hacklog_profile_path(profile_name, path, sizeof(path)) != 0) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (log->count >= MAX_ENTRIES) break;

        HackEntry entry;
        if (parse_tsv_line(line, &entry) == 0) {
            log->entries[log->count++] = entry;
        }
    }

    fclose(f);
    return 0;
}

int hacklog_save(const char *profile_name, const HackLog *log) {
    if (!profile_name || !log) return -1;

    char path[700];
    if (hacklog_profile_path(profile_name, path, sizeof(path)) != 0) return -1;

    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fputs(TSV_HEADER, f);

    for (int i = 0; i < log->count; i++) {
        const HackEntry *e = &log->entries[i];
        fprintf(f, "%s\t%s\t%s\t%.2f\t%s\t%.2f\t%s\n",
                e->name,
                e->deadline,
                status_to_str(e->status),
                e->prize_amount,
                e->prize_currency,
                e->prize_zar,
                e->notes);
    }

    fclose(f);
    return 0;
}
