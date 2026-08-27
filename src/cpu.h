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

/* Formats "<Vendor> family=N model=N stepping=N" if CPUID is available,
 * else a coarse pre-CPUID class description ("8086/80286-class
 * (pre-386)", "80386", or "80486 (no CPUID)").
 */
void get_cpu_info(char *buf, size_t buflen);

/* Formats a space-separated list of recognized CPUID leaf-1 EDX feature
 * flags (FPU TSC MSR CX8 PGE MMX) into buf, or "UNKNOWN (no CPUID)" if
 * CPUID is unavailable on this CPU.
 */
void get_cpu_features(char *buf, size_t buflen);

/* Formats an estimated clock speed into buf:
 *   "<n> MHz (RDTSC)"          - measured via the time-stamp counter
 *                                (Pentium-class+ with TSC); trustworthy.
 *   "~<n> loop-iter/sec (uncalibrated, no TSC)" - a PIT channel 2 timed
 *                                loop, used when there's no TSC. Not a
 *                                clock speed -- there's no reliable way
 *                                to convert loop throughput to MHz
 *                                without hardware-specific calibration
 *                                this code doesn't have, and earlier
 *                                attempts to force it into a MHz-shaped
 *                                number rounded to 0 (misreported as
 *                                UNKNOWN) on perfectly real hardware.
 *                                Useful only as a relative/comparative
 *                                figure, e.g. across repeated runs on
 *                                the same machine.
 *   "UNKNOWN"                  - neither measurement produced a usable
 *                                result.
 */
void get_cpu_speed(char *buf, size_t buflen);

#endif
