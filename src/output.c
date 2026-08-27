/* output.c - text-mode screen rendering
 *
 * Writes characters directly into video memory (segment 0xB000 for
 * monochrome adapters, 0xB800 otherwise, per the BIOS data area video mode
 * byte at 0040:0049) rather than going through stdio or the BIOS teletype
 * call, since teletype output does not honor a foreground color in text
 * modes.
 */

#include <stdio.h>
#include <string.h>
#include "output.h"

#define SCREEN_COLS 80
#define SCREEN_ROWS 25

#define LOGO_COL  1
#define INFO_COL 44
#define ROW_START 1
#define LOGO_ROWS 8

/* Standard DOS text-mode attribute values (background black throughout). */
#define ATTR_YELLOW       14
#define ATTR_LIGHTBLUE     9
#define ATTR_LIGHTRED     12
#define ATTR_LIGHTGRAY     7
#define ATTR_LIGHTMAGENTA 13
#define ATTR_WHITE        15
#define ATTR_NORMAL        7

#define VIDEO_MK_FP(seg, off) \
    ((unsigned char _far *)(((unsigned long)(seg) << 16) | (unsigned)(off)))

static unsigned video_segment(void)
{
    unsigned char _far *mode_byte = VIDEO_MK_FP(0x0040, 0x0049);
    return (*mode_byte == 7) ? 0xB000u : 0xB800u;
}

static void term_clear(unsigned char attr)
{
    unsigned char _far *vram = VIDEO_MK_FP(video_segment(), 0);
    unsigned i;

    for (i = 0; i < (unsigned)(SCREEN_COLS * SCREEN_ROWS); i++) {
        vram[i * 2]     = ' ';
        vram[i * 2 + 1] = attr;
    }
}

static void term_puts(int col, int row, const char *s, unsigned char attr)
{
    unsigned char _far *vram = VIDEO_MK_FP(video_segment(), 0);
    unsigned offset = (unsigned)(row * SCREEN_COLS + col) * 2;
    int cur_col = col;

    /* Clip at the screen edge -- without this, a value longer than the
     * remaining columns spills into the next row's video memory instead
     * of just getting cut off.
     */
    while (*s != '\0' && cur_col < SCREEN_COLS) {
        vram[offset]     = (unsigned char)*s;
        vram[offset + 1] = attr;
        offset += 2;
        s++;
        cur_col++;
    }
}

static void term_set_cursor(int col, int row)
{
    _asm {
        mov ah, 02h
        mov bh, 0
        mov dh, byte ptr row
        mov dl, byte ptr col
        int 10h
    }
}

/* One positioned, colored text fragment within a logo -- a whole
 * 14-column color band for the wordmark-style logos, or a single
 * letter's single row of block-character pixels for the stacked
 * MS-DOS logo, where each letter needs its own solid color. Using the
 * same small "list of runs" representation for both keeps print_logo()
 * itself trivial regardless of how a given logo is composed.
 */
typedef struct {
    int col, row; /* offset from the logo's own origin */
    const char *text;
    unsigned char attr;
} logo_run_t;

typedef struct {
    const logo_run_t *runs;
    int run_count;
} logo_t;

/* Generated (not hand-transcribed) from the 8 original wordmark lines,
 * split into three 14-column color bands per row -- same appearance as
 * the original fixed-3-band renderer, just expressed as runs.
 */
static const logo_run_t default_runs[] = {
    { 0, 0, "88888888ba,   ", ATTR_YELLOW },
    { 14, 0, "  ,ad8888ba,  ", ATTR_LIGHTBLUE },
    { 28, 0, "  ad88888ba   ", ATTR_LIGHTRED },
    { 0, 1, "88      `\"8b  ", ATTR_YELLOW },
    { 14, 1, " d8\"'    `\"8b ", ATTR_LIGHTBLUE },
    { 28, 1, " d8\"     \"8b  ", ATTR_LIGHTRED },
    { 0, 2, "88        `8b ", ATTR_YELLOW },
    { 14, 2, "d8'        `8b", ATTR_LIGHTBLUE },
    { 28, 2, " Y8,          ", ATTR_LIGHTRED },
    { 0, 3, "88         88 ", ATTR_YELLOW },
    { 14, 3, "88          88", ATTR_LIGHTBLUE },
    { 28, 3, " `Y8aaaaa,    ", ATTR_LIGHTRED },
    { 0, 4, "88         88 ", ATTR_YELLOW },
    { 14, 4, "88          88", ATTR_LIGHTBLUE },
    { 28, 4, "   `\"\"\"\"\"8b,  ", ATTR_LIGHTRED },
    { 0, 5, "88         8P ", ATTR_YELLOW },
    { 14, 5, "Y8,        ,8P", ATTR_LIGHTBLUE },
    { 28, 5, "         `8b  ", ATTR_LIGHTRED },
    { 0, 6, "88      .a8P  ", ATTR_YELLOW },
    { 14, 6, " Y8a.    .a8P ", ATTR_LIGHTBLUE },
    { 28, 6, " Y8a     a8P  ", ATTR_LIGHTRED },
    { 0, 7, "88888888Y\"'   ", ATTR_YELLOW },
    { 14, 7, "  `\"Y8888Y\"'  ", ATTR_LIGHTBLUE },
    { 28, 7, "  \"Y88888P\"   ", ATTR_LIGHTRED },
};
static const logo_t logo_default = { default_runs, sizeof(default_runs) / sizeof(default_runs[0]) };

/* Stacked "MS" (small, top, gray) over "DOS" (bigger, bottom, D=red
 * O=magenta S=yellow) -- matching the classic MS-DOS badge's two-line
 * layout and color story, rather than a single left-to-right wordmark.
 * Per-letter block-character bitmaps generated from a small 5-wide
 * font (3 rows tall for MS, 5 rows tall for DOS) rather than
 * hand-transcribed, to avoid transcription errors -- see the project's
 * dev notes for the generator.
 */
static const logo_run_t msdos_runs[] = {
    { 15, 0, "\xDB   \xDB", ATTR_LIGHTGRAY },
    { 15, 1, "\xDB\xDB \xDB\xDB", ATTR_LIGHTGRAY },
    { 15, 2, "\xDB   \xDB", ATTR_LIGHTGRAY },
    { 21, 0, " \xDB\xDB\xDB\xDB", ATTR_LIGHTGRAY },
    { 21, 1, "  \xDB\xDB ", ATTR_LIGHTGRAY },
    { 21, 2, "\xDB\xDB\xDB\xDB ", ATTR_LIGHTGRAY },
    { 12, 3, "\xDB\xDB\xDB\xDB ", ATTR_LIGHTRED },
    { 12, 4, "\xDB   \xDB", ATTR_LIGHTRED },
    { 12, 5, "\xDB   \xDB", ATTR_LIGHTRED },
    { 12, 6, "\xDB   \xDB", ATTR_LIGHTRED },
    { 12, 7, "\xDB\xDB\xDB\xDB ", ATTR_LIGHTRED },
    { 18, 3, " \xDB\xDB\xDB ", ATTR_LIGHTMAGENTA },
    { 18, 4, "\xDB   \xDB", ATTR_LIGHTMAGENTA },
    { 18, 5, "\xDB   \xDB", ATTR_LIGHTMAGENTA },
    { 18, 6, "\xDB   \xDB", ATTR_LIGHTMAGENTA },
    { 18, 7, " \xDB\xDB\xDB ", ATTR_LIGHTMAGENTA },
    { 24, 3, " \xDB\xDB\xDB\xDB", ATTR_YELLOW },
    { 24, 4, "\xDB    ", ATTR_YELLOW },
    { 24, 5, " \xDB\xDB\xDB ", ATTR_YELLOW },
    { 24, 6, "    \xDB", ATTR_YELLOW },
    { 24, 7, "\xDB\xDB\xDB\xDB ", ATTR_YELLOW },
};
static const logo_t logo_msdos = { msdos_runs, sizeof(msdos_runs) / sizeof(msdos_runs[0]) };

/* Generated the same way as the default logo's runs (see above), from
 * the original 8 FreeDOS wordmark lines split into three color bands.
 */
static const logo_run_t freedos_runs[] = {
    { 0, 1, " \xDB\xDB\xDB\xDB\xDB \xDB\xDB\xDB\xDB  \xDB", ATTR_LIGHTBLUE },
    { 14, 1, "\xDB\xDB\xDB\xDB \xDB\xDB\xDB\xDB\xDB \xDB\xDB\xDB", ATTR_LIGHTBLUE },
    { 28, 1, "\xDB   \xDB\xDB\xDB   \xDB\xDB\xDB\xDB", ATTR_WHITE },
    { 0, 2, " \xDB     \xDB   \xDB \xDB", ATTR_LIGHTBLUE },
    { 14, 2, "     \xDB     \xDB  ", ATTR_LIGHTBLUE },
    { 28, 2, " \xDB \xDB   \xDB \xDB    ", ATTR_WHITE },
    { 0, 3, " \xDB     \xDB   \xDB \xDB", ATTR_LIGHTBLUE },
    { 14, 3, "     \xDB     \xDB  ", ATTR_LIGHTBLUE },
    { 28, 3, " \xDB \xDB   \xDB \xDB    ", ATTR_WHITE },
    { 0, 4, " \xDB\xDB\xDB\xDB  \xDB\xDB\xDB\xDB  \xDB", ATTR_LIGHTBLUE },
    { 14, 4, "\xDB\xDB\xDB  \xDB\xDB\xDB\xDB  \xDB  ", ATTR_LIGHTBLUE },
    { 28, 4, " \xDB \xDB   \xDB  \xDB\xDB\xDB ", ATTR_WHITE },
    { 0, 5, " \xDB     \xDB \xDB   \xDB", ATTR_LIGHTBLUE },
    { 14, 5, "     \xDB     \xDB  ", ATTR_LIGHTBLUE },
    { 28, 5, " \xDB \xDB   \xDB     \xDB", ATTR_WHITE },
    { 0, 6, " \xDB     \xDB  \xDB  \xDB", ATTR_LIGHTBLUE },
    { 14, 6, "     \xDB     \xDB  ", ATTR_LIGHTBLUE },
    { 28, 6, " \xDB \xDB   \xDB     \xDB", ATTR_WHITE },
    { 0, 7, " \xDB     \xDB   \xDB \xDB", ATTR_LIGHTBLUE },
    { 14, 7, "\xDB\xDB\xDB\xDB \xDB\xDB\xDB\xDB\xDB \xDB\xDB\xDB", ATTR_LIGHTBLUE },
    { 28, 7, "\xDB   \xDB\xDB\xDB  \xDB\xDB\xDB\xDB ", ATTR_WHITE },
};
static const logo_t logo_freedos = { freedos_runs, sizeof(freedos_runs) / sizeof(freedos_runs[0]) };

static const logo_t *pick_logo(dos_vendor_t vendor)
{
    switch (vendor) {
        case DOS_VENDOR_MS:      return &logo_msdos;
        case DOS_VENDOR_FREEDOS: return &logo_freedos;
        default:                 return &logo_default;
    }
}

static void print_logo(int col, int row, const logo_t *logo)
{
    int i;

    for (i = 0; i < logo->run_count; i++) {
        const logo_run_t *r = &logo->runs[i];
        term_puts(col + r->col, row + r->row, r->text, r->attr);
    }
}

static void print_field(int col, int row, const char *label, const char *value,
                         unsigned char label_attr, unsigned char value_attr)
{
    char text[80];
    int len;

    sprintf(text, "%s: ", label);
    term_puts(col, row, text, label_attr);

    len = (int)strlen(text);
    term_puts(col + len, row, value, value_attr);
}

void render_screen(const field_t *fields, int count, int show_logo, int plain,
                    dos_vendor_t vendor)
{
    int i;
    unsigned char label_attr = plain ? ATTR_NORMAL : ATTR_WHITE;
    int last_row = count;

    term_clear(ATTR_NORMAL);

    if (show_logo) {
        print_logo(LOGO_COL, ROW_START, pick_logo(vendor));
        if (LOGO_ROWS > last_row)
            last_row = LOGO_ROWS;
    }

    for (i = 0; i < count; i++) {
        /* Only rows actually beside the logo need to leave room for it --
         * once past the logo's own height, use the full screen width.
         * Without this, every field past the 8th (the vast majority of
         * them, now that there are ~19 fields) was needlessly squeezed
         * into the narrow 36-column info strip and got clipped.
         */
        int field_col = (show_logo && i < LOGO_ROWS) ? INFO_COL : LOGO_COL;

        print_field(field_col, ROW_START + i, fields[i].label, fields[i].value,
                    label_attr, ATTR_NORMAL);
    }

    term_set_cursor(0, ROW_START + last_row);
}
