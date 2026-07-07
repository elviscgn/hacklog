#ifndef HACKLOG_STORAGE_H
#define HACKLOG_STORAGE_H

#include "data.h"

/* Ensure ~/.hacklog/profiles/ exists, create default.db if needed.
   Returns 0 on success, -1 on error. */
int hacklog_dir_init(void);

/* Get the base hacklog directory path (~/.hacklog/).
   Writes to buf, returns 0 on success. */
int hacklog_base_path(char *buf, size_t bufsz);

/* Get the full path to a profile's .db file.
   Writes to buf, returns 0 on success. */
int hacklog_profile_path(const char *profile_name, char *buf, size_t bufsz);

/* Load a profile's data from disk into log.
   Returns 0 on success, -1 on error (file not found, parse error). */
int hacklog_load(const char *profile_name, HackLog *log);

/* Save log data to disk for the given profile.
   Returns 0 on success, -1 on error. */
int hacklog_save(const char *profile_name, const HackLog *log);

/* Check if a profile exists on disk */
int hacklog_profile_exists(const char *profile_name);

/* Create and seed the demo profile with realistic fabricated data.
   Only creates if demo.db doesn't already exist. */
void hacklog_seed_demo(void);

#endif /* HACKLOG_STORAGE_H */
