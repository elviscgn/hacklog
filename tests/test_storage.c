#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../src/data.h"
#include "../src/storage.h"

/*
 * Storage tests use a temporary directory to avoid touching real user data.
 * We override HOME to a temp dir for isolation.
 */

static char test_home[256];

static void setup_test_env(void) {
    snprintf(test_home, sizeof(test_home), "/tmp/hacklog_test_%d", getpid());
    mkdir(test_home, 0755);
    setenv("HOME", test_home, 1);
}

static void cleanup_test_env(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_home);
    system(cmd);
}

static void test_dir_init(void) {
    assert(hacklog_dir_init() == 0);

    /* Verify directories were created */
    char path[512];
    snprintf(path, sizeof(path), "%s/.hacklog", test_home);
    struct stat st;
    assert(stat(path, &st) == 0 && S_ISDIR(st.st_mode));

    snprintf(path, sizeof(path), "%s/.hacklog/profiles", test_home);
    assert(stat(path, &st) == 0 && S_ISDIR(st.st_mode));

    snprintf(path, sizeof(path), "%s/.hacklog/profiles/default.db", test_home);
    assert(stat(path, &st) == 0);

    /* Calling again should be idempotent */
    assert(hacklog_dir_init() == 0);
    printf("  PASS: hacklog_dir_init\n");
}

static void test_roundtrip(void) {
    hacklog_dir_init();

    HackLog log;
    hacklog_init(&log);

    /* Add a few entries with varying data */
    HackEntry *e = &log.entries[0];
    strcpy(e->name, "Devpost AI Challenge");
    strcpy(e->deadline, "2026-08-15");
    e->status = STATUS_WON;
    e->prize_amount = 2500.0;
    strcpy(e->prize_currency, "USD");
    e->prize_zar = 46250.0;
    strcpy(e->notes, "team of 3, used Next.js");

    e = &log.entries[1];
    strcpy(e->name, "Local Hack Day");
    strcpy(e->deadline, "2026-07-01");
    e->status = STATUS_SUBMITTED;
    e->prize_amount = 0;
    e->prize_currency[0] = '\0';
    e->prize_zar = 0;
    strcpy(e->notes, "solo project");

    e = &log.entries[2];
    strcpy(e->name, "No Notes Hack");
    strcpy(e->deadline, "2026-09-30");
    e->status = STATUS_APPLIED;
    e->notes[0] = '\0';

    log.count = 3;

    /* Save */
    assert(hacklog_save("default", &log) == 0);

    /* Load into a fresh log */
    HackLog loaded;
    assert(hacklog_load("default", &loaded) == 0);

    /* Verify count */
    assert(loaded.count == 3);

    /* Verify first entry fields */
    assert(strcmp(loaded.entries[0].name, "Devpost AI Challenge") == 0);
    assert(strcmp(loaded.entries[0].deadline, "2026-08-15") == 0);
    assert(loaded.entries[0].status == STATUS_WON);
    assert(loaded.entries[0].prize_amount > 2499.0 && loaded.entries[0].prize_amount < 2501.0);
    assert(strcmp(loaded.entries[0].prize_currency, "USD") == 0);
    assert(loaded.entries[0].prize_zar > 46249.0 && loaded.entries[0].prize_zar < 46251.0);
    assert(strcmp(loaded.entries[0].notes, "team of 3, used Next.js") == 0);

    /* Verify second entry */
    assert(strcmp(loaded.entries[1].name, "Local Hack Day") == 0);
    assert(loaded.entries[1].status == STATUS_SUBMITTED);

    /* Verify third entry (no notes) */
    assert(strcmp(loaded.entries[2].name, "No Notes Hack") == 0);
    assert(loaded.entries[2].status == STATUS_APPLIED);

    printf("  PASS: save/load roundtrip\n");
}

static void test_profile_exists(void) {
    hacklog_dir_init();

    assert(hacklog_profile_exists("default") == 1);
    assert(hacklog_profile_exists("nonexistent") == 0);
    printf("  PASS: profile_exists\n");
}

static void test_load_nonexistent(void) {
    HackLog log;
    assert(hacklog_load("does_not_exist", &log) == -1);
    printf("  PASS: load nonexistent profile\n");
}

static void test_empty_profile(void) {
    hacklog_dir_init();

    /* default.db starts empty (just header) */
    HackLog log;
    assert(hacklog_load("default", &log) == 0);
    assert(log.count == 0);
    printf("  PASS: empty profile loads with 0 entries\n");
}

void run_storage_tests(void) {
    printf("storage tests:\n");
    setup_test_env();

    test_dir_init();
    test_empty_profile();
    test_roundtrip();
    test_profile_exists();
    test_load_nonexistent();

    cleanup_test_env();
    printf("storage tests: ALL PASSED\n\n");
}
