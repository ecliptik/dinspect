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

/* Video chipset identification via PCI configuration space -- unlike
 * the VBE OEM string above, this can't be answered by a VESA TSR
 * hooking INT 10h: a TSR like UniVBE hands back its own generic
 * "Universal VESA VBE" string for AX=4F00h regardless of what chip is
 * actually installed, so on a box running one, get_video_info() above
 * will never say "S3" even though the hardware plainly is. Reading the
 * chip's PCI vendor/device ID bypasses whatever's answering INT 10h
 * entirely.
 *
 * Uses INT 1Ah AH=B1h (the PCI BIOS service), restricted to the
 * sub-functions whose inputs/outputs fit in 16-bit registers (AX/BX/
 * CX/DX/SI/DI), so this file can stay at the project's 8086-baseline
 * compile target like the VBE code above -- FIND_PCI_CLASS_CODE and
 * READ_CONFIG_DWORD both take/return a 32-bit ECX, which Watcom's
 * inline assembler refuses below a 386 target (see cpu386.h for the
 * same constraint on CPUID/RDTSC). A manual bus/device/function walk
 * using READ_CONFIG_BYTE/WORD gets the same result without ECX. This
 * doesn't need a CPU-generation gate the way cpu386.c's calls do,
 * though: PCI_BIOS_PRESENT below just comes back "not present" on
 * genuinely pre-PCI hardware, so there's no illegal-instruction risk
 * on old CPUs either way.
 */

static int pci_bios_present(unsigned *last_bus)
{
    unsigned status_ax, cx_out, flags;

    _asm {
        mov ax, 0B101h
        int 1Ah
        mov status_ax, ax
        mov cx_out, cx
        pushf
        pop ax
        mov flags, ax
    }

    if ((flags & 1u) != 0 || (status_ax & 0xFF00u) != 0)
        return 0; /* carry set, or AH != 0: no PCI BIOS */

    *last_bus = cx_out & 0xFFu;
    return 1;
}

static unsigned pci_read_config_word(unsigned char bus, unsigned char devfn, unsigned char reg)
{
    unsigned bus_devfn = ((unsigned)bus << 8) | devfn;
    unsigned reg_num = reg;
    unsigned result;

    _asm {
        mov ax, 0B109h
        mov bx, bus_devfn
        mov di, reg_num
        int 1Ah
        mov result, cx
    }

    return result;
}

static unsigned char pci_read_config_byte(unsigned char bus, unsigned char devfn, unsigned char reg)
{
    unsigned bus_devfn = ((unsigned)bus << 8) | devfn;
    unsigned reg_num = reg;
    unsigned result;

    _asm {
        mov ax, 0B108h
        mov bx, bus_devfn
        mov di, reg_num
        int 1Ah
        mov result, cx
    }

    return (unsigned char)(result & 0xFFu);
}

#define PCI_MAX_BUS_SCAN     8  /* cap even if a buggy BIOS reports an
                                  * implausible last-bus number -- DOS-era
                                  * boxes never have more than one or two
                                  * PCI bridges */
#define PCI_DEVICES_PER_BUS  32
#define PCI_FUNCS_PER_DEVICE 8

#define PCI_REG_VENDOR_ID    0x00
#define PCI_REG_DEVICE_ID    0x02
#define PCI_REG_HEADER_TYPE  0x0E
#define PCI_REG_BASE_CLASS   0x0B

#define PCI_CLASS_DISPLAY_CONTROLLER 0x03
#define PCI_VENDOR_ID_ABSENT          0xFFFFu
#define PCI_HEADER_TYPE_MULTIFUNCTION 0x80u

/* Walks PCI config space for the first display-class (base class 03h)
 * device and returns its vendor/device ID. Stops at the first match,
 * which is all a single-GPU DOS box ever has. *pci_ok is set to
 * whether a PCI BIOS was found at all, so callers can tell "no PCI
 * bus" apart from "PCI bus present, nothing display-class on it".
 */
static int find_pci_vga(unsigned *vendor_id, unsigned *device_id, int *pci_ok)
{
    unsigned last_bus, bus, dev, func, max_func;
    unsigned char devfn, header_type, base_class;
    unsigned vendor;

    if (!pci_bios_present(&last_bus)) {
        *pci_ok = 0;
        return 0;
    }
    *pci_ok = 1;

    if (last_bus >= PCI_MAX_BUS_SCAN)
        last_bus = PCI_MAX_BUS_SCAN - 1;

    for (bus = 0; bus <= last_bus; bus++) {
        for (dev = 0; dev < PCI_DEVICES_PER_BUS; dev++) {
            max_func = 0;

            for (func = 0; func <= max_func; func++) {
                devfn = (unsigned char)((dev << 3) | func);
                vendor = pci_read_config_word((unsigned char)bus, devfn, PCI_REG_VENDOR_ID);

                if (vendor == PCI_VENDOR_ID_ABSENT) {
                    if (func == 0)
                        break; /* nothing at this device slot */
                    continue;
                }

                if (func == 0) {
                    header_type = pci_read_config_byte((unsigned char)bus, devfn, PCI_REG_HEADER_TYPE);
                    if (header_type & PCI_HEADER_TYPE_MULTIFUNCTION)
                        max_func = PCI_FUNCS_PER_DEVICE - 1;
                }

                base_class = pci_read_config_byte((unsigned char)bus, devfn, PCI_REG_BASE_CLASS);
                if (base_class == PCI_CLASS_DISPLAY_CONTROLLER) {
                    *vendor_id = vendor;
                    *device_id = pci_read_config_word((unsigned char)bus, devfn, PCI_REG_DEVICE_ID);
                    return 1;
                }
            }
        }
    }

    return 0;
}

typedef struct {
    unsigned vendor_id;
    const char *name;
} pci_vendor_entry_t;

/* PCI vendor IDs are assigned by the PCI SIG and publicly documented --
 * the same category of fact as a CPUID family/model pair (see cpu.c's
 * cpu_model_name()). Limited to vendors that actually shipped DOS-era
 * video hardware.
 */
static const pci_vendor_entry_t pci_vendors[] = {
    { 0x5333, "S3" },
    { 0x1002, "ATI" },
    { 0x102B, "Matrox" },
    { 0x1013, "Cirrus Logic" },
    { 0x1023, "Trident" },
    { 0x100C, "Tseng Labs" },
    { 0x121A, "3dfx" },
    { 0x10DE, "NVIDIA" },
    { 0x105D, "Number Nine" },
};
#define PCI_VENDOR_COUNT (sizeof(pci_vendors) / sizeof(pci_vendors[0]))

static const char *pci_vendor_name(unsigned vendor_id)
{
    size_t i;

    for (i = 0; i < PCI_VENDOR_COUNT; i++) {
        if (pci_vendors[i].vendor_id == vendor_id)
            return pci_vendors[i].name;
    }
    return NULL;
}

typedef struct {
    unsigned vendor_id;
    unsigned device_id;
    const char *chip_name;
} pci_device_entry_t;

/* Specific chip names for the vendor/device ID pairs this project has
 * a documented or real-hardware-confirmed identity for. Anything else
 * falls back to the vendor name alone (pci_vendor_name()) or the raw
 * hex IDs -- same "don't guess, show the closest true thing" approach
 * as cpu_model_name()'s fallback to raw family/model numbers.
 */
static const pci_device_entry_t pci_devices[] = {
    { 0x5333, 0x5631, "ViRGE" },
    { 0x5333, 0x8A01, "ViRGE/DX or /GX" },
    { 0x5333, 0x8A10, "ViRGE/GX2" },
    { 0x5333, 0x8904, "ViRGE/MX+" },
    { 0x5333, 0x883D, "ViRGE/MX" },
    { 0x5333, 0x8810, "Trio32" },
    { 0x5333, 0x8811, "Trio64" },
};
#define PCI_DEVICE_COUNT (sizeof(pci_devices) / sizeof(pci_devices[0]))

static const char *pci_device_name(unsigned vendor_id, unsigned device_id)
{
    size_t i;

    for (i = 0; i < PCI_DEVICE_COUNT; i++) {
        if (pci_devices[i].vendor_id == vendor_id && pci_devices[i].device_id == device_id)
            return pci_devices[i].chip_name;
    }
    return NULL;
}

void get_video_chipset(char *buf, size_t buflen)
{
    unsigned vendor_id, device_id;
    const char *vendor_name, *chip_name;
    int pci_ok;

    if (!find_pci_vga(&vendor_id, &device_id, &pci_ok)) {
        strncpy(buf, pci_ok ? "UNKNOWN (no PCI video device)" : "UNKNOWN (no PCI bus)", buflen - 1);
        buf[buflen - 1] = '\0';
        return;
    }

    vendor_name = pci_vendor_name(vendor_id);
    chip_name = pci_device_name(vendor_id, device_id);

    if (chip_name != NULL)
        sprintf(buf, "%s %s", vendor_name != NULL ? vendor_name : "", chip_name);
    else if (vendor_name != NULL)
        sprintf(buf, "%s (device %04X)", vendor_name, device_id);
    else
        sprintf(buf, "PCI %04X:%04X", vendor_id, device_id);

    buf[buflen - 1] = '\0';
}
