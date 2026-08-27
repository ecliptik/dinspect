/* cpu386.c - 386+/CPUID/RDTSC-dependent helpers.
 *
 * Compiled at a higher CPU target than the rest of dosfetch -- see
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

unsigned long measure_mhz_via_tsc(void)
{
    unsigned long start_tick, tick;
    unsigned long tsc_start, tsc_end, eax_val;
    unsigned long elapsed_ticks, elapsed_ms;

    start_tick = get_tick_count();
    do {
        tick = get_tick_count();
    } while (tick == start_tick); /* align to a tick boundary */

    _asm {
        rdtsc
        mov eax_val, eax
    }
    tsc_start = eax_val;

    start_tick = tick;
    do {
        tick = get_tick_count();
    } while (tick - start_tick < 4UL);

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
