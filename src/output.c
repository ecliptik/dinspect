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

/* Standard DOS text-mode attribute values (background black throughout). */
#define ATTR_YELLOW       14
#define ATTR_LIGHTBLUE     9
#define ATTR_LIGHTRED     12
#define ATTR_LIGHTGRAY     7
#define ATTR_LIGHTMAGENTA 13
#define ATTR_WHITE        15
#define ATTR_RED           4
#define ATTR_BLUE          1
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
    int height; /* rows this logo occupies, so the field column starts
                  * below it even when a given logo is taller/shorter
                  * than the others */
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
static const logo_t logo_default = { default_runs, sizeof(default_runs) / sizeof(default_runs[0]), 8 };

/* "MS" (small, top, gray, 7-row font) over "DOS" (big, bottom, 9-row
 * letterforms, D=red, O=magenta/blue checkerboard, S=yellow) --
 * inspired by the layout and per-letter overlap of EmgrtE/ascii-ansi's
 * ms-dos badge (see THIRD-PARTY.md; unlicensed upstream, used as a
 * design reference only, nothing copied) while keeping this project's
 * own color story. D and O overlap, and O and S overlap, each woven so
 * the earlier/later letter in the badge (D at the D-O junction, S at
 * the O-S junction) wins along most of the shared columns but the
 * other letter's curl/bottom pokes back in front -- matching the
 * original logo's interlocking look rather than a flat overlap. MS
 * uses a shorter font than DOS (legs trimmed) so it reads as clearly
 * smaller. Per-letter bitmaps generated from a small font rather than
 * hand-transcribed, to avoid transcription errors -- see the project's
 * dev notes for the generator.
 */
static const logo_run_t msdos_runs[] = {
    { 0, 7, "\xDB", ATTR_LIGHTRED },
    { 1, 7, "\xDB", ATTR_LIGHTRED },
    { 2, 7, "\xDB", ATTR_LIGHTRED },
    { 3, 7, "\xDB", ATTR_LIGHTRED },
    { 4, 7, "\xDB", ATTR_LIGHTRED },
    { 5, 7, "\xDB", ATTR_LIGHTRED },
    { 6, 7, "\xDB", ATTR_LIGHTRED },
    { 0, 8, "\xDB", ATTR_LIGHTRED },
    { 7, 8, "\xDB", ATTR_LIGHTRED },
    { 0, 9, "\xDB", ATTR_LIGHTRED },
    { 8, 9, "\xDB", ATTR_LIGHTRED },
    { 0, 10, "\xDB", ATTR_LIGHTRED },
    { 8, 10, "\xDB", ATTR_LIGHTRED },
    { 0, 11, "\xDB", ATTR_LIGHTRED },
    { 8, 11, "\xDB", ATTR_LIGHTRED },
    { 0, 12, "\xDB", ATTR_LIGHTRED },
    { 8, 12, "\xDB", ATTR_LIGHTRED },
    { 0, 13, "\xDB", ATTR_LIGHTRED },
    { 8, 13, "\xDB", ATTR_LIGHTRED },
    { 14, 11, "\xDB", ATTR_YELLOW },
    { 15, 11, "\xDB", ATTR_YELLOW },
    { 16, 11, "\xDB", ATTR_YELLOW },
    { 17, 11, "\xDB", ATTR_YELLOW },
    { 18, 11, "\xDB", ATTR_YELLOW },
    { 20, 12, "\xDB", ATTR_YELLOW },
    { 20, 13, "\xDB", ATTR_YELLOW },
    { 8, 7, "\xDB", ATTR_LIGHTMAGENTA },
    { 9, 7, "\xDB", ATTR_LIGHTBLUE },
    { 10, 7, "\xDB", ATTR_LIGHTMAGENTA },
    { 11, 7, "\xDB", ATTR_LIGHTBLUE },
    { 12, 7, "\xDB", ATTR_LIGHTMAGENTA },
    { 6, 8, "\xDB", ATTR_LIGHTBLUE },
    { 7, 8, "\xDB", ATTR_LIGHTMAGENTA },
    { 13, 8, "\xDB", ATTR_LIGHTMAGENTA },
    { 14, 8, "\xDB", ATTR_LIGHTBLUE },
    { 6, 9, "\xDB", ATTR_LIGHTMAGENTA },
    { 14, 9, "\xDB", ATTR_LIGHTMAGENTA },
    { 6, 10, "\xDB", ATTR_LIGHTBLUE },
    { 14, 10, "\xDB", ATTR_LIGHTBLUE },
    { 6, 11, "\xDB", ATTR_LIGHTMAGENTA },
    { 14, 11, "\xDB", ATTR_LIGHTMAGENTA },
    { 6, 12, "\xDB", ATTR_LIGHTBLUE },
    { 14, 12, "\xDB", ATTR_LIGHTBLUE },
    { 6, 13, "\xDB", ATTR_LIGHTMAGENTA },
    { 14, 13, "\xDB", ATTR_LIGHTMAGENTA },
    { 6, 14, "\xDB", ATTR_LIGHTBLUE },
    { 7, 14, "\xDB", ATTR_LIGHTMAGENTA },
    { 13, 14, "\xDB", ATTR_LIGHTMAGENTA },
    { 14, 14, "\xDB", ATTR_LIGHTBLUE },
    { 8, 15, "\xDB", ATTR_LIGHTMAGENTA },
    { 9, 15, "\xDB", ATTR_LIGHTBLUE },
    { 10, 15, "\xDB", ATTR_LIGHTMAGENTA },
    { 11, 15, "\xDB", ATTR_LIGHTBLUE },
    { 12, 15, "\xDB", ATTR_LIGHTMAGENTA },
    { 0, 14, "\xDB", ATTR_LIGHTRED },
    { 7, 14, "\xDB", ATTR_LIGHTRED },
    { 0, 15, "\xDB", ATTR_LIGHTRED },
    { 1, 15, "\xDB", ATTR_LIGHTRED },
    { 2, 15, "\xDB", ATTR_LIGHTRED },
    { 3, 15, "\xDB", ATTR_LIGHTRED },
    { 4, 15, "\xDB", ATTR_LIGHTRED },
    { 5, 15, "\xDB", ATTR_LIGHTRED },
    { 6, 15, "\xDB", ATTR_LIGHTRED },
    { 14, 7, "\xDB", ATTR_YELLOW },
    { 15, 7, "\xDB", ATTR_YELLOW },
    { 16, 7, "\xDB", ATTR_YELLOW },
    { 17, 7, "\xDB", ATTR_YELLOW },
    { 18, 7, "\xDB", ATTR_YELLOW },
    { 12, 8, "\xDB", ATTR_YELLOW },
    { 13, 8, "\xDB", ATTR_YELLOW },
    { 19, 8, "\xDB", ATTR_YELLOW },
    { 20, 8, "\xDB", ATTR_YELLOW },
    { 12, 9, "\xDB", ATTR_YELLOW },
    { 12, 10, "\xDB", ATTR_YELLOW },
    { 12, 14, "\xDB", ATTR_YELLOW },
    { 13, 14, "\xDB", ATTR_YELLOW },
    { 19, 14, "\xDB", ATTR_YELLOW },
    { 20, 14, "\xDB", ATTR_YELLOW },
    { 14, 15, "\xDB", ATTR_YELLOW },
    { 15, 15, "\xDB", ATTR_YELLOW },
    { 16, 15, "\xDB", ATTR_YELLOW },
    { 17, 15, "\xDB", ATTR_YELLOW },
    { 18, 15, "\xDB", ATTR_YELLOW },
    { 1, 0, "\xDB", ATTR_LIGHTGRAY },
    { 2, 0, "\xDB", ATTR_LIGHTGRAY },
    { 9, 0, "\xDB", ATTR_LIGHTGRAY },
    { 10, 0, "\xDB", ATTR_LIGHTGRAY },
    { 1, 1, "\xDB", ATTR_LIGHTGRAY },
    { 2, 1, "\xDB", ATTR_LIGHTGRAY },
    { 3, 1, "\xDB", ATTR_LIGHTGRAY },
    { 8, 1, "\xDB", ATTR_LIGHTGRAY },
    { 9, 1, "\xDB", ATTR_LIGHTGRAY },
    { 10, 1, "\xDB", ATTR_LIGHTGRAY },
    { 1, 2, "\xDB", ATTR_LIGHTGRAY },
    { 3, 2, "\xDB", ATTR_LIGHTGRAY },
    { 4, 2, "\xDB", ATTR_LIGHTGRAY },
    { 7, 2, "\xDB", ATTR_LIGHTGRAY },
    { 8, 2, "\xDB", ATTR_LIGHTGRAY },
    { 10, 2, "\xDB", ATTR_LIGHTGRAY },
    { 1, 3, "\xDB", ATTR_LIGHTGRAY },
    { 4, 3, "\xDB", ATTR_LIGHTGRAY },
    { 5, 3, "\xDB", ATTR_LIGHTGRAY },
    { 6, 3, "\xDB", ATTR_LIGHTGRAY },
    { 7, 3, "\xDB", ATTR_LIGHTGRAY },
    { 10, 3, "\xDB", ATTR_LIGHTGRAY },
    { 1, 4, "\xDB", ATTR_LIGHTGRAY },
    { 5, 4, "\xDB", ATTR_LIGHTGRAY },
    { 6, 4, "\xDB", ATTR_LIGHTGRAY },
    { 10, 4, "\xDB", ATTR_LIGHTGRAY },
    { 1, 5, "\xDB", ATTR_LIGHTGRAY },
    { 10, 5, "\xDB", ATTR_LIGHTGRAY },
    { 1, 6, "\xDB", ATTR_LIGHTGRAY },
    { 10, 6, "\xDB", ATTR_LIGHTGRAY },
    { 13, 0, "\xDB", ATTR_LIGHTGRAY },
    { 14, 0, "\xDB", ATTR_LIGHTGRAY },
    { 15, 0, "\xDB", ATTR_LIGHTGRAY },
    { 16, 0, "\xDB", ATTR_LIGHTGRAY },
    { 17, 0, "\xDB", ATTR_LIGHTGRAY },
    { 11, 1, "\xDB", ATTR_LIGHTGRAY },
    { 12, 1, "\xDB", ATTR_LIGHTGRAY },
    { 18, 1, "\xDB", ATTR_LIGHTGRAY },
    { 19, 1, "\xDB", ATTR_LIGHTGRAY },
    { 11, 2, "\xDB", ATTR_LIGHTGRAY },
    { 13, 3, "\xDB", ATTR_LIGHTGRAY },
    { 14, 3, "\xDB", ATTR_LIGHTGRAY },
    { 15, 3, "\xDB", ATTR_LIGHTGRAY },
    { 16, 3, "\xDB", ATTR_LIGHTGRAY },
    { 17, 3, "\xDB", ATTR_LIGHTGRAY },
    { 19, 4, "\xDB", ATTR_LIGHTGRAY },
    { 11, 5, "\xDB", ATTR_LIGHTGRAY },
    { 12, 5, "\xDB", ATTR_LIGHTGRAY },
    { 18, 5, "\xDB", ATTR_LIGHTGRAY },
    { 19, 5, "\xDB", ATTR_LIGHTGRAY },
    { 13, 6, "\xDB", ATTR_LIGHTGRAY },
    { 14, 6, "\xDB", ATTR_LIGHTGRAY },
    { 15, 6, "\xDB", ATTR_LIGHTGRAY },
    { 16, 6, "\xDB", ATTR_LIGHTGRAY },
    { 17, 6, "\xDB", ATTR_LIGHTGRAY },
};
static const logo_t logo_msdos = { msdos_runs, sizeof(msdos_runs) / sizeof(msdos_runs[0]), 16 };

/* "FREE" (small, top, blue) over "DOS" (big, bottom, white, same
 * 9-wide font as the MS-DOS badge above, but plain letter spacing --
 * unlike MS-DOS, DOS here doesn't use the interlocking overlap)
 * -- mirrors the MS-DOS badge's two-line layout. FREE uses a narrower
 * 5-wide condensed font, centered over DOS. Generated, not
 * hand-transcribed -- see msdos_runs.
 */
static const logo_run_t freedos_runs[] = {
    { 0, 9, "\xDB", ATTR_WHITE },
    { 1, 9, "\xDB", ATTR_WHITE },
    { 2, 9, "\xDB", ATTR_WHITE },
    { 3, 9, "\xDB", ATTR_WHITE },
    { 4, 9, "\xDB", ATTR_WHITE },
    { 5, 9, "\xDB", ATTR_WHITE },
    { 6, 9, "\xDB", ATTR_WHITE },
    { 0, 10, "\xDB", ATTR_WHITE },
    { 7, 10, "\xDB", ATTR_WHITE },
    { 0, 11, "\xDB", ATTR_WHITE },
    { 8, 11, "\xDB", ATTR_WHITE },
    { 0, 12, "\xDB", ATTR_WHITE },
    { 8, 12, "\xDB", ATTR_WHITE },
    { 0, 13, "\xDB", ATTR_WHITE },
    { 8, 13, "\xDB", ATTR_WHITE },
    { 0, 14, "\xDB", ATTR_WHITE },
    { 8, 14, "\xDB", ATTR_WHITE },
    { 0, 15, "\xDB", ATTR_WHITE },
    { 8, 15, "\xDB", ATTR_WHITE },
    { 0, 16, "\xDB", ATTR_WHITE },
    { 7, 16, "\xDB", ATTR_WHITE },
    { 0, 17, "\xDB", ATTR_WHITE },
    { 1, 17, "\xDB", ATTR_WHITE },
    { 2, 17, "\xDB", ATTR_WHITE },
    { 3, 17, "\xDB", ATTR_WHITE },
    { 4, 17, "\xDB", ATTR_WHITE },
    { 5, 17, "\xDB", ATTR_WHITE },
    { 6, 17, "\xDB", ATTR_WHITE },
    { 12, 9, "\xDB", ATTR_WHITE },
    { 13, 9, "\xDB", ATTR_WHITE },
    { 14, 9, "\xDB", ATTR_WHITE },
    { 15, 9, "\xDB", ATTR_WHITE },
    { 16, 9, "\xDB", ATTR_WHITE },
    { 10, 10, "\xDB", ATTR_WHITE },
    { 11, 10, "\xDB", ATTR_WHITE },
    { 17, 10, "\xDB", ATTR_WHITE },
    { 18, 10, "\xDB", ATTR_WHITE },
    { 10, 11, "\xDB", ATTR_WHITE },
    { 18, 11, "\xDB", ATTR_WHITE },
    { 10, 12, "\xDB", ATTR_WHITE },
    { 18, 12, "\xDB", ATTR_WHITE },
    { 10, 13, "\xDB", ATTR_WHITE },
    { 18, 13, "\xDB", ATTR_WHITE },
    { 10, 14, "\xDB", ATTR_WHITE },
    { 18, 14, "\xDB", ATTR_WHITE },
    { 10, 15, "\xDB", ATTR_WHITE },
    { 18, 15, "\xDB", ATTR_WHITE },
    { 10, 16, "\xDB", ATTR_WHITE },
    { 11, 16, "\xDB", ATTR_WHITE },
    { 17, 16, "\xDB", ATTR_WHITE },
    { 18, 16, "\xDB", ATTR_WHITE },
    { 12, 17, "\xDB", ATTR_WHITE },
    { 13, 17, "\xDB", ATTR_WHITE },
    { 14, 17, "\xDB", ATTR_WHITE },
    { 15, 17, "\xDB", ATTR_WHITE },
    { 16, 17, "\xDB", ATTR_WHITE },
    { 22, 9, "\xDB", ATTR_WHITE },
    { 23, 9, "\xDB", ATTR_WHITE },
    { 24, 9, "\xDB", ATTR_WHITE },
    { 25, 9, "\xDB", ATTR_WHITE },
    { 26, 9, "\xDB", ATTR_WHITE },
    { 20, 10, "\xDB", ATTR_WHITE },
    { 21, 10, "\xDB", ATTR_WHITE },
    { 27, 10, "\xDB", ATTR_WHITE },
    { 28, 10, "\xDB", ATTR_WHITE },
    { 20, 11, "\xDB", ATTR_WHITE },
    { 20, 12, "\xDB", ATTR_WHITE },
    { 22, 13, "\xDB", ATTR_WHITE },
    { 23, 13, "\xDB", ATTR_WHITE },
    { 24, 13, "\xDB", ATTR_WHITE },
    { 25, 13, "\xDB", ATTR_WHITE },
    { 26, 13, "\xDB", ATTR_WHITE },
    { 28, 14, "\xDB", ATTR_WHITE },
    { 28, 15, "\xDB", ATTR_WHITE },
    { 20, 16, "\xDB", ATTR_WHITE },
    { 21, 16, "\xDB", ATTR_WHITE },
    { 27, 16, "\xDB", ATTR_WHITE },
    { 28, 16, "\xDB", ATTR_WHITE },
    { 22, 17, "\xDB", ATTR_WHITE },
    { 23, 17, "\xDB", ATTR_WHITE },
    { 24, 17, "\xDB", ATTR_WHITE },
    { 25, 17, "\xDB", ATTR_WHITE },
    { 26, 17, "\xDB", ATTR_WHITE },
    { 3, 0, "\xDB", ATTR_LIGHTBLUE },
    { 4, 0, "\xDB", ATTR_LIGHTBLUE },
    { 5, 0, "\xDB", ATTR_LIGHTBLUE },
    { 6, 0, "\xDB", ATTR_LIGHTBLUE },
    { 7, 0, "\xDB", ATTR_LIGHTBLUE },
    { 3, 1, "\xDB", ATTR_LIGHTBLUE },
    { 3, 2, "\xDB", ATTR_LIGHTBLUE },
    { 3, 3, "\xDB", ATTR_LIGHTBLUE },
    { 4, 3, "\xDB", ATTR_LIGHTBLUE },
    { 5, 3, "\xDB", ATTR_LIGHTBLUE },
    { 6, 3, "\xDB", ATTR_LIGHTBLUE },
    { 3, 4, "\xDB", ATTR_LIGHTBLUE },
    { 3, 5, "\xDB", ATTR_LIGHTBLUE },
    { 3, 6, "\xDB", ATTR_LIGHTBLUE },
    { 3, 7, "\xDB", ATTR_LIGHTBLUE },
    { 3, 8, "\xDB", ATTR_LIGHTBLUE },
    { 9, 0, "\xDB", ATTR_LIGHTBLUE },
    { 10, 0, "\xDB", ATTR_LIGHTBLUE },
    { 11, 0, "\xDB", ATTR_LIGHTBLUE },
    { 12, 0, "\xDB", ATTR_LIGHTBLUE },
    { 9, 1, "\xDB", ATTR_LIGHTBLUE },
    { 13, 1, "\xDB", ATTR_LIGHTBLUE },
    { 9, 2, "\xDB", ATTR_LIGHTBLUE },
    { 13, 2, "\xDB", ATTR_LIGHTBLUE },
    { 9, 3, "\xDB", ATTR_LIGHTBLUE },
    { 10, 3, "\xDB", ATTR_LIGHTBLUE },
    { 11, 3, "\xDB", ATTR_LIGHTBLUE },
    { 12, 3, "\xDB", ATTR_LIGHTBLUE },
    { 9, 4, "\xDB", ATTR_LIGHTBLUE },
    { 12, 4, "\xDB", ATTR_LIGHTBLUE },
    { 9, 5, "\xDB", ATTR_LIGHTBLUE },
    { 13, 5, "\xDB", ATTR_LIGHTBLUE },
    { 9, 6, "\xDB", ATTR_LIGHTBLUE },
    { 13, 6, "\xDB", ATTR_LIGHTBLUE },
    { 9, 7, "\xDB", ATTR_LIGHTBLUE },
    { 13, 7, "\xDB", ATTR_LIGHTBLUE },
    { 9, 8, "\xDB", ATTR_LIGHTBLUE },
    { 13, 8, "\xDB", ATTR_LIGHTBLUE },
    { 15, 0, "\xDB", ATTR_LIGHTBLUE },
    { 16, 0, "\xDB", ATTR_LIGHTBLUE },
    { 17, 0, "\xDB", ATTR_LIGHTBLUE },
    { 18, 0, "\xDB", ATTR_LIGHTBLUE },
    { 19, 0, "\xDB", ATTR_LIGHTBLUE },
    { 15, 1, "\xDB", ATTR_LIGHTBLUE },
    { 15, 2, "\xDB", ATTR_LIGHTBLUE },
    { 15, 3, "\xDB", ATTR_LIGHTBLUE },
    { 15, 4, "\xDB", ATTR_LIGHTBLUE },
    { 16, 4, "\xDB", ATTR_LIGHTBLUE },
    { 17, 4, "\xDB", ATTR_LIGHTBLUE },
    { 18, 4, "\xDB", ATTR_LIGHTBLUE },
    { 19, 4, "\xDB", ATTR_LIGHTBLUE },
    { 15, 5, "\xDB", ATTR_LIGHTBLUE },
    { 15, 6, "\xDB", ATTR_LIGHTBLUE },
    { 15, 7, "\xDB", ATTR_LIGHTBLUE },
    { 15, 8, "\xDB", ATTR_LIGHTBLUE },
    { 16, 8, "\xDB", ATTR_LIGHTBLUE },
    { 17, 8, "\xDB", ATTR_LIGHTBLUE },
    { 18, 8, "\xDB", ATTR_LIGHTBLUE },
    { 19, 8, "\xDB", ATTR_LIGHTBLUE },
    { 21, 0, "\xDB", ATTR_LIGHTBLUE },
    { 22, 0, "\xDB", ATTR_LIGHTBLUE },
    { 23, 0, "\xDB", ATTR_LIGHTBLUE },
    { 24, 0, "\xDB", ATTR_LIGHTBLUE },
    { 25, 0, "\xDB", ATTR_LIGHTBLUE },
    { 21, 1, "\xDB", ATTR_LIGHTBLUE },
    { 21, 2, "\xDB", ATTR_LIGHTBLUE },
    { 21, 3, "\xDB", ATTR_LIGHTBLUE },
    { 21, 4, "\xDB", ATTR_LIGHTBLUE },
    { 22, 4, "\xDB", ATTR_LIGHTBLUE },
    { 23, 4, "\xDB", ATTR_LIGHTBLUE },
    { 24, 4, "\xDB", ATTR_LIGHTBLUE },
    { 25, 4, "\xDB", ATTR_LIGHTBLUE },
    { 21, 5, "\xDB", ATTR_LIGHTBLUE },
    { 21, 6, "\xDB", ATTR_LIGHTBLUE },
    { 21, 7, "\xDB", ATTR_LIGHTBLUE },
    { 21, 8, "\xDB", ATTR_LIGHTBLUE },
    { 22, 8, "\xDB", ATTR_LIGHTBLUE },
    { 23, 8, "\xDB", ATTR_LIGHTBLUE },
    { 24, 8, "\xDB", ATTR_LIGHTBLUE },
    { 25, 8, "\xDB", ATTR_LIGHTBLUE },
};
static const logo_t logo_freedos = { freedos_runs, sizeof(freedos_runs) / sizeof(freedos_runs[0]), 18 };

/* "DR" (top) over "DOS" (bottom), Digital Research's dark maroon
 * throughout -- matching their corporate wordmark colors, not the
 * DR DOS product packaging (which is mostly monochrome). Same
 * two-line layout and plain (non-overlapping) letter spacing as the
 * FreeDOS badge. Generated, not hand-transcribed -- see msdos_runs.
 */
static const logo_run_t drdos_runs[] = {
    { 0, 7, "\xDB", ATTR_RED },
    { 1, 7, "\xDB", ATTR_RED },
    { 2, 7, "\xDB", ATTR_RED },
    { 3, 7, "\xDB", ATTR_RED },
    { 4, 7, "\xDB", ATTR_RED },
    { 5, 7, "\xDB", ATTR_RED },
    { 6, 7, "\xDB", ATTR_RED },
    { 0, 8, "\xDB", ATTR_RED },
    { 7, 8, "\xDB", ATTR_RED },
    { 0, 9, "\xDB", ATTR_RED },
    { 8, 9, "\xDB", ATTR_RED },
    { 0, 10, "\xDB", ATTR_RED },
    { 8, 10, "\xDB", ATTR_RED },
    { 0, 11, "\xDB", ATTR_RED },
    { 8, 11, "\xDB", ATTR_RED },
    { 0, 12, "\xDB", ATTR_RED },
    { 8, 12, "\xDB", ATTR_RED },
    { 0, 13, "\xDB", ATTR_RED },
    { 8, 13, "\xDB", ATTR_RED },
    { 0, 14, "\xDB", ATTR_RED },
    { 7, 14, "\xDB", ATTR_RED },
    { 0, 15, "\xDB", ATTR_RED },
    { 1, 15, "\xDB", ATTR_RED },
    { 2, 15, "\xDB", ATTR_RED },
    { 3, 15, "\xDB", ATTR_RED },
    { 4, 15, "\xDB", ATTR_RED },
    { 5, 15, "\xDB", ATTR_RED },
    { 6, 15, "\xDB", ATTR_RED },
    { 12, 7, "\xDB", ATTR_RED },
    { 13, 7, "\xDB", ATTR_RED },
    { 14, 7, "\xDB", ATTR_RED },
    { 15, 7, "\xDB", ATTR_RED },
    { 16, 7, "\xDB", ATTR_RED },
    { 10, 8, "\xDB", ATTR_RED },
    { 11, 8, "\xDB", ATTR_RED },
    { 17, 8, "\xDB", ATTR_RED },
    { 18, 8, "\xDB", ATTR_RED },
    { 10, 9, "\xDB", ATTR_RED },
    { 18, 9, "\xDB", ATTR_RED },
    { 10, 10, "\xDB", ATTR_RED },
    { 18, 10, "\xDB", ATTR_RED },
    { 10, 11, "\xDB", ATTR_RED },
    { 18, 11, "\xDB", ATTR_RED },
    { 10, 12, "\xDB", ATTR_RED },
    { 18, 12, "\xDB", ATTR_RED },
    { 10, 13, "\xDB", ATTR_RED },
    { 18, 13, "\xDB", ATTR_RED },
    { 10, 14, "\xDB", ATTR_RED },
    { 11, 14, "\xDB", ATTR_RED },
    { 17, 14, "\xDB", ATTR_RED },
    { 18, 14, "\xDB", ATTR_RED },
    { 12, 15, "\xDB", ATTR_RED },
    { 13, 15, "\xDB", ATTR_RED },
    { 14, 15, "\xDB", ATTR_RED },
    { 15, 15, "\xDB", ATTR_RED },
    { 16, 15, "\xDB", ATTR_RED },
    { 22, 7, "\xDB", ATTR_RED },
    { 23, 7, "\xDB", ATTR_RED },
    { 24, 7, "\xDB", ATTR_RED },
    { 25, 7, "\xDB", ATTR_RED },
    { 26, 7, "\xDB", ATTR_RED },
    { 20, 8, "\xDB", ATTR_RED },
    { 21, 8, "\xDB", ATTR_RED },
    { 27, 8, "\xDB", ATTR_RED },
    { 28, 8, "\xDB", ATTR_RED },
    { 20, 9, "\xDB", ATTR_RED },
    { 20, 10, "\xDB", ATTR_RED },
    { 22, 11, "\xDB", ATTR_RED },
    { 23, 11, "\xDB", ATTR_RED },
    { 24, 11, "\xDB", ATTR_RED },
    { 25, 11, "\xDB", ATTR_RED },
    { 26, 11, "\xDB", ATTR_RED },
    { 28, 12, "\xDB", ATTR_RED },
    { 28, 13, "\xDB", ATTR_RED },
    { 20, 14, "\xDB", ATTR_RED },
    { 21, 14, "\xDB", ATTR_RED },
    { 27, 14, "\xDB", ATTR_RED },
    { 28, 14, "\xDB", ATTR_RED },
    { 22, 15, "\xDB", ATTR_RED },
    { 23, 15, "\xDB", ATTR_RED },
    { 24, 15, "\xDB", ATTR_RED },
    { 25, 15, "\xDB", ATTR_RED },
    { 26, 15, "\xDB", ATTR_RED },
    { 5, 0, "\xDB", ATTR_RED },
    { 6, 0, "\xDB", ATTR_RED },
    { 7, 0, "\xDB", ATTR_RED },
    { 8, 0, "\xDB", ATTR_RED },
    { 9, 0, "\xDB", ATTR_RED },
    { 10, 0, "\xDB", ATTR_RED },
    { 11, 0, "\xDB", ATTR_RED },
    { 5, 1, "\xDB", ATTR_RED },
    { 12, 1, "\xDB", ATTR_RED },
    { 5, 2, "\xDB", ATTR_RED },
    { 13, 2, "\xDB", ATTR_RED },
    { 5, 3, "\xDB", ATTR_RED },
    { 13, 3, "\xDB", ATTR_RED },
    { 5, 4, "\xDB", ATTR_RED },
    { 13, 4, "\xDB", ATTR_RED },
    { 5, 5, "\xDB", ATTR_RED },
    { 12, 5, "\xDB", ATTR_RED },
    { 5, 6, "\xDB", ATTR_RED },
    { 6, 6, "\xDB", ATTR_RED },
    { 7, 6, "\xDB", ATTR_RED },
    { 8, 6, "\xDB", ATTR_RED },
    { 9, 6, "\xDB", ATTR_RED },
    { 10, 6, "\xDB", ATTR_RED },
    { 11, 6, "\xDB", ATTR_RED },
    { 15, 0, "\xDB", ATTR_RED },
    { 16, 0, "\xDB", ATTR_RED },
    { 17, 0, "\xDB", ATTR_RED },
    { 18, 0, "\xDB", ATTR_RED },
    { 19, 0, "\xDB", ATTR_RED },
    { 20, 0, "\xDB", ATTR_RED },
    { 21, 0, "\xDB", ATTR_RED },
    { 15, 1, "\xDB", ATTR_RED },
    { 22, 1, "\xDB", ATTR_RED },
    { 15, 2, "\xDB", ATTR_RED },
    { 22, 2, "\xDB", ATTR_RED },
    { 15, 3, "\xDB", ATTR_RED },
    { 16, 3, "\xDB", ATTR_RED },
    { 17, 3, "\xDB", ATTR_RED },
    { 18, 3, "\xDB", ATTR_RED },
    { 19, 3, "\xDB", ATTR_RED },
    { 20, 3, "\xDB", ATTR_RED },
    { 21, 3, "\xDB", ATTR_RED },
    { 15, 4, "\xDB", ATTR_RED },
    { 20, 4, "\xDB", ATTR_RED },
    { 15, 5, "\xDB", ATTR_RED },
    { 21, 5, "\xDB", ATTR_RED },
    { 15, 6, "\xDB", ATTR_RED },
    { 22, 6, "\xDB", ATTR_RED },
};
static const logo_t logo_drdos = { drdos_runs, sizeof(drdos_runs) / sizeof(drdos_runs[0]), 16 };

/* "IBM" (top) over "DOS" (bottom), IBM's corporate blue throughout --
 * an IBM-branded badge rather than the multicolor diagonal DOS 5.0
 * retail-box logo, since PC DOS is IBM's product first. Same
 * two-line layout and plain letter spacing as the FreeDOS/DR-DOS
 * badges. Generated, not hand-transcribed -- see msdos_runs.
 */
static const logo_run_t pcdos_runs[] = {
    { 0, 7, "\xDB", ATTR_BLUE },
    { 1, 7, "\xDB", ATTR_BLUE },
    { 2, 7, "\xDB", ATTR_BLUE },
    { 3, 7, "\xDB", ATTR_BLUE },
    { 4, 7, "\xDB", ATTR_BLUE },
    { 5, 7, "\xDB", ATTR_BLUE },
    { 6, 7, "\xDB", ATTR_BLUE },
    { 0, 8, "\xDB", ATTR_BLUE },
    { 7, 8, "\xDB", ATTR_BLUE },
    { 0, 9, "\xDB", ATTR_BLUE },
    { 8, 9, "\xDB", ATTR_BLUE },
    { 0, 10, "\xDB", ATTR_BLUE },
    { 8, 10, "\xDB", ATTR_BLUE },
    { 0, 11, "\xDB", ATTR_BLUE },
    { 8, 11, "\xDB", ATTR_BLUE },
    { 0, 12, "\xDB", ATTR_BLUE },
    { 8, 12, "\xDB", ATTR_BLUE },
    { 0, 13, "\xDB", ATTR_BLUE },
    { 8, 13, "\xDB", ATTR_BLUE },
    { 0, 14, "\xDB", ATTR_BLUE },
    { 7, 14, "\xDB", ATTR_BLUE },
    { 0, 15, "\xDB", ATTR_BLUE },
    { 1, 15, "\xDB", ATTR_BLUE },
    { 2, 15, "\xDB", ATTR_BLUE },
    { 3, 15, "\xDB", ATTR_BLUE },
    { 4, 15, "\xDB", ATTR_BLUE },
    { 5, 15, "\xDB", ATTR_BLUE },
    { 6, 15, "\xDB", ATTR_BLUE },
    { 12, 7, "\xDB", ATTR_BLUE },
    { 13, 7, "\xDB", ATTR_BLUE },
    { 14, 7, "\xDB", ATTR_BLUE },
    { 15, 7, "\xDB", ATTR_BLUE },
    { 16, 7, "\xDB", ATTR_BLUE },
    { 10, 8, "\xDB", ATTR_BLUE },
    { 11, 8, "\xDB", ATTR_BLUE },
    { 17, 8, "\xDB", ATTR_BLUE },
    { 18, 8, "\xDB", ATTR_BLUE },
    { 10, 9, "\xDB", ATTR_BLUE },
    { 18, 9, "\xDB", ATTR_BLUE },
    { 10, 10, "\xDB", ATTR_BLUE },
    { 18, 10, "\xDB", ATTR_BLUE },
    { 10, 11, "\xDB", ATTR_BLUE },
    { 18, 11, "\xDB", ATTR_BLUE },
    { 10, 12, "\xDB", ATTR_BLUE },
    { 18, 12, "\xDB", ATTR_BLUE },
    { 10, 13, "\xDB", ATTR_BLUE },
    { 18, 13, "\xDB", ATTR_BLUE },
    { 10, 14, "\xDB", ATTR_BLUE },
    { 11, 14, "\xDB", ATTR_BLUE },
    { 17, 14, "\xDB", ATTR_BLUE },
    { 18, 14, "\xDB", ATTR_BLUE },
    { 12, 15, "\xDB", ATTR_BLUE },
    { 13, 15, "\xDB", ATTR_BLUE },
    { 14, 15, "\xDB", ATTR_BLUE },
    { 15, 15, "\xDB", ATTR_BLUE },
    { 16, 15, "\xDB", ATTR_BLUE },
    { 22, 7, "\xDB", ATTR_BLUE },
    { 23, 7, "\xDB", ATTR_BLUE },
    { 24, 7, "\xDB", ATTR_BLUE },
    { 25, 7, "\xDB", ATTR_BLUE },
    { 26, 7, "\xDB", ATTR_BLUE },
    { 20, 8, "\xDB", ATTR_BLUE },
    { 21, 8, "\xDB", ATTR_BLUE },
    { 27, 8, "\xDB", ATTR_BLUE },
    { 28, 8, "\xDB", ATTR_BLUE },
    { 20, 9, "\xDB", ATTR_BLUE },
    { 20, 10, "\xDB", ATTR_BLUE },
    { 22, 11, "\xDB", ATTR_BLUE },
    { 23, 11, "\xDB", ATTR_BLUE },
    { 24, 11, "\xDB", ATTR_BLUE },
    { 25, 11, "\xDB", ATTR_BLUE },
    { 26, 11, "\xDB", ATTR_BLUE },
    { 28, 12, "\xDB", ATTR_BLUE },
    { 28, 13, "\xDB", ATTR_BLUE },
    { 20, 14, "\xDB", ATTR_BLUE },
    { 21, 14, "\xDB", ATTR_BLUE },
    { 27, 14, "\xDB", ATTR_BLUE },
    { 28, 14, "\xDB", ATTR_BLUE },
    { 22, 15, "\xDB", ATTR_BLUE },
    { 23, 15, "\xDB", ATTR_BLUE },
    { 24, 15, "\xDB", ATTR_BLUE },
    { 25, 15, "\xDB", ATTR_BLUE },
    { 26, 15, "\xDB", ATTR_BLUE },
    { 1, 0, "\xDB", ATTR_BLUE },
    { 2, 0, "\xDB", ATTR_BLUE },
    { 3, 0, "\xDB", ATTR_BLUE },
    { 4, 0, "\xDB", ATTR_BLUE },
    { 5, 0, "\xDB", ATTR_BLUE },
    { 3, 1, "\xDB", ATTR_BLUE },
    { 3, 2, "\xDB", ATTR_BLUE },
    { 3, 3, "\xDB", ATTR_BLUE },
    { 3, 4, "\xDB", ATTR_BLUE },
    { 3, 5, "\xDB", ATTR_BLUE },
    { 1, 6, "\xDB", ATTR_BLUE },
    { 2, 6, "\xDB", ATTR_BLUE },
    { 3, 6, "\xDB", ATTR_BLUE },
    { 4, 6, "\xDB", ATTR_BLUE },
    { 5, 6, "\xDB", ATTR_BLUE },
    { 7, 0, "\xDB", ATTR_BLUE },
    { 8, 0, "\xDB", ATTR_BLUE },
    { 9, 0, "\xDB", ATTR_BLUE },
    { 10, 0, "\xDB", ATTR_BLUE },
    { 11, 0, "\xDB", ATTR_BLUE },
    { 12, 0, "\xDB", ATTR_BLUE },
    { 13, 0, "\xDB", ATTR_BLUE },
    { 7, 1, "\xDB", ATTR_BLUE },
    { 14, 1, "\xDB", ATTR_BLUE },
    { 7, 2, "\xDB", ATTR_BLUE },
    { 14, 2, "\xDB", ATTR_BLUE },
    { 7, 3, "\xDB", ATTR_BLUE },
    { 8, 3, "\xDB", ATTR_BLUE },
    { 9, 3, "\xDB", ATTR_BLUE },
    { 10, 3, "\xDB", ATTR_BLUE },
    { 11, 3, "\xDB", ATTR_BLUE },
    { 12, 3, "\xDB", ATTR_BLUE },
    { 13, 3, "\xDB", ATTR_BLUE },
    { 7, 4, "\xDB", ATTR_BLUE },
    { 14, 4, "\xDB", ATTR_BLUE },
    { 7, 5, "\xDB", ATTR_BLUE },
    { 14, 5, "\xDB", ATTR_BLUE },
    { 7, 6, "\xDB", ATTR_BLUE },
    { 8, 6, "\xDB", ATTR_BLUE },
    { 9, 6, "\xDB", ATTR_BLUE },
    { 10, 6, "\xDB", ATTR_BLUE },
    { 11, 6, "\xDB", ATTR_BLUE },
    { 12, 6, "\xDB", ATTR_BLUE },
    { 13, 6, "\xDB", ATTR_BLUE },
    { 17, 0, "\xDB", ATTR_BLUE },
    { 18, 0, "\xDB", ATTR_BLUE },
    { 25, 0, "\xDB", ATTR_BLUE },
    { 26, 0, "\xDB", ATTR_BLUE },
    { 17, 1, "\xDB", ATTR_BLUE },
    { 18, 1, "\xDB", ATTR_BLUE },
    { 19, 1, "\xDB", ATTR_BLUE },
    { 24, 1, "\xDB", ATTR_BLUE },
    { 25, 1, "\xDB", ATTR_BLUE },
    { 26, 1, "\xDB", ATTR_BLUE },
    { 17, 2, "\xDB", ATTR_BLUE },
    { 19, 2, "\xDB", ATTR_BLUE },
    { 20, 2, "\xDB", ATTR_BLUE },
    { 23, 2, "\xDB", ATTR_BLUE },
    { 24, 2, "\xDB", ATTR_BLUE },
    { 26, 2, "\xDB", ATTR_BLUE },
    { 17, 3, "\xDB", ATTR_BLUE },
    { 20, 3, "\xDB", ATTR_BLUE },
    { 21, 3, "\xDB", ATTR_BLUE },
    { 22, 3, "\xDB", ATTR_BLUE },
    { 23, 3, "\xDB", ATTR_BLUE },
    { 26, 3, "\xDB", ATTR_BLUE },
    { 17, 4, "\xDB", ATTR_BLUE },
    { 21, 4, "\xDB", ATTR_BLUE },
    { 22, 4, "\xDB", ATTR_BLUE },
    { 26, 4, "\xDB", ATTR_BLUE },
    { 17, 5, "\xDB", ATTR_BLUE },
    { 26, 5, "\xDB", ATTR_BLUE },
    { 17, 6, "\xDB", ATTR_BLUE },
    { 26, 6, "\xDB", ATTR_BLUE },
};
static const logo_t logo_pcdos = { pcdos_runs, sizeof(pcdos_runs) / sizeof(pcdos_runs[0]), 16 };


static const logo_t *pick_logo(dos_vendor_t vendor)
{
    switch (vendor) {
        case DOS_VENDOR_MS:      return &logo_msdos;
        case DOS_VENDOR_FREEDOS: return &logo_freedos;
        case DOS_VENDOR_DR:      return &logo_drdos;
        case DOS_VENDOR_IBM:     return &logo_pcdos;
        default:                 return &logo_default;
    }
}

static void print_logo(int col, int row, const logo_t *logo, int no_color)
{
    int i;

    for (i = 0; i < logo->run_count; i++) {
        const logo_run_t *r = &logo->runs[i];
        term_puts(col + r->col, row + r->row, r->text, no_color ? ATTR_NORMAL : r->attr);
    }
}

/* Writes "<label>: <value>" starting at (col, row); if the value doesn't
 * fit in the remaining columns, wraps the rest onto the next row(s),
 * indented to line up under where the value started (a hanging indent)
 * rather than clipping it at the screen edge. Returns how many rows
 * this field ended up using, so the caller can advance past all of
 * them before placing the next field.
 *
 * Screen-only: the plain-text file output (fileout.c) always writes
 * one field per line regardless of length, since there's no fixed
 * width to wrap against in a text file and multi-line output would
 * complicate parsing it back out.
 */
static int print_field(int col, int row, const char *label, const char *value,
                        unsigned char label_attr, unsigned char value_attr)
{
    char text[80];
    int value_col;
    int avail;
    int rows_used = 0;
    const char *v = value;

    sprintf(text, "%s: ", label);
    term_puts(col, row, text, label_attr);

    value_col = col + (int)strlen(text);
    avail = SCREEN_COLS - value_col;
    if (avail < 1)
        avail = 1; /* pathological: label alone already fills the row */

    for (;;) {
        int len = (int)strlen(v);

        if (len <= avail) {
            term_puts(value_col, row + rows_used, v, value_attr);
            rows_used++;
            break;
        }

        {
            char chunk[80];

            memcpy(chunk, v, (size_t)avail);
            chunk[avail] = '\0';
            term_puts(value_col, row + rows_used, chunk, value_attr);
        }

        v += avail;
        rows_used++;
    }

    return rows_used;
}

void render_screen(const field_t *fields, int count, int show_logo, int plain,
                    int no_color, dos_vendor_t vendor)
{
    int i;
    int field_col = show_logo ? INFO_COL : LOGO_COL;
    unsigned char label_attr = (plain || no_color) ? ATTR_NORMAL : ATTR_WHITE;
    int row_cursor = ROW_START;
    int logo_height = 0;

    term_clear(ATTR_NORMAL);

    if (show_logo) {
        const logo_t *logo = pick_logo(vendor);

        print_logo(LOGO_COL, ROW_START, logo, no_color);
        logo_height = logo->height;
    }

    /* Every field lines up at the same column, regardless of row --
     * intentionally not "full width once past the logo": a jagged left
     * edge (narrow column for the first few fields, full width for the
     * rest) read worse than a consistently-aligned column. A value that
     * doesn't fit now wraps onto extra rows (print_field's return value)
     * instead of clipping, so row_cursor has to advance by however many
     * rows each field actually used, not a flat one row per field.
     */
    for (i = 0; i < count; i++)
        row_cursor += print_field(field_col, row_cursor, fields[i].label, fields[i].value,
                                   label_attr, ATTR_NORMAL);

    if (ROW_START + logo_height > row_cursor)
        row_cursor = ROW_START + logo_height;

    term_set_cursor(0, row_cursor);
}
