/* fileout.h - plain-text report writer */

#ifndef DOSFETCH_FILEOUT_H
#define DOSFETCH_FILEOUT_H

#include "fields.h"

/* Writes "<label>: <value>" lines for each field to path (overwritten if
 * it exists). Returns 0 on success, -1 if the file could not be opened.
 */
int write_fields_file(const field_t *fields, int count, const char *path);

#endif
