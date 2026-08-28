/* disk.c - floppy count and disk free/total space
 *
 * Free/total space comes from INT 21h AH=36h (Get Disk Free Space); all
 * four returned registers are unsigned quantities, so the running totals
 * are computed in unsigned long to avoid overflow/sign issues on large
 * disks. AX == 0xFFFF signals "this drive doesn't exist" and is checked
 * before doing anything with the other (otherwise garbage) registers.
 */

#include <stdio.h>
#include "disk.h"

/* TEMPORARY diagnostic instrumentation for the real-hardware hang
 * investigation -- see the matching comment in main.c. Duplicated
 * rather than shared via a header since this is transient debugging
 * code, to be removed once the real hang is found and fixed.
 */
static void debug_log(const char *msg)
{
    FILE *fp = fopen("DFDEBUG.LOG", "a");

    if (fp != NULL) {
        fprintf(fp, "%s\n", msg);
        fclose(fp);
    }
}

unsigned get_floppy_drive_count(void)
{
    unsigned equip;

    _asm {
        int 11h
        mov equip, ax
    }

    if ((equip & 0x01) == 0x01)
        return ((equip >> 6) & 0x03) + 1;
    return 0;
}

void get_floppy_count(char *buf, size_t buflen)
{
    sprintf(buf, "%u", get_floppy_drive_count());
    buf[buflen - 1] = '\0';
}

int get_disk_usage(char *buf, size_t buflen, unsigned char disk)
{
    unsigned spc, free_clusters, bps, total_clusters;
    unsigned long free_kb, total_kb, used_kb;
    unsigned pct;

    _asm {
        mov ah, 36h
        mov dl, disk
        int 21h
        mov spc, ax
        mov free_clusters, bx
        mov bps, cx
        mov total_clusters, dx
    }

    if (spc == 0xFFFFu)
        return 0;

    free_kb  = ((unsigned long)spc * (unsigned long)bps * (unsigned long)free_clusters) / 1024UL;
    total_kb = ((unsigned long)spc * (unsigned long)bps * (unsigned long)total_clusters) / 1024UL;
    used_kb  = total_kb - free_kb;

    if (total_kb == 0UL)
        pct = 0;
    else
        pct = (unsigned)(((free_kb * 100UL) + (total_kb / 2UL)) / total_kb);

    sprintf(buf, "%lu/%lu KB (%u%% free)", used_kb, total_kb, pct);
    buf[buflen - 1] = '\0';
    return 1;
}

/* One label slot per possible non-floppy drive letter (C: through Z:).
 * Static, not stack-resident (matches fields[] in main.c) and must
 * outlive this call, since field_t.label just points at these bytes.
 */
static char disk_labels[24][8]; /* "Disk Z" + NUL = 7 bytes, rounded up */

void add_disk_fields(field_t *fields, int *count, int max_fields)
{
    unsigned char drive_num;
    int label_idx = 0;

    /* Hard-disk-class drive letters always start at C:, full stop --
     * DOS reserves BOTH A: and B: as floppy-class letters regardless of
     * how many physical floppy drives actually exist (0, 1, or 2). A
     * single-floppy-drive machine still has a drive B: (DOS's classic
     * phantom/swap drive using the same physical drive), and probing it
     * hits the same "Abort, Retry, Fail?" hang this function exists to
     * avoid. This previously started from get_floppy_drive_count()+1,
     * which is wrong whenever there's exactly one physical floppy drive
     * -- confirmed the hard way on real hardware.
     */
    for (drive_num = 3;
         drive_num <= 26 && *count < max_fields && label_idx < 24;
         drive_num++, label_idx++) {
        char letter = (char)('A' + drive_num - 1);

        /* No trailing ':' here -- the renderer already appends its own
         * "<label>: <value>" separator, and a drive-letter colon on top
         * of that produced a doubled "Disk C:: ..." (and violates the
         * no-embedded-colons-in-labels convention).
         */
        sprintf(disk_labels[label_idx], "Disk %c", letter);
        fields[*count].label = disk_labels[label_idx];

        {
            char msg[32];
            sprintf(msg, "probing drive %c:", letter);
            debug_log(msg);
        }

        if (!get_disk_usage(fields[*count].value, sizeof(fields[*count].value), drive_num))
            break; /* first missing drive number = end of configured drives */

        (*count)++;
    }
}
