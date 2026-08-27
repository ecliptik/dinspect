/* video.c - video adapter detection via VESA BIOS Extensions (VBE)
 *
 * INT 10h AX=4F00h (Get SuperVGA Information) fills a caller-provided
 * buffer with a VESA "VESA" signature, BCD-free major/minor version,
 * a far pointer to an OEM string, and total video memory in 64KB
 * blocks. Since this whole project runs in real mode (not DJGPP's
 * protected-mode DPMI environment), the returned far pointer can be
 * dereferenced directly -- no transfer-buffer bounce needed.
 *
 * Writing "VBE2" into the buffer's signature field before the call
 * requests VBE 2.0+ extended info (OEM string etc.); BIOSes that only
 * support VBE 1.x ignore it and still fill the base fields.
 */

#include <stdio.h>
#include <string.h>
#include "video.h"

#define VBE_MK_FP(seg, off) \
    ((char _far *)(((unsigned long)(seg) << 16) | (unsigned)(off)))

typedef struct {
    int probed;
    int ok;
    unsigned version;
    unsigned oem_seg, oem_off;
    unsigned long total_kb;
} vbe_probe_t;

static vbe_probe_t g_vbe;
static unsigned char vbe_buf[512];

static void ensure_probed(void)
{
    unsigned result_ax;

    if (g_vbe.probed)
        return;
    g_vbe.probed = 1;

    memcpy(vbe_buf, "VBE2", 4);

    /* ES:DI -> vbe_buf. Small model keeps vbe_buf in the default DS,
     * so ES=DS + DI=offset(vbe_buf) addresses it correctly.
     */
    _asm {
        push ds
        pop es
        mov di, offset vbe_buf
        mov ax, 4F00h
        int 10h
        mov result_ax, ax
    }

    if (result_ax != 0x004Fu || memcmp(vbe_buf, "VESA", 4) != 0) {
        g_vbe.ok = 0;
        return;
    }

    g_vbe.ok = 1;
    g_vbe.version  = *(unsigned *)(vbe_buf + 4);
    g_vbe.oem_off  = *(unsigned *)(vbe_buf + 6);
    g_vbe.oem_seg  = *(unsigned *)(vbe_buf + 8);
    g_vbe.total_kb = (unsigned long)(*(unsigned *)(vbe_buf + 18)) * 64UL;
}

static void read_far_string(unsigned seg, unsigned off, char *dst, size_t dst_len)
{
    char _far *src = VBE_MK_FP(seg, off);
    size_t i = 0;

    while (i < dst_len - 1 && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void get_video_info(char *buf, size_t buflen)
{
    char oem[48];

    ensure_probed();

    if (!g_vbe.ok) {
        strncpy(buf, "UNKNOWN (no VBE)", buflen - 1);
        buf[buflen - 1] = '\0';
        return;
    }

    read_far_string(g_vbe.oem_seg, g_vbe.oem_off, oem, sizeof(oem));

    sprintf(buf, "%s (VBE %u.%u)", oem, g_vbe.version >> 8, g_vbe.version & 0xFFu);
    buf[buflen - 1] = '\0';
}

void get_video_memory(char *buf, size_t buflen)
{
    ensure_probed();

    if (!g_vbe.ok) {
        strncpy(buf, "UNKNOWN", buflen - 1);
        buf[buflen - 1] = '\0';
        return;
    }

    if (g_vbe.total_kb >= 1024UL)
        sprintf(buf, "%lu MB", g_vbe.total_kb / 1024UL);
    else
        sprintf(buf, "%lu KB", g_vbe.total_kb);

    buf[buflen - 1] = '\0';
}
