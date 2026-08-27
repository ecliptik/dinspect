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

#endif
