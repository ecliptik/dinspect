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
#define ATTR_YELLOW     14
#define ATTR_LIGHTBLUE   9
#define ATTR_LIGHTRED   12
#define ATTR_WHITE      15
#define ATTR_NORMAL      7

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

/* Copies up to n bytes from src[start..] into dst, space-padding any
 * shortfall if src is shorter than start+n, and NUL-terminating dst.
 * (The logo's bottom line is intentionally shorter than the others, so
 * this avoids reading past the end of that string literal.)
 */
static void copy_band(char *dst, const char *src, size_t src_len,
                       size_t start, size_t n)
{
    size_t avail = (start < src_len) ? (src_len - start) : 0;
    size_t take = (avail < n) ? avail : n;

    if (take > 0)
        memcpy(dst, src + start, take);
    memset(dst + take, ' ', n - take);
    dst[n] = '\0';
}

static void print_logo(int col, int row)
{
    static const char *const lines[] = {
        "88888888ba,     ,ad8888ba,    ad88888ba  ",
        "88      `\"8b   d8\"'    `\"8b  d8\"     \"8b ",
        "88        `8b d8'        `8b Y8,         ",
        "88         88 88          88 `Y8aaaaa,   ",
        "88         88 88          88   `\"\"\"\"\"8b, ",
        "88         8P Y8,        ,8P         `8b ",
        "88      .a8P   Y8a.    .a8P  Y8a     a8P ",
        "88888888Y\"'     `\"Y8888Y\"'    \"Y88888P\""
    };
    int i;

    for (i = 0; i < LOGO_ROWS; i++) {
        char band[15];
        size_t len = strlen(lines[i]);

        copy_band(band, lines[i], len, 0, 14);
        term_puts(col, row + i, band, ATTR_YELLOW);

        copy_band(band, lines[i], len, 14, 14);
        term_puts(col + 14, row + i, band, ATTR_LIGHTBLUE);

        copy_band(band, lines[i], len, 28, 14);
        term_puts(col + 28, row + i, band, ATTR_LIGHTRED);
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

void render_screen(const field_t *fields, int count, int show_logo, int plain)
{
    int i;
    int col = show_logo ? INFO_COL : LOGO_COL;
    unsigned char label_attr = plain ? ATTR_NORMAL : ATTR_WHITE;
    int last_row = count;

    term_clear(ATTR_NORMAL);

    if (show_logo) {
        print_logo(LOGO_COL, ROW_START);
        if (LOGO_ROWS > last_row)
            last_row = LOGO_ROWS;
    }

    for (i = 0; i < count; i++)
        print_field(col, ROW_START + i, fields[i].label, fields[i].value,
                    label_attr, ATTR_NORMAL);

    term_set_cursor(0, ROW_START + last_row);
}
