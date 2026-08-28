/* dinspect - a neofetch clone for DOS
 *
 * Originally written by Leah Neukirchen <leah@vuxu.org> in Turbo Pascal 7.
 * This C port (Open Watcom, 16-bit real-mode DOS target) reimplements the
 * same fields using the same BIOS/DOS interrupts.
 *
 * To the extent possible under law, the creator(s) of this work have
 * waived all copyright and related or neighboring rights to this work.
 * See LICENSE (CC0 1.0 Universal).
 */

#include <stdio.h>
#include <string.h>
#include "critical_error.h"
#include "cpu.h"
#include "disk.h"
#include "fields.h"
#include "fileout.h"
#include "memory.h"
#include "network.h"
#include "output.h"
#include "picogus.h"
#include "sound.h"
#include "sysinfo.h"
#include "video.h"

#define ADD_FIELD(label_str, getter_call) \
    do { \
        fields[count].label = (label_str); \
        getter_call; \
        count++; \
    } while (0)

static void print_usage(void)
{
    printf("dinspect - a neofetch clone for DOS\n\n");
    printf("Usage: dinspect [options]\n\n");
    printf("  --no-logo          Do not draw the ASCII logo\n");
    printf("  --plain            Monochrome screen output, no logo (for OCR capture)\n");
    printf("  --no-color         Disable color output; keeps the logo and layout\n");
    printf("  -o, --out FILE     Also write a plain-text report to FILE\n");
    printf("  --show-undetected  Include fields that couldn't be detected (UNKNOWN,\n");
    printf("                     not detected, etc.) instead of omitting them\n");
    printf("  -h, --help         Show this help\n");
}

/* Whether a field's value marks it as not actually detected -- these
 * are the exact lead-in strings every detector in this project uses
 * for "couldn't determine this" (see disk.h/sound.h/etc. for the
 * per-field UNKNOWN-vs-real-value convention). Matched as a prefix,
 * not a substring, so a partially-successful value like
 * "IP UNKNOWN, GW 192.168.1.1" (real gateway, unknown address) is
 * correctly left in rather than dropped for merely containing the
 * word UNKNOWN somewhere.
 */
static int is_undetected_value(const char *value)
{
    static const char *const markers[] = {
        "UNKNOWN", "not detected", "not configured", "not probed",
        "no response", "not set"
    };
    size_t i;

    for (i = 0; i < sizeof(markers) / sizeof(markers[0]); i++) {
        size_t len = strlen(markers[i]);
        if (strncmp(value, markers[i], len) == 0)
            return 1;
    }
    return 0;
}

/* Compacts fields[] in place, dropping any whose value is an
 * "undetected" marker (see is_undetected_value()), and returns the new
 * count.
 */
static int filter_undetected(field_t *fields, int count)
{
    int kept = 0;
    int i;

    for (i = 0; i < count; i++) {
        if (!is_undetected_value(fields[i].value)) {
            if (kept != i)
                fields[kept] = fields[i];
            kept++;
        }
    }
    return kept;
}

/* static, not stack-resident: a 16-bit DOS program's default stack is a
 * few KB, and this array alone is ~2KB -- combined with the rest of the
 * call chain that was enough to trip Watcom's stack-overflow check.
 */
static field_t fields[MAX_FIELDS];

int main(int argc, char *argv[])
{
    int count = 0;
    int show_logo = 1;
    int plain = 0;
    int no_color = 0;
    int show_undetected = 0;
    const char *out_path = NULL;
    int i;

    /* Before anything else touches a disk: silently Fail any critical
     * error (empty floppy, empty CD-ROM, etc.) instead of blocking on
     * DOS's "Abort, Retry, Fail?" prompt. See critical_error.h.
     */
    install_silent_critical_handler();

    for (i = 1; i < argc; i++) {
        if (stricmp(argv[i], "--no-logo") == 0) {
            show_logo = 0;
        } else if (stricmp(argv[i], "--plain") == 0) {
            plain = 1;
            show_logo = 0;
        } else if (stricmp(argv[i], "--no-color") == 0) {
            no_color = 1;
        } else if (stricmp(argv[i], "--show-undetected") == 0) {
            show_undetected = 1;
        } else if (stricmp(argv[i], "-o") == 0 || stricmp(argv[i], "--out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "dinspect: %s requires a filename\n", argv[i]);
                return 1;
            }
            out_path = argv[++i];
        } else if (stricmp(argv[i], "-h") == 0 || stricmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        } else {
            fprintf(stderr, "dinspect: unknown option '%s'\n", argv[i]);
            print_usage();
            return 1;
        }
    }

    ADD_FIELD("OS", get_dos_version(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("Shell", get_shell(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("CPU", get_cpu_info(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("CPU Speed", get_cpu_speed(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("CPU Features", get_cpu_features(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("Floating Point Unit", get_fpu_status(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("L1 Cache", get_cpu_l1_cache(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("L2 Cache", get_cpu_l2_cache(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("Base Memory", get_base_memory(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("Ext. Memory", get_extended_memory(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("Video", get_video_info(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("Video Chipset", get_video_chipset(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("Video Memory", get_video_memory(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("Sound BLASTER", get_blaster_env(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("Sound OPL", get_opl_status(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("Sound SB DSP", get_sb_dsp_version(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("Sound MPU-401", get_mpu401_status(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("PicoGUS", get_picogus_info(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("Network Packet Driver", get_packet_driver_info(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("Network IP Config", get_network_ip_info(fields[count].value, sizeof(fields[count].value)));
    ADD_FIELD("Floppy drives", get_floppy_count(fields[count].value, sizeof(fields[count].value)));

    add_disk_fields(fields, &count, MAX_FIELDS);

    if (!show_undetected)
        count = filter_undetected(fields, count);

    render_screen(fields, count, show_logo, plain, no_color, get_dos_vendor());

    if (out_path != NULL) {
        if (write_fields_file(fields, count, out_path) != 0) {
            fprintf(stderr, "dinspect: could not write '%s'\n", out_path);
            return 1;
        }
    }

    return 0;
}
