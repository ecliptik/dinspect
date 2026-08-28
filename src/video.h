/* video.h - video adapter detection via VESA BIOS Extensions (VBE) */

#ifndef DOSFETCH_VIDEO_H
#define DOSFETCH_VIDEO_H

#include <stddef.h>

/* Formats "<OEM string> (VBE <major>.<minor>)" into buf via INT 10h
 * AX=4F00h, or "UNKNOWN (no VBE)" if the BIOS doesn't support it.
 */
void get_video_info(char *buf, size_t buflen);

/* Formats total video memory ("<n> KB" or "<n> MB") into buf, or
 * "UNKNOWN" if VBE info wasn't available.
 */
void get_video_memory(char *buf, size_t buflen);

/* Formats the video chipset make/model into buf, identified via a PCI
 * configuration-space scan (not the VBE OEM string above, which a
 * VESA TSR can and often does answer generically -- see video.c):
 *   "<Vendor> <Chip>"       - both recognized (e.g. "S3 ViRGE").
 *   "<Vendor> (device NNNN)" - vendor recognized, exact chip isn't.
 *   "PCI NNNN:NNNN"         - neither recognized; raw vendor:device ID.
 *   "UNKNOWN (no PCI bus)"  - no PCI BIOS present (pre-PCI hardware,
 *                             or a PCI-less bus like ISA/VLB for the
 *                             video card specifically).
 *   "UNKNOWN (no PCI video device)" - PCI present but no display-class
 *                             device found on it.
 */
void get_video_chipset(char *buf, size_t buflen);

#endif
