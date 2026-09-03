/* cpu386.c - 386+/CPUID/RDTSC-dependent helpers.
 *
 * Compiled at a higher CPU target than the rest of dinspect -- see
 * cpu386.h for why that's safe despite the rest of the project
 * deliberately targeting 8086 baseline.
 */

#include "cpu386.h"
#include "sysinfo.h"

int is_486_or_later(void)
{
    unsigned long orig, after;

    _asm {
        pushfd
        pop eax
        mov orig, eax
    }

    _asm {
        mov eax, orig
        xor eax, 40000h
        push eax
        popfd
        pushfd
        pop eax
        mov after, eax
    }

    _asm {
        mov eax, orig
        push eax
        popfd
    }

    return (after != orig) ? 1 : 0; /* AC bit (18) stuck: 80386 */
}

int has_cpuid(void)
{
    unsigned long orig, after;

    _asm {
        pushfd
        pop eax
        mov orig, eax
    }

    _asm {
        mov eax, orig
        xor eax, 200000h
        push eax
        popfd
        pushfd
        pop eax
        mov after, eax
    }

    _asm {
        mov eax, orig
        push eax
        popfd
    }

    return (after != orig) ? 1 : 0; /* ID bit (21) stuck: pre-CPUID 486 */
}

void cpuid_call(unsigned long leaf, unsigned long *a, unsigned long *b,
                 unsigned long *c, unsigned long *d)
{
    unsigned long ra, rb, rc, rd;

    _asm {
        mov eax, leaf
        cpuid
        mov ra, eax
        mov rb, ebx
        mov rc, ecx
        mov rd, edx
    }

    *a = ra;
    *b = rb;
    *c = rc;
    *d = rd;
}

/* Hard cap on each BIOS-tick poll loop below, matching the bounded-poll
 * discipline every other hardware probe in this project follows (see
 * PIT_LOOP_MAX_PASSES in cpu.c). Both loops here spin on get_tick_count()
 * (INT 1Ah), which only ever advances because IRQ0's handler updates the
 * BIOS tick count in the background -- nothing stops that from never
 * happening again after this function is entered (a masked/hung IRQ0 on
 * whatever's driving the PIT, for instance), and unlike the PIT-channel-0
 * read cpu.c uses, get_tick_count() itself can't detect that condition on
 * its own. This path only ever runs once CPUID leaf 1 has already
 * confirmed TSC support (has_tsc), which every chip tested against this
 * project so far predates -- so it's never actually been exercised on
 * real hardware until now, unlike the PIT loop it mirrors. 300,000 polls
 * is far more than the handful of ticks either wait normally needs even
 * on the slowest TSC-capable (Pentium-class) chip, while still bounded.
 */
#define TSC_WAIT_MAX_POLLS 300000UL

unsigned long measure_mhz_via_tsc(void)
{
    unsigned long start_tick, tick;
    unsigned long tsc_start, tsc_end, eax_val;
    unsigned long elapsed_ticks, elapsed_ms;
    unsigned long polls;

    start_tick = get_tick_count();
    polls = 0UL;
    do {
        tick = get_tick_count();
        polls++;
    } while (tick == start_tick && polls < TSC_WAIT_MAX_POLLS); /* align
                                                                   * to a
                                                                   * tick
                                                                   * boundary */
    if (polls >= TSC_WAIT_MAX_POLLS)
        return 0UL; /* BIOS tick count never advanced -- report UNKNOWN,
                      * don't spin forever */

    _asm {
        rdtsc
        mov eax_val, eax
    }
    tsc_start = eax_val;

    start_tick = tick;
    polls = 0UL;
    do {
        tick = get_tick_count();
        polls++;
    } while (tick - start_tick < 4UL && polls < TSC_WAIT_MAX_POLLS);
    if (polls >= TSC_WAIT_MAX_POLLS)
        return 0UL;

    _asm {
        rdtsc
        mov eax_val, eax
    }
    tsc_end = eax_val;

    elapsed_ticks = tick - start_tick;
    elapsed_ms = elapsed_ticks * 54925UL / 1000UL;
    if (elapsed_ms == 0UL)
        return 0UL;

    return (tsc_end - tsc_start) / (elapsed_ms * 1000UL);
}
