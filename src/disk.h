/* disk.h - floppy count and disk free/total space */

#ifndef DOSFETCH_DISK_H
#define DOSFETCH_DISK_H

#include <stddef.h>
#include "fields.h"

/* Raw floppy drive count (0-4), via the INT 11h equipment word. */
unsigned get_floppy_drive_count(void);

/* Formats the number of floppy drives into buf. */
void get_floppy_count(char *buf, size_t buflen);

/* Formats "<used>/<total> KB (<pct>% free)" for the given drive
 * (0 = default drive, 1 = A:, 2 = B:, ...) into buf. Returns 1 if the
 * drive exists, 0 (buf left untouched) if it doesn't.
 */
int get_disk_usage(char *buf, size_t buflen, unsigned char disk);

/* Appends one "Disk X" field per detected drive from C: onward
 * (stopping at the first drive number DOS reports as not existing --
 * drive letters are contiguous, so this correctly enumerates every
 * configured hard-disk-class drive) to fields[], starting at *count,
 * never exceeding max_fields.
 *
 * Always starts at drive number 3 (C:), never derived from the floppy
 * count: DOS reserves BOTH A: and B: as floppy-class letters regardless
 * of how many physical floppy drives exist (0, 1, or 2) -- a
 * single-floppy-drive machine still has a drive B: (DOS's classic
 * phantom/swap drive sharing the one physical drive). An earlier
 * version started from get_floppy_drive_count()+1, which pointed at
 * drive B: on exactly this (very common) configuration and hit the
 * "Abort, Retry, Fail?" hang below on real hardware.
 *
 * Floppy-lettered drives (A:, B:) are still deliberately never probed
 * here, as belt-and-suspenders: an empty/phantom floppy drive can
 * trigger DOS's interactive "Abort, Retry, Fail?" critical-error
 * prompt. (Their count alone is still reported via
 * get_floppy_count().) A non-floppy removable drive with no media
 * (e.g. an empty CD-ROM drive) can in principle trigger the same
 * prompt for a drive letter enumerated here -- that's now handled at
 * the source rather than avoided: main() installs a silent INT 24h
 * critical-error handler (see critical_error.h) before any disk code
 * runs, so any such prompt resolves itself as a Fail instead of
 * blocking.
 */
void add_disk_fields(field_t *fields, int *count, int max_fields);

#endif
