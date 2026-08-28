/* cpu.h - CPU facts: FPU presence (INT 11h), and, where the hardware
 * supports it, CPUID-derived vendor/family/model/stepping, feature
 * flags, and an approximate clock speed.
 *
 * Detection is staged so nothing 386+-only ever executes on genuinely
 * older hardware: a 16-bit-only FLAGS-bit test (safe on any 8086+)
 * gates whether we're 386-or-later before any PUSHFD/POPFD/CPUID
 * instruction is attempted.
 */

#ifndef DOSFETCH_CPU_H
#define DOSFETCH_CPU_H

#include <stddef.h>

/* Formats "YES"/"no" into buf depending on math coprocessor presence. */
void get_fpu_status(char *buf, size_t buflen);

/* Formats "<Vendor> <Model> (f<family>/m<model>/s<stepping>)" if CPUID
 * is available and the (vendor, family, model) is recognized (e.g.
 * "Intel 486DX2 (f4/m3/s2)", "Intel Pentium OverDrive (f5/m3/s1)");
 * falls back to "<Vendor> f<family>/m<model>/s<stepping>" if CPUID
 * works but the specific model isn't in the table, or a coarse
 * pre-CPUID class description ("8086/80286-class (pre-386)", "80386",
 * or "80486 (no CPUID)") if CPUID isn't available at all.
 */
void get_cpu_info(char *buf, size_t buflen);

/* Formats a space-separated list of recognized CPUID leaf-1 EDX feature
 * flags (FPU TSC MSR CX8 PGE MMX) into buf, or "UNKNOWN (no CPUID)" if
 * CPUID is unavailable on this CPU.
 */
void get_cpu_features(char *buf, size_t buflen);

/* Formats a clock speed into buf:
 *   "<n> MHz"  - measured via the time-stamp counter (Pentium-class+
 *               with TSC); a real reading.
 *   "~<n> MHz" - derived from a PIT channel 2 timed loop (used when
 *               there's no TSC) via a cycles-per-iteration constant
 *               calibrated against one real data point so far -- see
 *               the EST_CYCLES_PER_ITERATION comment in cpu.c. The
 *               leading "~" marks it as approximate, not exact.
 *   "UNKNOWN"  - neither measurement produced a usable result.
 */
void get_cpu_speed(char *buf, size_t buflen);

/* Formats L1/L2 cache size into buf:
 *   "<n> KB"  - detected (AMD via extended CPUID leaves 0x80000005/6,
 *               Intel via CPUID leaf 2 descriptor bytes -- see cpu.c).
 *   "none"    - detected and confirmed absent (e.g. no L2 cache).
 *   "UNKNOWN" - CPUID unavailable, a non-Intel/AMD vendor, or no
 *               recognized descriptor for this specific CPU.
 */
void get_cpu_l1_cache(char *buf, size_t buflen);
void get_cpu_l2_cache(char *buf, size_t buflen);

#endif
