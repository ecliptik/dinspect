/* output.h - text-mode screen rendering */

#ifndef DOSFETCH_OUTPUT_H
#define DOSFETCH_OUTPUT_H

#include "fields.h"

/* Clears the screen and draws the given fields.
 *
 * show_logo - draw the ASCII logo in the left column and the fields in
 *             the right column (like the original layout); if false, the
 *             fields start at the left margin instead.
 * plain     - use a single, uniform text attribute for every field
 *             (label and value alike) instead of white-label/grey-value,
 *             and implies show_logo is ignored (no logo). Intended for
 *             OCR-friendly, monochrome capture.
 */
void render_screen(const field_t *fields, int count, int show_logo, int plain);

#endif
