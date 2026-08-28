/* sound.c - sound card detection
 *
 * OPL detection: the classic AdLib/OPL2/3 timer status test (well
 * documented, decades-old public technique -- reset both timers, start
 * timer 1 with a short count, and check that its "expired" status bit
 * set after a brief delay). The delay is produced by repeatedly reading
 * the OPL status port: each read takes a small but real, non-zero
 * amount of time on the ISA bus, which is the standard portable way
 * this test has always been done (no BIOS timer call needed, so it
 * works uniformly on any hardware this project targets).
 *
 * SB16 DSP query and MPU-401 UART probe: both poll a status port
 * waiting for a ready bit, bounded by SOUND_POLL_MAX iterations rather
 * than looping forever -- a real device answers within a handful of
 * polls, so this bound only ever matters when nothing is there to
 * answer, in which case it reports a clean "not detected"/"TIMEOUT"
 * instead of hanging.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sound.h"

#define SOUND_POLL_MAX 60000UL

/* Parses a single-letter, decimal- or hex-valued field out of the
 * BLASTER env var, e.g. field='A' on "A220 I5 D1 H5 T6" -> 0x220
 * (base), field='T' -> 6 (decimal). Returns 1 and sets *val on
 * success, 0 if BLASTER isn't set or has no such field. Shared by the
 * base-port and card-type lookups below rather than duplicated.
 */
static int blaster_field(char field, int is_hex, unsigned *val)
{
    const char *blaster = getenv("BLASTER");
    const char *p;

    if (blaster == NULL)
        return 0;

    for (p = blaster; *p != '\0'; p++) {
        if ((*p == field || *p == (field | 0x20)) && (p == blaster || p[-1] == ' ')) {
            unsigned long parsed = strtoul(p + 1, NULL, is_hex ? 16 : 10);
            if (parsed > 0UL && (!is_hex || parsed < 0x400UL)) {
                *val = (unsigned)parsed;
                return 1;
            }
        }
    }
    return 0;
}

static int blaster_base_port(unsigned *port)
{
    return blaster_field('A', 1, port);
}

/* Card model for the BLASTER env var's 'T' (type) field -- a
 * long-documented Creative Labs convention (every DOS-era sound
 * driver/game that reads BLASTER agrees on these), not something this
 * project invented. T5 and T7-T9 were never assigned a model by
 * Creative; T10 (MCA original SoundBlaster) is rare enough in
 * practice that it's grouped with "unrecognized" here rather than
 * given its own table row.
 */
static const char *blaster_type_name(unsigned type)
{
    switch (type) {
        case 1: return "Sound Blaster 1.x";
        case 2: return "Sound Blaster Pro";
        case 3: return "Sound Blaster 2.0";
        case 4: return "Sound Blaster Pro 2.0";
        case 6: return "Sound Blaster 16/AWE32/AWE64";
        default: return NULL;
    }
}

void get_blaster_env(char *buf, size_t buflen)
{
    const char *blaster = getenv("BLASTER");
    unsigned type;
    const char *type_name;

    if (blaster == NULL) {
        strncpy(buf, "not set", buflen - 1);
        buf[buflen - 1] = '\0';
        return;
    }

    if (blaster_field('T', 0, &type) && (type_name = blaster_type_name(type)) != NULL)
        sprintf(buf, "%s (%s)", blaster, type_name);
    else
        strncpy(buf, blaster, buflen - 1);

    buf[buflen - 1] = '\0';
}

#define OPL_PORT 0x388

static void opl_delay(void)
{
    unsigned i;
    unsigned char dummy;

    for (i = 0; i < 100U; i++) {
        _asm {
            mov dx, OPL_PORT
            in al, dx
            mov dummy, al
        }
    }
}

void get_opl_status(char *buf, size_t buflen)
{
    unsigned char status1, status2;

    /* Reset both timers, clear IRQ, capture status1 (should be 0x00). */
    _asm {
        mov dx, OPL_PORT
        mov al, 04h
        out dx, al
        inc dx
        mov al, 60h
        out dx, al
        dec dx
        mov al, 04h
        out dx, al
        inc dx
        mov al, 80h
        out dx, al
        dec dx
        in al, dx
        mov status1, al
    }

    /* Load timer 1 with a short count and start it. */
    _asm {
        mov dx, OPL_PORT
        mov al, 02h
        out dx, al
        inc dx
        mov al, 0FFh
        out dx, al
        dec dx
        mov al, 04h
        out dx, al
        inc dx
        mov al, 21h
        out dx, al
    }

    opl_delay();

    /* Timer 1 should have expired by now: capture status2 (0xC0). */
    _asm {
        mov dx, OPL_PORT
        in al, dx
        mov status2, al
    }

    /* Mask timers and clear IRQ again, leaving the chip quiescent. */
    _asm {
        mov dx, OPL_PORT
        mov al, 04h
        out dx, al
        inc dx
        mov al, 60h
        out dx, al
        dec dx
        mov al, 04h
        out dx, al
        inc dx
        mov al, 80h
        out dx, al
    }

    if ((status1 & 0xE0u) == 0x00u && (status2 & 0xE0u) == 0xC0u)
        strncpy(buf, "OPL2/3 detected", buflen - 1);
    else
        strncpy(buf, "not detected", buflen - 1);
    buf[buflen - 1] = '\0';
}

static int poll_bit_bounded(unsigned port, unsigned char mask, unsigned char want)
{
    unsigned long i;
    unsigned char status;

    for (i = 0; i < SOUND_POLL_MAX; i++) {
        _asm {
            mov dx, port
            in al, dx
            mov status, al
        }
        if ((status & mask) == want)
            return 1;
    }
    return 0;
}

static unsigned char sb_read_data(unsigned base)
{
    unsigned port = base + 0x0Au;
    unsigned char val;

    _asm {
        mov dx, port
        in al, dx
        mov val, al
    }
    return val;
}

static void sb_write(unsigned port, unsigned char val)
{
    _asm {
        mov dx, port
        mov al, val
        out dx, al
    }
}

static int sb_reset(unsigned base)
{
    unsigned reset_port = base + 0x06u;
    unsigned i;
    unsigned char dummy;

    sb_write(reset_port, 1);

    /* ~3us minimum reset pulse; same port-read delay technique as OPL. */
    for (i = 0; i < 4U; i++) {
        unsigned p = base + 0x0Eu;
        _asm {
            mov dx, p
            in al, dx
            mov dummy, al
        }
    }

    sb_write(reset_port, 0);

    if (!poll_bit_bounded(base + 0x0Eu, 0x80u, 0x80u))
        return 0;

    return sb_read_data(base) == 0xAAu;
}

void get_sb_dsp_version(char *buf, size_t buflen)
{
    unsigned base;
    unsigned char major, minor;

    if (!blaster_base_port(&base))
        goto not_set;

    if (!sb_reset(base))
        goto timeout;
    if (!poll_bit_bounded(base + 0x0Cu, 0x80u, 0x00u)) /* write buffer ready */
        goto timeout;
    sb_write(base + 0x0Cu, 0xE1u); /* DSP command: get version */
    if (!poll_bit_bounded(base + 0x0Eu, 0x80u, 0x80u)) /* data available */
        goto timeout;
    major = sb_read_data(base);
    if (!poll_bit_bounded(base + 0x0Eu, 0x80u, 0x80u))
        goto timeout;
    minor = sb_read_data(base);

    sprintf(buf, "DSP v%u.%u", (unsigned)major, (unsigned)minor);
    buf[buflen - 1] = '\0';
    return;

not_set:
    strncpy(buf, "not probed (BLASTER not set)", buflen - 1);
    buf[buflen - 1] = '\0';
    return;

timeout:
    strncpy(buf, "no response (TIMEOUT)", buflen - 1);
    buf[buflen - 1] = '\0';
}

/* No 'u' suffix: these are textually substituted into _asm blocks
 * below, and Watcom's inline assembler doesn't understand C
 * integer-literal suffixes.
 */
#define MPU_DATA_PORT   0x330
#define MPU_STATUS_PORT 0x331

void get_mpu401_status(char *buf, size_t buflen)
{
    unsigned char val;

    if (!poll_bit_bounded(MPU_STATUS_PORT, 0x40u, 0x00u)) /* ready to write */
        goto not_detected;

    _asm {
        mov dx, MPU_STATUS_PORT
        mov al, 0FFh
        out dx, al
    }

    if (!poll_bit_bounded(MPU_STATUS_PORT, 0x80u, 0x00u)) /* data available */
        goto not_detected;

    _asm {
        mov dx, MPU_DATA_PORT
        in al, dx
        mov val, al
    }

    if (val == 0xFEu) {
        strncpy(buf, "detected", buflen - 1);
        buf[buflen - 1] = '\0';
        return;
    }

not_detected:
    strncpy(buf, "not detected", buflen - 1);
    buf[buflen - 1] = '\0';
}
