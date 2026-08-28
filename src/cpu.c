/* cpu.c - CPU facts
 *
 * CPU class is detected in stages, each gated on the previous one, so a
 * 386+-only instruction is never executed on hardware that doesn't
 * support it:
 *
 *   1. is_386_or_later() (below) - the classic FLAGS bits 12-15 test.
 *      Uses only 16-bit PUSHF/POPF, valid on every 8086+, so it's
 *      always safe to run first, and this file stays compiled at the
 *      project's default 8086-baseline target.
 *   2. is_486_or_later() (cpu386.c) - the AC-bit (bit 18) toggle test,
 *      via PUSHFD/POPFD. Only reached once (1) has confirmed 386+,
 *      since PUSHFD/POPFD don't exist before the 386 -- and PUSHFD/
 *      POPFD/CPUID/RDTSC need a higher compile-time CPU target than
 *      this file uses, hence they live in cpu386.c instead. See
 *      cpu386.h for why splitting the file (rather than raising this
 *      whole file's target) is the safe way to do that.
 *   3. has_cpuid() (cpu386.c) - the ID-bit (bit 21) toggle test. Only
 *      reached once (2) has confirmed 486+ (CPUID itself first
 *      appeared on later 486s and is not universal even then).
 *
 * Clock speed: RDTSC (cpu386.c) across a BIOS-tick window when the
 * CPUID feature bit says TSC is present (Pentium-class+) -- a real,
 * trustworthy MHz reading. Otherwise, a loop timed against PIT channel
 * 2 (the PC speaker channel, silently gated via port 0x61 bit 0, left
 * exactly as found afterward), converted to an *estimated* MHz figure
 * via a single reasoned cycles-per-iteration constant (see
 * EST_CYCLES_PER_ITERATION below) -- always labeled "~N MHz
 * (estimated)" rather than presented as a precise reading, since that
 * constant is derived from published 80386 instruction timings and
 * the loop's disassembly, not calibrated against real silicon (no
 * pre-Pentium hardware was available during development to calibrate
 * against). An earlier version of this code avoided the MHz unit
 * entirely for exactly this reason; the current approach trades a bit
 * of that caution for actually answering "how fast is this CPU"
 * in the units people expect, while keeping the "estimated" label
 * front and center rather than burying the caveat in a comment only.
 */

#include <stdio.h>
#include <string.h>
#include "cpu.h"
#include "cpu386.h"
#include "sysinfo.h"

/* TEMPORARY diagnostic instrumentation for the real-hardware CPU-speed
 * estimation investigation (vcctrl reported ~605 MHz "estimated" for a
 * real 486DX2-50, which should read ~50) -- logs the raw intermediate
 * values from measure_loop_rate_via_pit() so the actual discrepancy
 * can be pinpointed from a real run instead of guessed at. To be
 * removed once the real cause is found and fixed.
 */
static void debug_log(const char *msg)
{
    FILE *fp = fopen("DFDEBUG.LOG", "a");

    if (fp != NULL) {
        fprintf(fp, "%s\n", msg);
        fclose(fp);
    }
}

typedef struct {
    int probed;
    int has_cpuid;
    int has_tsc;
    char vendor[13];
    unsigned family, model, stepping;
    unsigned long edx_features;
    unsigned long speed_value; /* MHz always -- real (RDTSC) if has_tsc,
                                 * else an estimate; see get_cpu_speed() */
    const char *class_desc;
    int l1_known, l2_known;    /* whether cache size was determinable at all */
    unsigned long l1_kb, l2_kb; /* 0 with *_known set means "no such cache" */
} cpu_probe_t;

static cpu_probe_t g_probe;

static int is_386_or_later(void)
{
    unsigned orig, cleared, attempted_set;

    _asm {
        pushf
        pop ax
        mov orig, ax
    }

    _asm {
        mov ax, orig
        and ax, 0FFFh
        push ax
        popf
        pushf
        pop ax
        mov cleared, ax
    }

    if ((cleared & 0xF000u) == 0xF000u) {
        _asm {
            mov ax, orig
            push ax
            popf
        }
        return 0; /* bits 12-15 stuck at 1: 8086/8088/80186/80188 */
    }

    _asm {
        mov ax, orig
        or ax, 0F000h
        push ax
        popf
        pushf
        pop ax
        mov attempted_set, ax
    }

    _asm {
        mov ax, orig
        push ax
        popf
    }

    if ((attempted_set & 0xF000u) == 0)
        return 0; /* bits 12-15 could not be set: 80286 */

    return 1; /* both cleared and set: 80386 or later */
}

#define PIT_CH2_TARGET_TICKS 20000UL /* ~16.76 ms at 1.193182 MHz */

/* Hard cap on the calibration loop below, matching the bounded-poll
 * discipline every other hardware probe in this project already
 * follows (sound.c's SOUND_POLL_MAX, etc.) -- this one didn't have
 * one, and a real machine hung indefinitely in it (PIT channel 2
 * never advancing, likely from the missing I/O delay io_delay() now
 * adds). Even a genuinely slow 8086 clears the real target in a
 * handful of passes; this exists purely so a non-functional PIT
 * channel can't hang forever regardless of the reason.
 */
#define PIT_LOOP_MAX_PASSES 1000000UL

/* A bare `out 80h, al` (the value doesn't matter -- port 0x80 is the
 * conventional POST-diagnostic scratch port, wired but otherwise
 * unused, kept around across the PC-compatible ecosystem specifically
 * for this purpose) costs one bus cycle and nothing else. Real 8253/
 * 8254 PIT silicon can need a beat between consecutive command/data
 * writes to latch correctly; DOSBox-X's PIT emulation doesn't model
 * that timing hazard, so this only ever showed up on real hardware --
 * a 486DX2-50 can issue back-to-back OUTs faster than real PIT logic
 * keeps up with, in a way no dosfetch/dinspect testing here could see.
 */
static void io_delay(void)
{
    _asm {
        out 80h, al
    }
}

static unsigned pit_read_channel2(void)
{
    unsigned char lo, hi;

    _asm {
        mov al, 80h  ; latch command, channel 2
        out 43h, al
    }
    io_delay();
    _asm {
        in al, 42h
        mov lo, al
    }
    io_delay();
    _asm {
        in al, 42h
        mov hi, al
    }

    return ((unsigned)hi << 8) | (unsigned)lo;
}

/* Returns loop iterations per second (NOT MHz -- see the file header
 * comment for why), or 0 if the measurement window couldn't be timed.
 */
static unsigned long measure_loop_rate_via_pit(void)
{
    unsigned char port61_orig;
    unsigned start_count, count;
    unsigned long elapsed_pit_ticks, elapsed_ms, iterations, pass_count;
    unsigned inner;

    _asm {
        in al, 61h
        mov port61_orig, al
    }

    /* Program channel 2: mode 2 (rate generator), LSB/MSB, binary,
     * reload 0 (== 65536), a free-running down-counter we only read.
     * io_delay() between each write: see its comment above pit_read_
     * channel2() -- real PIT silicon needs a beat between these that
     * fast real hardware can outrun without it.
     */
    _asm {
        mov al, 0B4h
        out 43h, al
    }
    io_delay();
    _asm {
        mov al, 0
        out 42h, al
    }
    io_delay();
    _asm {
        out 42h, al
    }
    io_delay();

    /* Enable the gate (bit 0) so the counter runs; leave every other
     * bit (including the speaker-data bit) exactly as found, so this
     * stays silent and doesn't disturb anything else using port 0x61.
     */
    _asm {
        mov al, port61_orig
        or al, 01h
        out 61h, al
    }

    start_count = pit_read_channel2();
    iterations = 0UL;
    elapsed_pit_ticks = 0UL;

    {
        unsigned long pass;

        for (pass = 0UL; pass < PIT_LOOP_MAX_PASSES; pass++) {
            for (inner = 0; inner < 256U; inner++) {
                _asm nop
            }
            iterations += 256UL;

            count = pit_read_channel2();
            elapsed_pit_ticks = (unsigned long)(start_count - count);
            if (elapsed_pit_ticks >= PIT_CH2_TARGET_TICKS)
                break;
        }

        pass_count = pass;

        if (pass >= PIT_LOOP_MAX_PASSES)
            elapsed_pit_ticks = 0UL; /* never reached the target: PIT
                                       * channel 2 isn't counting --
                                       * report UNKNOWN, not a bogus
                                       * rate, and don't spin forever */
    }

    /* Restore the gate bit exactly as we found it. */
    _asm {
        mov al, port61_orig
        out 61h, al
    }

    {
        char msg[96];
        sprintf(msg, "pit: start=%u count=%u pass=%lu elapsed_ticks=%lu iters=%lu",
                start_count, count, pass_count, elapsed_pit_ticks, iterations);
        debug_log(msg);
    }

    if (elapsed_pit_ticks == 0UL)
        return 0UL;

    /* elapsed_ms = elapsed_pit_ticks / 1193.182 (approx). Divide-then-
     * multiply below (not the other way around) so the result stays
     * meaningful even when iterations is small relative to elapsed_ms
     * -- see the file header comment for why that matters.
     */
    elapsed_ms = elapsed_pit_ticks * 100UL / 119318UL;
    if (elapsed_ms == 0UL)
        elapsed_ms = 1UL;

    {
        char msg[64];
        unsigned long rate = (iterations / elapsed_ms) * 1000UL;
        sprintf(msg, "pit: elapsed_ms=%lu rate=%lu iters/sec", elapsed_ms, rate);
        debug_log(msg);
        return rate;
    }
}

/* Rough cycles-per-iteration estimate for the inner NOP-counting loop
 * above. Disassembling the compiled loop shows each iteration is NOP +
 * INC word ptr [mem] + CMP word ptr [mem],imm16 + JB (taken on all but
 * the last of every 256). Published 80386 timings put NOP at 3 clocks;
 * a memory-operand INC/CMP and a taken conditional jump each cost
 * several more on top of that. ~20 cycles/iteration is a reasoned
 * middle estimate from those figures, not a per-architecture-
 * calibrated one -- see the file header comment.
 */
#define EST_CYCLES_PER_ITERATION 20UL

static unsigned long estimate_mhz_from_loop_rate(unsigned long iterations_per_sec)
{
    return (iterations_per_sec * EST_CYCLES_PER_ITERATION) / 1000000UL;
}

/* Recognized CPUID (family, model) -> friendly name, for the vendors
 * that actually shipped DOS-era hardware. NULL for anything not in the
 * table, in which case callers fall back to the raw family/model/
 * stepping numbers. These are public, documented hardware identifiers
 * (the same kind of fact as a CPUID feature-bit position or a VBE
 * structure offset elsewhere in this project), not copied from any
 * particular vendor's or tool's code.
 */
static const char *cpu_model_name(const char *vendor, unsigned family, unsigned model)
{
    int is_intel = (strncmp(vendor, "GenuineIntel", 12) == 0);
    int is_amd   = (strncmp(vendor, "AuthenticAMD", 12) == 0);

    if (is_intel) {
        if (family == 4) {
            switch (model) {
                case 0: case 1: return "486DX";
                case 2:         return "486SX";
                case 3:         return "486DX2";
                case 4:         return "486SL";
                case 5:         return "486SX2";
                case 7:         return "486DX2 (WB)";
                case 8:         return "486DX4";
                case 9:         return "486DX4 (WB)";
                default:        return NULL;
            }
        }
        if (family == 5) {
            switch (model) {
                case 0: case 1: case 2: case 7: return "Pentium";
                case 3:                         return "Pentium OverDrive";
                case 4: case 8:                 return "Pentium MMX";
                default:                        return NULL;
            }
        }
        if (family == 6) {
            switch (model) {
                case 1:            return "Pentium Pro";
                case 3: case 5:    return "Pentium II";
                case 6:            return "Celeron";
                case 7: case 8:
                case 11:           return "Pentium III";
                case 10:           return "Pentium III Xeon";
                default:           return NULL;
            }
        }
    } else if (is_amd) {
        if (family == 4) return "Am486/5x86";
        if (family == 5) {
            switch (model) {
                case 0: case 1: case 2: case 3: return "K5";
                case 6: case 7:                 return "K6";
                case 8:                         return "K6-2";
                case 9:                         return "K6-III";
                default:                        return NULL;
            }
        }
        if (family == 6) return "Athlon/Duron";
    }

    return NULL;
}

static const char *short_vendor(const char *vendor)
{
    if (strncmp(vendor, "GenuineIntel", 12) == 0) return "Intel";
    if (strncmp(vendor, "AuthenticAMD", 12) == 0) return "AMD";
    return vendor;
}

/* AMD publishes L1/L2 cache sizes directly as bitfields in extended
 * CPUID leaves (K5/K6/Athlon-era, and later) -- no descriptor table
 * needed. Leaf 0x80000000 first reports the highest extended leaf this
 * CPU supports; querying a leaf beyond that returns undefined data on
 * some CPUs, so each leaf is gated on that count before use.
 */
static void detect_cache_amd(void)
{
    unsigned long a, b, c, d, max_ext;

    cpuid_call(0x80000000UL, &a, &b, &c, &d);
    max_ext = a;

    if (max_ext >= 0x80000005UL) {
        /* ECX bits 31-24: L1 data cache size in KB. */
        cpuid_call(0x80000005UL, &a, &b, &c, &d);
        g_probe.l1_kb = (c >> 24) & 0xFFUL;
        g_probe.l1_known = 1;
    }

    if (max_ext >= 0x80000006UL) {
        /* ECX bits 31-16: L2 cache size in KB. */
        cpuid_call(0x80000006UL, &a, &b, &c, &d);
        g_probe.l2_kb = (c >> 16) & 0xFFFFUL;
        g_probe.l2_known = 1;
    }
}

typedef struct {
    unsigned char code;
    int is_l1;   /* 1 = L1 (instruction or data), 0 = L2/L3 unified */
    unsigned kb; /* 0 alongside is_l1==0 means "no L2/L3 cache" (0x40) */
} cache_desc_t;

/* Intel CPUID leaf 2 reports cache/TLB info as one-byte "descriptors"
 * scattered across EAX/EBX/ECX/EDX, each looked up in a vendor-defined
 * table. This covers the common 486/Pentium/Pentium Pro/Pentium
 * II-era descriptors -- the hardware this project actually targets --
 * not Intel's full modern table (which has grown to include decades
 * of later descriptors irrelevant here); an unrecognized byte is
 * silently skipped rather than guessed at.
 */
static const cache_desc_t INTEL_CACHE_DESCRIPTORS[] = {
    { 0x06, 1,    8 }, { 0x08, 1,   16 }, { 0x0A, 1,    8 }, { 0x0C, 1,   16 },
    { 0x0D, 1,   16 }, { 0x60, 1,   16 }, { 0x66, 1,    8 }, { 0x67, 1,   16 },
    { 0x68, 1,   32 },
    { 0x40, 0,    0 }, { 0x41, 0,  128 }, { 0x42, 0,  256 }, { 0x43, 0,  512 },
    { 0x44, 0, 1024 }, { 0x45, 0, 2048 }, { 0x78, 0, 1024 }, { 0x79, 0,  128 },
    { 0x7A, 0,  256 }, { 0x7B, 0,  512 }, { 0x7C, 0, 1024 }, { 0x7D, 0, 2048 },
    { 0x7F, 0,  512 }, { 0x82, 0,  256 }, { 0x83, 0,  512 }, { 0x84, 0, 1024 },
    { 0x85, 0, 2048 }, { 0x86, 0,  512 }, { 0x87, 0, 1024 },
};
#define INTEL_CACHE_DESCRIPTOR_COUNT \
    (sizeof(INTEL_CACHE_DESCRIPTORS) / sizeof(INTEL_CACHE_DESCRIPTORS[0]))

static void accumulate_intel_descriptor(unsigned char code)
{
    size_t i;

    if (code == 0x00 || code == 0xFFU)
        return; /* padding, or "see leaf 4 instead" marker */

    for (i = 0; i < INTEL_CACHE_DESCRIPTOR_COUNT; i++) {
        if (INTEL_CACHE_DESCRIPTORS[i].code != code)
            continue;
        if (INTEL_CACHE_DESCRIPTORS[i].is_l1) {
            g_probe.l1_kb += INTEL_CACHE_DESCRIPTORS[i].kb;
            g_probe.l1_known = 1;
        } else {
            g_probe.l2_kb += INTEL_CACHE_DESCRIPTORS[i].kb;
            g_probe.l2_known = 1;
        }
        return;
    }
}

static void detect_cache_intel(void)
{
    unsigned long a, b, c, d;
    unsigned char bytes[16];
    int i;

    cpuid_call(2UL, &a, &b, &c, &d);

    memcpy(bytes,      &a, 4);
    memcpy(bytes + 4,  &b, 4);
    memcpy(bytes + 8,  &c, 4);
    memcpy(bytes + 12, &d, 4);

    for (i = 0; i < 16; i++) {
        /* Byte 0 of EAX is an iteration count (always 1 on the CPUs
         * this table covers), not a descriptor -- skip it. A register
         * with its top bit set holds no valid descriptor bytes at all.
         */
        if (i == 0)
            continue;
        if ((i < 4  && (a & 0x80000000UL)) ||
            (i >= 4 && i < 8  && (b & 0x80000000UL)) ||
            (i >= 8 && i < 12 && (c & 0x80000000UL)) ||
            (i >= 12          && (d & 0x80000000UL)))
            continue;
        accumulate_intel_descriptor(bytes[i]);
    }
}

static void ensure_probed(void)
{
    if (g_probe.probed)
        return;
    g_probe.probed = 1;

    if (!is_386_or_later()) {
        g_probe.has_cpuid = 0;
        g_probe.has_tsc = 0;
        g_probe.class_desc = "8086/80286-class (pre-386)";
    } else {
        if (!is_486_or_later()) {
            g_probe.has_cpuid = 0;
            g_probe.has_tsc = 0;
            g_probe.class_desc = "80386";
        } else {
            if (!has_cpuid()) {
                g_probe.has_cpuid = 0;
                g_probe.has_tsc = 0;
                g_probe.class_desc = "80486 (no CPUID)";
            } else {
                unsigned long a, b, c, d;

                g_probe.has_cpuid = 1;

                cpuid_call(0UL, &a, &b, &c, &d);
                memcpy(g_probe.vendor, &b, 4);
                memcpy(g_probe.vendor + 4, &d, 4);
                memcpy(g_probe.vendor + 8, &c, 4);
                g_probe.vendor[12] = '\0';

                cpuid_call(1UL, &a, &b, &c, &d);
                g_probe.stepping = (unsigned)(a & 0xFUL);
                g_probe.model    = (unsigned)((a >> 4) & 0xFUL);
                g_probe.family   = (unsigned)((a >> 8) & 0xFUL);
                g_probe.edx_features = d;
                g_probe.has_tsc = (d & 0x10UL) != 0UL;

                if (strncmp(g_probe.vendor, "AuthenticAMD", 12) == 0)
                    detect_cache_amd();
                else if (strncmp(g_probe.vendor, "GenuineIntel", 12) == 0)
                    detect_cache_intel();
            }
        }
    }

    if (g_probe.has_tsc)
        g_probe.speed_value = measure_mhz_via_tsc();
    else
        g_probe.speed_value = estimate_mhz_from_loop_rate(measure_loop_rate_via_pit());
}

void get_fpu_status(char *buf, size_t buflen)
{
    unsigned equip;

    _asm {
        int 11h
        mov equip, ax
    }

    strncpy(buf, ((equip & 0x02) == 0x02) ? "YES" : "no", buflen - 1);
    buf[buflen - 1] = '\0';
}

static void format_cache_size(char *buf, size_t buflen, int known, unsigned long kb)
{
    if (!known)
        strncpy(buf, "UNKNOWN", buflen - 1);
    else if (kb == 0UL)
        strncpy(buf, "none", buflen - 1);
    else
        sprintf(buf, "%lu KB", kb);

    buf[buflen - 1] = '\0';
}

void get_cpu_l1_cache(char *buf, size_t buflen)
{
    ensure_probed();
    format_cache_size(buf, buflen, g_probe.l1_known, g_probe.l1_kb);
}

void get_cpu_l2_cache(char *buf, size_t buflen)
{
    ensure_probed();
    format_cache_size(buf, buflen, g_probe.l2_known, g_probe.l2_kb);
}

void get_cpu_info(char *buf, size_t buflen)
{
    ensure_probed();

    if (g_probe.has_cpuid) {
        const char *model_name = cpu_model_name(g_probe.vendor, g_probe.family, g_probe.model);
        const char *vname = short_vendor(g_probe.vendor);

        if (model_name != NULL)
            sprintf(buf, "%s %s (f%u/m%u/s%u)", vname, model_name,
                    g_probe.family, g_probe.model, g_probe.stepping);
        else
            sprintf(buf, "%s f%u/m%u/s%u", vname,
                    g_probe.family, g_probe.model, g_probe.stepping);
    } else {
        strncpy(buf, g_probe.class_desc, buflen - 1);
    }

    buf[buflen - 1] = '\0';
}

void get_cpu_features(char *buf, size_t buflen)
{
    ensure_probed();

    if (!g_probe.has_cpuid) {
        strncpy(buf, "UNKNOWN (no CPUID)", buflen - 1);
        buf[buflen - 1] = '\0';
        return;
    }

    buf[0] = '\0';
    if (g_probe.edx_features & 0x00000001UL) strcat(buf, "FPU ");
    if (g_probe.edx_features & 0x00000010UL) strcat(buf, "TSC ");
    if (g_probe.edx_features & 0x00000020UL) strcat(buf, "MSR ");
    if (g_probe.edx_features & 0x00000100UL) strcat(buf, "CX8 ");
    if (g_probe.edx_features & 0x00002000UL) strcat(buf, "PGE ");
    if (g_probe.edx_features & 0x00800000UL) strcat(buf, "MMX ");

    if (buf[0] == '\0') {
        strcpy(buf, "(none)");
    } else {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == ' ')
            buf[len - 1] = '\0';
    }

    buf[buflen - 1] = '\0';
}

void get_cpu_speed(char *buf, size_t buflen)
{
    ensure_probed();

    if (g_probe.has_tsc && g_probe.speed_value > 0UL)
        sprintf(buf, "%lu MHz", g_probe.speed_value);
    else if (g_probe.speed_value > 0UL)
        sprintf(buf, "~%lu MHz (estimated)", g_probe.speed_value);
    else {
        strncpy(buf, "UNKNOWN", buflen - 1);
        buf[buflen - 1] = '\0';
        return;
    }

    buf[buflen - 1] = '\0';
}
