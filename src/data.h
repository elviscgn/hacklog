#ifndef HACKLOG_DATA_H
#define HACKLOG_DATA_H

#include <time.h>

/* Maximum field lengths */
#define MAX_NAME_LEN     256
#define MAX_NOTES_LEN    512
#define MAX_CURRENCY_LEN 4
#define MAX_ENTRIES      1024

/* Status enum — exactly 7 values per spec */
typedef enum {
    STATUS_APPLIED = 0,
    STATUS_ACTIVE,
    STATUS_SUBMITTED,
    STATUS_WON,
    STATUS_LOST,
    STATUS_REJECTED,
    STATUS_CANCELLED,
    STATUS_COUNT  /* sentinel for iteration */
} HackStatus;

/* Single hackathon/project entry */
typedef struct {
    char        name[MAX_NAME_LEN];
    char        deadline[11];          /* YYYY-MM-DD + null */
    HackStatus  status;
    double      prize_amount;          /* original amount (0 if none) */
    char        prize_currency[MAX_CURRENCY_LEN]; /* e.g. "USD", "" if none */
    double      prize_zar;             /* frozen ZAR-converted value */
    char        notes[MAX_NOTES_LEN];
} HackEntry;

/* Collection of entries — the in-memory database */
typedef struct {
    HackEntry   entries[MAX_ENTRIES];
    int         count;
    char        profile_name[64];
} HackLog;

/* Status string conversion */
const char *status_to_str(HackStatus s);
HackStatus  str_to_status(const char *s);

/* Win rate: won / (won + lost). Returns -1.0 if no resolved entries */
double win_rate(const HackLog *log);

/* Total ZAR won across all 'won' entries */
double total_won_zar(const HackLog *log);

/* Streak: consecutive submitted/won/lost entries (by deadline order)
   without a cancelled or rejected breaking the chain */
int streak_count(const HackLog *log);

/* Sort entries by deadline ascending */
void sort_entries_by_deadline(HackLog *log);

/* Find entry by name (case-insensitive). Returns index or -1 */
int find_entry_by_name(const HackLog *log, const char *name);

/* Initialize an empty log */
void hacklog_init(HackLog *log);

/* Date helpers */
int  parse_date(const char *str, struct tm *out);
int  days_until_deadline(const char *deadline);
int  is_valid_date(const char *str);

#endif /* HACKLOG_DATA_H */
