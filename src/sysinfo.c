/* sysinfo.c - OS-level facts (DOS version, shell) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sysinfo.h"

static unsigned char read_dos_vendor_byte(void)
{
    unsigned char vendor;

    _asm {
        mov ax, 3000h
        int 21h
        mov vendor, bh
    }

    return vendor;
}

dos_vendor_t get_dos_vendor(void)
{
    switch (read_dos_vendor_byte()) {
        case 0x00: return DOS_VENDOR_IBM;
        case 0xFD: return DOS_VENDOR_FREEDOS;
        case 0xFF: return DOS_VENDOR_MS;
        default:   return DOS_VENDOR_UNKNOWN;
    }
}

void get_dos_version(char *buf, size_t buflen)
{
    unsigned char major, minor;
    const char *vendor_name;

    switch (get_dos_vendor()) {
        case DOS_VENDOR_IBM:     vendor_name = "IBM DOS";     break;
        case DOS_VENDOR_FREEDOS: vendor_name = "FreeDOS";     break;
        case DOS_VENDOR_MS:      vendor_name = "MS DOS";      break;
        default:                 vendor_name = "Unknown DOS"; break;
    }

    _asm {
        mov ax, 3306h
        int 21h
        mov major, bl
        mov minor, bh
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
