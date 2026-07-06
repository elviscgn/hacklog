#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include "../src/data.h"

/* Test status string conversion */
static void test_status_to_str(void) {
    assert(strcmp(status_to_str(STATUS_APPLIED), "applied") == 0);
    assert(strcmp(status_to_str(STATUS_ACTIVE), "active") == 0);
    assert(strcmp(status_to_str(STATUS_SUBMITTED), "submitted") == 0);
    assert(strcmp(status_to_str(STATUS_WON), "won") == 0);
    assert(strcmp(status_to_str(STATUS_LOST), "lost") == 0);
    assert(strcmp(status_to_str(STATUS_REJECTED), "rejected") == 0);
    assert(strcmp(status_to_str(STATUS_CANCELLED), "cancelled") == 0);
    printf("  PASS: status_to_str\n");
}

static void test_str_to_status(void) {
    assert(str_to_status("applied") == STATUS_APPLIED);
    assert(str_to_status("ACTIVE") == STATUS_ACTIVE);
    assert(str_to_status("Won") == STATUS_WON);
    assert(str_to_status("cancelled") == STATUS_CANCELLED);
    /* Unknown string defaults to applied */
    assert(str_to_status("garbage") == STATUS_APPLIED);
    assert(str_to_status(NULL) == STATUS_APPLIED);
    printf("  PASS: str_to_status\n");
}

/* Test win rate with various scenarios */
static void test_win_rate_no_resolved(void) {
    HackLog log;
    hacklog_init(&log);
    /* No entries at all */
    assert(win_rate(&log) == -1.0);

    /* Only applied entries — still no resolved */
    strcpy(log.entries[0].name, "Test");
    log.entries[0].status = STATUS_APPLIED;
    log.count = 1;
    assert(win_rate(&log) == -1.0);
    printf("  PASS: win_rate with no resolved entries\n");
}

static void test_win_rate_basic(void) {
    HackLog log;
    hacklog_init(&log);

    /* 2 won, 1 lost → 2/3 */
    for (int i = 0; i < 3; i++) {
        snprintf(log.entries[i].name, MAX_NAME_LEN, "Hack %d", i);
        log.entries[i].status = (i < 2) ? STATUS_WON : STATUS_LOST;
    }
    log.count = 3;
    double rate = win_rate(&log);
    assert(fabs(rate - (2.0/3.0)) < 0.001);
    printf("  PASS: win_rate basic calculation\n");
}

static void test_win_rate_ignores_non_resolved(void) {
    HackLog log;
    hacklog_init(&log);

    /* 1 won, 1 lost, plus applied/active/submitted/rejected/cancelled */
    log.entries[0].status = STATUS_WON;
    log.entries[1].status = STATUS_LOST;
    log.entries[2].status = STATUS_APPLIED;
    log.entries[3].status = STATUS_ACTIVE;
    log.entries[4].status = STATUS_SUBMITTED;
    log.entries[5].status = STATUS_REJECTED;
    log.entries[6].status = STATUS_CANCELLED;
    log.count = 7;

    double rate = win_rate(&log);
    assert(fabs(rate - 0.5) < 0.001); /* 1/(1+1) */
    printf("  PASS: win_rate ignores non-resolved\n");
}

static void test_total_won_zar(void) {
    HackLog log;
    hacklog_init(&log);

    log.entries[0].status = STATUS_WON;
    log.entries[0].prize_zar = 10000.0;
    log.entries[1].status = STATUS_WON;
    log.entries[1].prize_zar = 5000.0;
    log.entries[2].status = STATUS_LOST;
    log.entries[2].prize_zar = 999.0; /* should not count */
    log.count = 3;

    assert(fabs(total_won_zar(&log) - 15000.0) < 0.01);
    printf("  PASS: total_won_zar\n");
}

static void test_streak(void) {
    HackLog log;
    hacklog_init(&log);

    /* Entries sorted by deadline. Pattern: won, won, submitted, cancelled, won
       Walking from end: won(1), then cancelled breaks → streak = 1 */
    strcpy(log.entries[0].deadline, "2026-01-01");
    log.entries[0].status = STATUS_WON;
    strcpy(log.entries[1].deadline, "2026-02-01");
    log.entries[1].status = STATUS_WON;
    strcpy(log.entries[2].deadline, "2026-03-01");
    log.entries[2].status = STATUS_SUBMITTED;
    strcpy(log.entries[3].deadline, "2026-04-01");
    log.entries[3].status = STATUS_CANCELLED;
    strcpy(log.entries[4].deadline, "2026-05-01");
    log.entries[4].status = STATUS_WON;
    log.count = 5;

    assert(streak_count(&log) == 1);
    printf("  PASS: streak with cancelled break\n");
}

static void test_streak_all_completed(void) {
    HackLog log;
    hacklog_init(&log);

    /* All submitted/won/lost — no break */
    strcpy(log.entries[0].deadline, "2026-01-01");
    log.entries[0].status = STATUS_WON;
    strcpy(log.entries[1].deadline, "2026-02-01");
    log.entries[1].status = STATUS_LOST;
    strcpy(log.entries[2].deadline, "2026-03-01");
    log.entries[2].status = STATUS_SUBMITTED;
    log.count = 3;

    assert(streak_count(&log) == 3);
    printf("  PASS: streak all completed\n");
}

static void test_streak_with_applied(void) {
    HackLog log;
    hacklog_init(&log);

    /* applied entries should be skipped, not break the streak */
    strcpy(log.entries[0].deadline, "2026-01-01");
    log.entries[0].status = STATUS_WON;
    strcpy(log.entries[1].deadline, "2026-02-01");
    log.entries[1].status = STATUS_APPLIED;
    strcpy(log.entries[2].deadline, "2026-03-01");
    log.entries[2].status = STATUS_WON;
    log.count = 3;

    assert(streak_count(&log) == 2);
    printf("  PASS: streak skips applied\n");
}

static void test_find_entry(void) {
    HackLog log;
    hacklog_init(&log);

    strcpy(log.entries[0].name, "Alpha Hack");
    strcpy(log.entries[1].name, "Beta Hack");
    log.count = 2;

    assert(find_entry_by_name(&log, "Alpha Hack") == 0);
    assert(find_entry_by_name(&log, "alpha hack") == 0); /* case insensitive */
    assert(find_entry_by_name(&log, "Beta Hack") == 1);
    assert(find_entry_by_name(&log, "Gamma Hack") == -1);
    printf("  PASS: find_entry_by_name\n");
}

static void test_sort_entries(void) {
    HackLog log;
    hacklog_init(&log);

    strcpy(log.entries[0].name, "Later");
    strcpy(log.entries[0].deadline, "2026-09-01");
    strcpy(log.entries[1].name, "Earlier");
    strcpy(log.entries[1].deadline, "2026-03-01");
    strcpy(log.entries[2].name, "Middle");
    strcpy(log.entries[2].deadline, "2026-06-15");
    log.count = 3;

    sort_entries_by_deadline(&log);
    assert(strcmp(log.entries[0].name, "Earlier") == 0);
    assert(strcmp(log.entries[1].name, "Middle") == 0);
    assert(strcmp(log.entries[2].name, "Later") == 0);
    printf("  PASS: sort_entries_by_deadline\n");
}

static void test_date_validation(void) {
    assert(is_valid_date("2026-08-01") == 1);
    assert(is_valid_date("2026-12-31") == 1);
    assert(is_valid_date("not-a-date") == 0);
    assert(is_valid_date("2026-8-1") == 0);   /* wrong format */
    assert(is_valid_date("") == 0);
    assert(is_valid_date(NULL) == 0);
    printf("  PASS: date validation\n");
}

void run_data_tests(void) {
    printf("data tests:\n");
    test_status_to_str();
    test_str_to_status();
    test_win_rate_no_resolved();
    test_win_rate_basic();
    test_win_rate_ignores_non_resolved();
    test_total_won_zar();
    test_streak();
    test_streak_all_completed();
    test_streak_with_applied();
    test_find_entry();
    test_sort_entries();
    test_date_validation();
    printf("data tests: ALL PASSED\n\n");
}
