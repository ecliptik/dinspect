/* picogus.c - PicoGUS presence and mode detection. See picogus.h for
 * the protocol-source/licensing note.
 */

#include <stdio.h>
#include <string.h>
#include "picogus.h"

/* No 'u'/'U' suffix on any of these: they're textually substituted
 * into _asm blocks below, and Watcom's inline assembler (MASM syntax)
 * doesn't understand C integer-literal suffixes.
 */
#define PG_CONTROL_PORT   0x1D0
#define PG_DATA_PORT_HIGH 0x1D2

#define PG_CMD_MAGIC    0x00
#define PG_CMD_PROTOCOL 0x01
#define PG_CMD_BOOTMODE 0x03
#define PG_CMD_HWTYPE   0xF0

#define PG_MAGIC_REPLY  0xDD

static const char *const mode_names[8] = {
    "INVALID", "GUS", "ADLIB", "MPU", "PSG", "SB", "USB", "NE2000"
};

/* Selects a command register (one write to the control port) and reads
 * its value back from the high data port. Every PicoGUS command in
 * this file follows this same one-write-one-read shape.
 */
static unsigned char pg_read(unsigned char cmd)
{
    unsigned char result;

    _asm {
        mov al, cmd
        mov dx, PG_CONTROL_PORT
        out dx, al
        mov dx, PG_DATA_PORT_HIGH
        in al, dx
        mov result, al
    }

    return result;
}

static int pg_detect(void)
{
    unsigned char reply;

    _asm {
        mov al, 0CCh    ; magic knock
        mov dx, PG_CONTROL_PORT
        out dx, al
        mov al, PG_CMD_MAGIC
        out dx, al
        mov dx, PG_DATA_PORT_HIGH
        in al, dx
        mov reply, al
    }

    return reply == PG_MAGIC_REPLY;
}

void get_picogus_info(char *buf, size_t buflen)
{
    unsigned char mode, hwtype, protocol;
    const char *board_name;

    if (!pg_detect()) {
        strncpy(buf, "not detected", buflen - 1);
        buf[buflen - 1] = '\0';
        return;
    }

    mode     = pg_read(PG_CMD_BOOTMODE);
    hwtype   = pg_read(PG_CMD_HWTYPE);
    protocol = pg_read(PG_CMD_PROTOCOL);

    board_name = (hwtype == 1u) ? "PicoGUS 2" : "Pico-based";

    if (mode > 7u)
        mode = 0u; /* clamp an unrecognized value to INVALID rather
                     than index past mode_names[] */

    sprintf(buf, "%s mode (%s, protocol v%u)",
            mode_names[mode], board_name, (unsigned)protocol);
    buf[buflen - 1] = '\0';
}
