#ifndef HACKLOG_BANNER_H
#define HACKLOG_BANNER_H

/* Get the ASCII art banner string (big block-letter "hacklog").
   Returns a pointer to a static string constant. */
const char *banner_get(void);

/* Get the small fallback banner for narrow terminals */
const char *banner_get_small(void);

/* Get the number of lines in the big banner */
int banner_height(void);

/* Get the width of the big banner */
int banner_width(void);

#endif /* HACKLOG_BANNER_H */
