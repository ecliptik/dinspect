/* fileout.h - plain-text report writer */

#ifndef DOSFETCH_FILEOUT_H
#define DOSFETCH_FILEOUT_H

#include "fields.h"

/* Writes "<label>: <value>" lines for each field to path (overwritten if
 * it exists). Returns 0 on success, -1 if the file could not be opened.
 */
int write_fields_file(const field_t *fields, int count, const char *path);

/* Creates or truncates the report file at path to zero bytes, without
 * writing anything. Called before probing begins so that a run which
 * never reaches write_fields_file() (hang, reboot, crash) leaves an
 * empty file rather than a stale report from an earlier run -- see the
 * comment at the call site in main.c. Returns 0 on success, -1 if the
 * file could not be opened for writing.
 */
int truncate_fields_file(const char *path);

#endif
