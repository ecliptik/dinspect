/* picogus.h - PicoGUS (RP2040-based ISA sound card) presence and mode
 * detection.
 *
 * Protocol facts used here (port addresses, magic bytes, command/mode
 * numbers) come from PicoGUS's own public protocol definitions
 * (https://github.com/polpo/picogus, GPLv2). Reimplemented from those
 * documented values -- not copied code -- to keep dosfetch CC0, same
 * policy as every other detector in this project.
 *
 * Port 0x1D0 is PicoGUS's own deliberate choice, picked specifically
 * because it's unclaimed in Ralf Brown's Interrupt/Port List, so
 * probing it is safe on any machine, PicoGUS-equipped or not: it's a
 * direct, immediate read/write with no "wait for a device to answer"
 * polling loop, unlike the SB16/MPU-401 probes in sound.c.
 */

#ifndef DOSFETCH_PICOGUS_H
#define DOSFETCH_PICOGUS_H

#include <stddef.h>

/* Formats "not detected", or "<MODE> mode (<board>, protocol vN)" into
 * buf if a PicoGUS answers the presence check.
 */
void get_picogus_info(char *buf, size_t buflen);

#endif
