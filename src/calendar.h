#ifndef HACKLOG_CALENDAR_H
#define HACKLOG_CALENDAR_H

#include "data.h"

/* Print a static text calendar to stdout (for `hack cal` shell command) */
void calendar_print_static(const HackLog *log);

/* Launch interactive calendar view inside ncurses TUI.
   Assumes ncurses is already initialized. */
void calendar_view(HackLog *log);

#endif /* HACKLOG_CALENDAR_H */
