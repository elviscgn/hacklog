#ifndef HACKLOG_CURRENCY_H
#define HACKLOG_CURRENCY_H

#include <stddef.h>

/* Maximum number of currency rates we store */
#define MAX_RATES 32

/* A single currency rate entry */
typedef struct {
    char code[4];   /* 3-letter currency code, e.g. "USD" */
    double rate;    /* how many ZAR per 1 unit of this currency */
} CurrencyRate;

/* Rate table loaded from config */
typedef struct {
    CurrencyRate rates[MAX_RATES];
    int count;
} RateTable;

/* Parse a prize string like "$2500", "R10000", "£500", "EUR 200"
   into amount and currency code.
   Returns 0 on success, -1 on failure. */
int parse_prize(const char *input, double *amount, char *currency_code);

/* Convert an amount in a given currency to ZAR using the rate table.
   Returns the ZAR value, or the original amount if no rate found (assumes ZAR). */
double convert_to_zar(double amount, const char *currency_code, const RateTable *rates);

/* Load rates from ~/.hacklog/config. Returns 0 on success. */
int load_rates(RateTable *rates);

/* Save rates to ~/.hacklog/config. Returns 0 on success. */
int save_rates(const RateTable *rates);

/* Update or add a rate in the table */
void set_rate(RateTable *rates, const char *code, double rate);

/* Get rate for a currency code. Returns 0.0 if not found. */
double get_rate(const RateTable *rates, const char *code);

/* Initialize rate table with sensible defaults */
void init_default_rates(RateTable *rates);

#endif /* HACKLOG_CURRENCY_H */
