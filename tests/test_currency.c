#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../src/currency.h"

static char test_home[256];

static void setup_test_env(void) {
    snprintf(test_home, sizeof(test_home), "/tmp/hacklog_test_cur_%d", getpid());
    mkdir(test_home, 0755);
    setenv("HOME", test_home, 1);

    /* Create hacklog directory structure */
    char path[512];
    snprintf(path, sizeof(path), "%s/.hacklog", test_home);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.hacklog/profiles", test_home);
    mkdir(path, 0755);
}

static void cleanup_test_env(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_home);
    system(cmd);
}

/* Test parsing $ symbol */
static void test_parse_usd_symbol(void) {
    double amount;
    char code[4];
    assert(parse_prize("$2500", &amount, code) == 0);
    assert(fabs(amount - 2500.0) < 0.01);
    assert(strcmp(code, "USD") == 0);
    printf("  PASS: parse $2500\n");
}

/* Test parsing with commas */
static void test_parse_with_commas(void) {
    double amount;
    char code[4];
    assert(parse_prize("$10,000", &amount, code) == 0);
    assert(fabs(amount - 10000.0) < 0.01);
    assert(strcmp(code, "USD") == 0);
    printf("  PASS: parse $10,000 (with commas)\n");
}

/* Test parsing R symbol */
static void test_parse_zar_symbol(void) {
    double amount;
    char code[4];
    assert(parse_prize("R10000", &amount, code) == 0);
    assert(fabs(amount - 10000.0) < 0.01);
    assert(strcmp(code, "ZAR") == 0);
    printf("  PASS: parse R10000\n");
}

/* Test parsing 3-letter code with space */
static void test_parse_code_with_space(void) {
    double amount;
    char code[4];
    assert(parse_prize("USD 2500", &amount, code) == 0);
    assert(fabs(amount - 2500.0) < 0.01);
    assert(strcmp(code, "USD") == 0);
    printf("  PASS: parse USD 2500\n");
}

/* Test parsing EUR code */
static void test_parse_eur_code(void) {
    double amount;
    char code[4];
    assert(parse_prize("EUR 200", &amount, code) == 0);
    assert(fabs(amount - 200.0) < 0.01);
    assert(strcmp(code, "EUR") == 0);
    printf("  PASS: parse EUR 200\n");
}

/* Test parsing £ symbol (UTF-8) */
static void test_parse_gbp_symbol(void) {
    double amount;
    char code[4];
    assert(parse_prize("£500", &amount, code) == 0);
    assert(fabs(amount - 500.0) < 0.01);
    assert(strcmp(code, "GBP") == 0);
    printf("  PASS: parse £500\n");
}

/* Test parsing € symbol (UTF-8) */
static void test_parse_eur_symbol(void) {
    double amount;
    char code[4];
    assert(parse_prize("€200", &amount, code) == 0);
    assert(fabs(amount - 200.0) < 0.01);
    assert(strcmp(code, "EUR") == 0);
    printf("  PASS: parse €200\n");
}

/* Test malformed input */
static void test_parse_malformed(void) {
    double amount;
    char code[4];
    assert(parse_prize("", &amount, code) == -1);
    assert(parse_prize(NULL, &amount, code) == -1);
    assert(parse_prize("$", &amount, code) == -1);
    assert(parse_prize("USD", &amount, code) == -1);
    printf("  PASS: malformed inputs rejected\n");
}

/* Test zero amount */
static void test_parse_zero(void) {
    double amount;
    char code[4];
    assert(parse_prize("$0", &amount, code) == 0);
    assert(fabs(amount) < 0.01);
    printf("  PASS: parse $0\n");
}

/* Test decimal amount */
static void test_parse_decimal(void) {
    double amount;
    char code[4];
    assert(parse_prize("$99.99", &amount, code) == 0);
    assert(fabs(amount - 99.99) < 0.01);
    printf("  PASS: parse $99.99\n");
}

/* Test conversion */
static void test_convert_to_zar(void) {
    RateTable rates;
    init_default_rates(&rates);

    /* USD at 18.50 */
    double zar = convert_to_zar(100.0, "USD", &rates);
    assert(fabs(zar - 1850.0) < 0.01);

    /* ZAR stays the same */
    zar = convert_to_zar(5000.0, "ZAR", &rates);
    assert(fabs(zar - 5000.0) < 0.01);

    /* Unknown currency returns original amount */
    zar = convert_to_zar(100.0, "XYZ", &rates);
    assert(fabs(zar - 100.0) < 0.01);

    printf("  PASS: convert_to_zar\n");
}

/* Test rate table operations */
static void test_rate_table(void) {
    RateTable rates;
    init_default_rates(&rates);

    assert(get_rate(&rates, "USD") > 0);
    assert(get_rate(&rates, "GBP") > 0);
    assert(get_rate(&rates, "EUR") > 0);
    assert(get_rate(&rates, "XYZ") == 0.0); /* not found */

    /* Update a rate */
    set_rate(&rates, "USD", 19.00);
    assert(fabs(get_rate(&rates, "USD") - 19.00) < 0.01);

    /* Add a new rate */
    set_rate(&rates, "JPY", 0.12);
    assert(fabs(get_rate(&rates, "JPY") - 0.12) < 0.01);

    printf("  PASS: rate table operations\n");
}

/* Test rate file round-trip */
static void test_rate_file_roundtrip(void) {
    RateTable rates;
    init_default_rates(&rates);
    set_rate(&rates, "JPY", 0.12);

    assert(save_rates(&rates) == 0);

    RateTable loaded;
    loaded.count = 0;
    assert(load_rates(&loaded) == 0);

    assert(fabs(get_rate(&loaded, "USD") - get_rate(&rates, "USD")) < 0.01);
    assert(fabs(get_rate(&loaded, "GBP") - get_rate(&rates, "GBP")) < 0.01);
    assert(fabs(get_rate(&loaded, "EUR") - get_rate(&rates, "EUR")) < 0.01);
    assert(fabs(get_rate(&loaded, "JPY") - 0.12) < 0.01);

    printf("  PASS: rate file round-trip\n");
}

void run_currency_tests(void) {
    printf("currency tests:\n");
    setup_test_env();

    test_parse_usd_symbol();
    test_parse_with_commas();
    test_parse_zar_symbol();
    test_parse_code_with_space();
    test_parse_eur_code();
    test_parse_gbp_symbol();
    test_parse_eur_symbol();
    test_parse_malformed();
    test_parse_zero();
    test_parse_decimal();
    test_convert_to_zar();
    test_rate_table();
    test_rate_file_roundtrip();

    cleanup_test_env();
    printf("currency tests: ALL PASSED\n\n");
}
