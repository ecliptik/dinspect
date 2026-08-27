/* memory.c - base and extended memory detection
 *
 * Extended memory is queried via INT 15h AX=E801h (CX/DX pair, which BIOSes
 * fill in more reliably than AX/BX), falling back to the CMOS RTC's
 * extended-memory registers (0x17/0x18) on pre-E801h BIOSes.
 */

#include <stdio.h>
#include <string.h>
#include "bios.h"
#include "memory.h"

unsigned get_base_memory_kb(void)
{
    unsigned kb;

    _asm {
        int 12h
        mov kb, ax
    }

    return kb;
}

void get_base_memory(char *buf, size_t buflen)
{
    sprintf(buf, "%u KB", get_base_memory_kb());
    buf[buflen - 1] = '\0';
}

void get_extended_memory(char *buf, size_t buflen)
{
    unsigned char err;
    unsigned cx_val, dx_val;
    unsigned long total_kb;

    err = 0;

    _asm {
        mov ah, 88h
        int 15h
        jnc ext_have_88
        mov err, ah
    ext_have_88:
    }

    if (err != 0) {
        /* INT 15h AH=88h itself failed (pre-286 BIOS, typically) -- this
         * means the amount is undetermined, not that it's zero.
         */
        strncpy(buf, "UNKNOWN", buflen - 1);
        buf[buflen - 1] = '\0';
        return;
    }

    err = 0;
    cx_val = 0;
    dx_val = 0;

    _asm {
        clc
        mov ax, 0E801h
        int 15h
        jnc ext_have_e801
        mov err, ah
    ext_have_e801:
        mov cx_val, cx
        mov dx_val, dx
    }

    if (err != 0) {
        total_kb = (unsigned long)cmos_read(0x17) +
                   256UL * (unsigned long)cmos_read(0x18);
    } else {
        total_kb = (unsigned long)cx_val + 64UL * (unsigned long)dx_val;
    }

    sprintf(buf, "%lu KB", total_kb);
    buf[buflen - 1] = '\0';
}
