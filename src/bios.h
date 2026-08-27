/* bios.h - shared low-level BIOS/CMOS helpers */

#ifndef DOSFETCH_BIOS_H
#define DOSFETCH_BIOS_H

/* Read a byte from a CMOS/RTC register (port 0x70/0x71). */
unsigned char cmos_read(unsigned char reg);

#endif
