#include "calendar.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

void calendar_print_static(const HackLog *log) {
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    int year = tm_now->tm_year + 1900;
    int month = tm_now->tm_mon + 1;

    /* Month names */
    static const char *month_names[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    printf("\n  %s %d\n\n", month_names[month - 1], year);
    printf("  Mon  Tue  Wed  Thu  Fri  Sat  Sun\n");

    /* Calculate first day of month (Monday = 0) */
    struct tm first = {0};
    first.tm_year = year - 1900;
    first.tm_mon = month - 1;
    first.tm_mday = 1;
    mktime(&first);
    int first_dow = (first.tm_wday + 6) % 7;

    /* Days in month */
    struct tm last = {0};
    last.tm_year = year - 1900;
    last.tm_mon = month;
    last.tm_mday = 0;
    mktime(&last);
    int days_in_month = last.tm_mday;

    /* Build deadline map */
    int has_deadline[32] = {0};
    for (int i = 0; i < log->count; i++) {
        int y, m, d;
        if (sscanf(log->entries[i].deadline, "%d-%d-%d", &y, &m, &d) == 3) {
            if (y == year && m == month && d >= 1 && d <= 31) {
                has_deadline[d]++;
            }
        }
    }

    /* Print padding for first week */
    printf("  ");
    for (int i = 0; i < first_dow; i++) {
        printf("     ");
    }

    for (int day = 1; day <= days_in_month; day++) {
        if (has_deadline[day] > 0) {
            printf(" [%2d]", day);
        } else {
            printf("  %2d ", day);
        }

        if ((first_dow + day) % 7 == 0) {
            printf("\n  ");
        }
    }
    printf("\n\n");

    /* List deadlines this month */
    int shown = 0;
    for (int i = 0; i < log->count; i++) {
        int y, m, d;
        if (sscanf(log->entries[i].deadline, "%d-%d-%d", &y, &m, &d) == 3) {
            if (y == year && m == month) {
                if (!shown) printf("  Deadlines:\n");
                printf("    %02d/%02d  %-30s  [%s]\n",
                       m, d, log->entries[i].name,
                       status_to_str(log->entries[i].status));
                shown++;
            }
        }
    }
    if (!shown) {
        printf("  No deadlines this month.\n");
    }
    printf("\n");
}

/* Interactive calendar is handled entirely in tui.c */
void calendar_view(HackLog *log) {
    (void)log;
}
