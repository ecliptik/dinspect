/* sysinfo.h - OS-level facts (DOS version, shell) */

#ifndef DOSFETCH_SYSINFO_H
#define DOSFETCH_SYSINFO_H

#include <stddef.h>

/* Formats "<Vendor> DOS <major>.<minor>" into buf. */
void get_dos_version(char *buf, size_t buflen);

/* Formats the COMSPEC environment variable into buf, or "UNKNOWN". */
void get_shell(char *buf, size_t buflen);

/* Current BIOS clock tick count (INT 1Ah AH=00h), ~18.2 ticks/second
 * since midnight. Used by cpu386.c to time the RDTSC measurement
 * window.
 */
unsigned long get_tick_count(void);

typedef enum {
    DOS_VENDOR_UNKNOWN,
    DOS_VENDOR_IBM,
    DOS_VENDOR_MS,
    DOS_VENDOR_FREEDOS,
    DOS_VENDOR_DR
} dos_vendor_t;

/* Raw DOS vendor, via the same INT 21h AH=30h call get_dos_version()
 * formats into a string -- exposed separately so callers (the logo
 * selector) can branch on it without reparsing the formatted string.
 */
dos_vendor_t get_dos_vendor(void);

#endif
