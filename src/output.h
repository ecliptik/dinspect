/* output.h - text-mode screen rendering */

#ifndef DOSFETCH_OUTPUT_H
#define DOSFETCH_OUTPUT_H

#include "fields.h"
#include "sysinfo.h"

/* Clears the screen and draws the given fields.
 *
 * show_logo - draw the ASCII logo in the left column and the fields in
 *             the right column (like the original layout); if false, the
 *             fields start at the left margin instead. Rows beyond the
 *             logo's own height always use the full screen width, logo
 *             or not, since there's nothing there to make room for.
 * plain     - use a single, uniform text attribute for every field
 *             (label and value alike) instead of white-label/grey-value,
 *             and implies show_logo is ignored (no logo). Intended for
 *             OCR-friendly, monochrome capture.
 * no_color  - draw the logo and fields with a single, uniform text
 *             attribute instead of their normal colors, but (unlike
 *             plain) still show the logo and its normal layout.
 * vendor    - selects which logo variant to draw (MS-DOS, FreeDOS, or a
 *             generic default for anything else), ignored if show_logo
 *             is false.
 */
void render_screen(const field_t *fields, int count, int show_logo, int plain,
                    int no_color, dos_vendor_t vendor);

#endif
