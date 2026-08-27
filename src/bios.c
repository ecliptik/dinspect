/* bios.c - shared low-level BIOS/CMOS helpers */

#include "bios.h"

unsigned char cmos_read(unsigned char reg)
{
    unsigned char value;

    _asm {
        mov al, reg
        out 70h, al
        in  al, 71h
        mov value, al
    }

    return value;
}
