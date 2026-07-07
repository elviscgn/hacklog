#include "calendar.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

/* Stub — will be replaced with full implementation on Day 7 */
void calendar_print_static(const HackLog *log) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    printf("Calendar for %d-%02d:\n", tm->tm_year + 1900, tm->tm_mon + 1);
    printf("(Full implementation coming soon)\n\n");

    if (log->count > 0) {
        printf("Upcoming deadlines:\n");
        for (int i = 0; i < log->count; i++) {
            printf("  %s  %s  [%s]\n",
                   log->entries[i].deadline,
                   log->entries[i].name,
                   status_to_str(log->entries[i].status));
        }
    }
}

void calendar_view(HackLog *log) {
    (void)log;
    /* Will be implemented in Day 7 */
}
