/* network.c - network stack detection
 *
 * Packet driver detection: the Packet Driver Specification's own
 * documented convention is that every compliant driver places the
 * ASCII signature "PKT DRVR" starting 3 bytes into its interrupt
 * handler (right after its entry jump instruction), specifically so
 * callers can identify one without calling it. Scanning the reserved
 * 0x60-0x80 vector range and reading those 8 bytes at each candidate
 * is a pure memory read -- it never executes/calls the vector, so
 * there's no risk from probing a vector that turns out to be
 * something else (or nothing).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "network.h"

#define NET_MK_FP(vseg, voff) \
    ((char _far *)(((unsigned long)(vseg) << 16) | (unsigned)(voff)))

static int has_pkt_drvr_signature(unsigned vseg, unsigned voff)
{
    static const char sig[8] = "PKT DRVR";
    char _far *p = NET_MK_FP(vseg, voff + 3);
    int i;

    for (i = 0; i < 8; i++) {
        if (p[i] != sig[i])
            return 0;
    }
    return 1;
}

static int find_packet_driver_vector(void)
{
    unsigned char vec;

    for (vec = 0x60; vec <= 0x80; vec++) {
        /* Not "seg"/"off": those collide with MASM's SEG/OFFSET
         * keywords and break the inline assembler below.
         */
        unsigned vseg, voff;

        _asm {
            mov ah, 35h
            mov al, vec
            int 21h
            mov vseg, es
            mov voff, bx
        }

        if (vseg == 0 && voff == 0)
            continue;

        if (has_pkt_drvr_signature(vseg, voff))
            return (int)vec;
    }
    return -1;
}

void get_packet_driver_info(char *buf, size_t buflen)
{
    int vec = find_packet_driver_vector();

    if (vec < 0)
        strncpy(buf, "not detected", buflen - 1);
    else
        sprintf(buf, "vector 0x%02X", vec);
    buf[buflen - 1] = '\0';
}

/* Copies the whitespace-delimited token starting at src (after
 * skipping leading whitespace) into dst, stopping at whitespace, EOL,
 * or dst_len - 1 bytes.
 */
static void copy_token(const char *src, char *dst, size_t dst_len)
{
    size_t i = 0;

    while (*src == ' ' || *src == '\t')
        src++;

    while (*src != '\0' && *src != ' ' && *src != '\t' &&
           *src != '\r' && *src != '\n' && i < dst_len - 1) {
        dst[i++] = *src++;
    }
    dst[i] = '\0';
}

void get_network_ip_info(char *buf, size_t buflen)
{
    const char *cfg_path = getenv("MTCPCFG");
    FILE *fp;
    char line[128];
    char ipaddr[32];
    char gateway[32];

    if (cfg_path == NULL) {
        strncpy(buf, "not configured (MTCPCFG not set)", buflen - 1);
        buf[buflen - 1] = '\0';
        return;
    }

    fp = fopen(cfg_path, "r");
    if (fp == NULL) {
        strncpy(buf, "UNKNOWN (MTCPCFG set but file not found)", buflen - 1);
        buf[buflen - 1] = '\0';
        return;
    }

    ipaddr[0] = '\0';
    gateway[0] = '\0';

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *p = line;

        while (*p == ' ' || *p == '\t')
            p++;

        if (strnicmp(p, "IPADDR", 6) == 0 && (p[6] == ' ' || p[6] == '\t'))
            copy_token(p + 6, ipaddr, sizeof(ipaddr));
        else if (strnicmp(p, "GATEWAY", 7) == 0 && (p[7] == ' ' || p[7] == '\t'))
            copy_token(p + 7, gateway, sizeof(gateway));
    }

    fclose(fp);

    if (ipaddr[0] == '\0' && gateway[0] == '\0') {
        strncpy(buf, "UNKNOWN (no IPADDR/GATEWAY line -- DHCP config?)", buflen - 1);
        buf[buflen - 1] = '\0';
        return;
    }

    sprintf(buf, "IP %s, GW %s",
            ipaddr[0] ? ipaddr : "UNKNOWN",
            gateway[0] ? gateway : "UNKNOWN");
    buf[buflen - 1] = '\0';
}
