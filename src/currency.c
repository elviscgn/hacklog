#include "currency.h"
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* Check for multi-byte currency symbols (£, €) */
static int check_multibyte_symbol(const char *input, const char **code, int *skip) {
    /* £ = UTF-8: C2 A3 */
    if ((unsigned char)input[0] == 0xC2 && (unsigned char)input[1] == 0xA3) {
        *code = "GBP";
        *skip = 2;
        return 1;
    }
    /* € = UTF-8: E2 82 AC */
    if ((unsigned char)input[0] == 0xE2 && (unsigned char)input[1] == 0x82 &&
        (unsigned char)input[2] == 0xAC) {
        *code = "EUR";
        *skip = 3;
        return 1;
    }
    return 0;
}

int parse_prize(const char *input, double *amount, char *currency_code) {
    if (!input || !amount || !currency_code) return -1;

    const char *p = input;

    /* Skip leading whitespace */
    while (*p && isspace((unsigned char)*p)) p++;

    if (!*p) return -1;

    /* Check for multi-byte symbols first */
    const char *detected_code = NULL;
    int skip_bytes = 0;

    if (check_multibyte_symbol(p, &detected_code, &skip_bytes)) {
        strcpy(currency_code, detected_code);
        p += skip_bytes;
    }
    /* Check for $ symbol */
    else if (*p == '$') {
        strcpy(currency_code, "USD");
        p++;
    }
    /* Check for R symbol (but not if followed by letters that make a code like "RUB") */
    else if (*p == 'R' && p[1] && (isdigit((unsigned char)p[1]) || isspace((unsigned char)p[1]))) {
        strcpy(currency_code, "ZAR");
        p++;
    }
    /* Check for 3-letter currency code */
    else if (isalpha((unsigned char)p[0]) && isalpha((unsigned char)p[1]) && isalpha((unsigned char)p[2])) {
        currency_code[0] = toupper((unsigned char)p[0]);
        currency_code[1] = toupper((unsigned char)p[1]);
        currency_code[2] = toupper((unsigned char)p[2]);
        currency_code[3] = '\0';
        p += 3;
    }
    else {
        return -1; /* no recognizable currency */
    }

    /* Skip whitespace between currency and number */
    while (*p && isspace((unsigned char)*p)) p++;

    /* Strip commas from the number for parsing */
    char num_buf[64];
    int ni = 0;
    while (*p && ni < 63) {
        if (*p == ',') { p++; continue; }
        if (isdigit((unsigned char)*p) || *p == '.') {
            num_buf[ni++] = *p;
        } else {
            break;
        }
        p++;
    }
    num_buf[ni] = '\0';

    if (ni == 0) return -1; /* no digits found */

    *amount = atof(num_buf);
    return 0;
}

double convert_to_zar(double amount, const char *currency_code, const RateTable *rates) {
    if (!currency_code || !rates) return amount;

    /* ZAR is the base currency */
    if (strcasecmp(currency_code, "ZAR") == 0) return amount;

    double rate = get_rate(rates, currency_code);
    if (rate <= 0.0) return amount; /* unknown currency, return as-is */

    return amount * rate;
}

int load_rates(RateTable *rates) {
    if (!rates) return -1;

    rates->count = 0;

    char base[512];
    if (hacklog_base_path(base, sizeof(base)) != 0) return -1;

    char config_path[600];
    snprintf(config_path, sizeof(config_path), "%s/config", base);

    FILE *f = fopen(config_path, "r");
    if (!f) {
        /* No config file — use defaults */
        init_default_rates(rates);
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), f) && rates->count < MAX_RATES) {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        /* Parse CODE=RATE */
        char code[4] = {0};
        double rate = 0.0;
        if (sscanf(line, "%3[A-Za-z]=%lf", code, &rate) == 2) {
            /* Uppercase the code */
            for (int i = 0; code[i]; i++) code[i] = toupper((unsigned char)code[i]);
            set_rate(rates, code, rate);
        }
    }

    fclose(f);
    return 0;
}

int save_rates(const RateTable *rates) {
    if (!rates) return -1;

    char base[512];
    if (hacklog_base_path(base, sizeof(base)) != 0) return -1;

    char config_path[600];
    snprintf(config_path, sizeof(config_path), "%s/config", base);

    FILE *f = fopen(config_path, "w");
    if (!f) return -1;

    for (int i = 0; i < rates->count; i++) {
        fprintf(f, "%s=%.2f\n", rates->rates[i].code, rates->rates[i].rate);
    }

    fclose(f);
    return 0;
}

void set_rate(RateTable *rates, const char *code, double rate) {
    if (!rates || !code) return;

    /* Update existing */
    for (int i = 0; i < rates->count; i++) {
        if (strcasecmp(rates->rates[i].code, code) == 0) {
            rates->rates[i].rate = rate;
            return;
        }
    }

    /* Add new */
    if (rates->count < MAX_RATES) {
        strncpy(rates->rates[rates->count].code, code, 3);
        rates->rates[rates->count].code[3] = '\0';
        /* Uppercase */
        for (int i = 0; rates->rates[rates->count].code[i]; i++) {
            rates->rates[rates->count].code[i] =
                toupper((unsigned char)rates->rates[rates->count].code[i]);
        }
        rates->rates[rates->count].rate = rate;
        rates->count++;
    }
}

double get_rate(const RateTable *rates, const char *code) {
    if (!rates || !code) return 0.0;
    for (int i = 0; i < rates->count; i++) {
        if (strcasecmp(rates->rates[i].code, code) == 0) {
            return rates->rates[i].rate;
        }
    }
    return 0.0;
}

void init_default_rates(RateTable *rates) {
    if (!rates) return;
    rates->count = 0;
    set_rate(rates, "USD", 18.50);
    set_rate(rates, "GBP", 23.10);
    set_rate(rates, "EUR", 19.80);
}
