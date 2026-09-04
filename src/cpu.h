/* cpu.h - CPU facts: FPU presence (CPUID leaf 1 EDX bit 0 when CPUID is
 * available, since that reflects the silicon rather than a possibly
 * stale BIOS/CMOS setting -- INT 11h's equipment word only as a
 * fallback on pre-CPUID hardware), and, where the hardware supports
 * it, CPUID-derived vendor/family/model/stepping, feature flags, and
 * an approximate clock speed.
 *
 * Detection is staged so nothing 386+-only ever executes on genuinely
 * older hardware: a 16-bit-only FLAGS-bit test (safe on any 8086+)
 * gates whether we're 386-or-later before any PUSHFD/POPFD/CPUID
 * instruction is attempted.
 */

#ifndef DOSFETCH_CPU_H
#define DOSFETCH_CPU_H

#include <stddef.h>

/* Formats "YES"/"no" into buf depending on math coprocessor presence.
 * Uses CPUID's own FPU feature bit when CPUID is available (the
 * authoritative source -- see cpu.h's file header comment), falling
 * back to the INT 11h BIOS equipment word only on pre-CPUID hardware.
 */
void get_fpu_status(char *buf, size_t buflen);

/* Formats "<Vendor> <Model>" if CPUID is available and the (vendor,
 * family, model) is recognized (e.g. "Intel 486DX2", "Intel Pentium
 * OverDrive") -- matching neofetch/fastfetch's convention of showing
 * a friendly product name rather than raw identifiers whenever one is
 * available. Falls back to "<Vendor> f<family>/m<model>/s<stepping>"
 * (CPUID leaf 1's raw family/model/stepping fields) if CPUID works but
 * the specific model isn't in cpu_model_name()'s table -- there's no
 * friendly name to fall back to in that case, since pre-Pentium-4
 * CPUID has no brand-string leaf the way modern CPUs do. A coarse
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
 *               with TSC, running in true real mode); a real reading.
 *   "~<n> MHz" - derived from a PIT channel 0 timed loop (used when
 *               there's no TSC, or when a V86 monitor such as EMM386 is
 *               loaded -- RDTSC under one has hung real hardware, see
 *               ensure_probed() in cpu.c) via a cycles-per-iteration constant
 *               calibrated against two real data points so far -- see
 *               the EST_CYCLES_PER_ITERATION comment in cpu.c. This
 *               method has also shown ~8% run-to-run variance on
 *               identical hardware, so the raw estimate is snapped to
 *               the nearest bus-speed x multiplier a real 386/486-class
 *               CPU actually shipped at (see snap_to_plausible_mhz() in
 *               cpu.c) before formatting -- e.g. a noisy "~59 MHz"
 *               reading on a true DX2-66 becomes "~66 MHz". The leading
 *               "~" still marks it as derived/approximate, not exact.
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
