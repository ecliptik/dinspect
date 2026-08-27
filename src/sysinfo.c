/* sysinfo.c - OS-level facts (DOS version, shell) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sysinfo.h"

void get_dos_version(char *buf, size_t buflen)
{
    unsigned char vendor;
    unsigned char major, minor;
    const char *vendor_name;

    _asm {
        mov ax, 3000h
        int 21h
        mov vendor, bh
    }

    _asm {
        mov ax, 3306h
        int 21h
        mov major, bl
        mov minor, bh
    }

    switch (vendor) {
        case 0x00: vendor_name = "IBM DOS";     break;
        case 0xFD: vendor_name = "FreeDOS";     break;
        case 0xFF: vendor_name = "MS DOS";      break;
        default:   vendor_name = "Unknown DOS"; break;
    }

    sprintf(buf, "%s %u.%u", vendor_name, (unsigned)major, (unsigned)minor);
    buf[buflen - 1] = '\0';
}

void get_shell(char *buf, size_t buflen)
{
    const char *comspec = getenv("COMSPEC");

    if (comspec == NULL)
        comspec = "UNKNOWN";

    strncpy(buf, comspec, buflen - 1);
    buf[buflen - 1] = '\0';
}

unsigned long get_tick_count(void)
{
    unsigned cx_val, dx_val;

    _asm {
        mov ah, 00h
        int 1Ah
        mov cx_val, cx
        mov dx_val, dx
    }

    return ((unsigned long)cx_val << 16) | (unsigned long)dx_val;
}

void format_runtime(char *buf, size_t buflen, unsigned long start_ticks)
{
    unsigned long end_ticks = get_tick_count();
    unsigned long elapsed_ticks = (end_ticks >= start_ticks) ? (end_ticks - start_ticks) : 0UL;
    /* 1 BIOS tick = 1000/18.2065 ms; 54925/1000 approximates that ratio
     * closely enough for a rough timing indicator, without floating point.
     */
    unsigned long ms = elapsed_ticks * 54925UL / 1000UL;

    sprintf(buf, "%lu ms", ms);
    buf[buflen - 1] = '\0';
}
