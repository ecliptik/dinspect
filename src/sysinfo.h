/* sysinfo.h - OS-level facts (DOS version, shell) */

#ifndef DOSFETCH_SYSINFO_H
#define DOSFETCH_SYSINFO_H

#include <stddef.h>

/* Formats "<Vendor> DOS <major>.<minor>" into buf. */
void get_dos_version(char *buf, size_t buflen);

/* Formats the COMSPEC environment variable into buf, or "UNKNOWN". */
void get_shell(char *buf, size_t buflen);

/* Current BIOS clock tick count (INT 1Ah AH=00h), ~18.2 ticks/second
 * since midnight. Call once at startup and pass the result to
 * format_runtime() at the end to report how long detection took.
 */
unsigned long get_tick_count(void);

/* Formats the elapsed time since start_ticks (as returned by
 * get_tick_count()) into buf, in milliseconds. Reports "0 ms" instead of
 * a bogus huge value across the midnight tick-count wraparound.
 */
void format_runtime(char *buf, size_t buflen, unsigned long start_ticks);

#endif
