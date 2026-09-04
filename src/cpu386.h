/* cpu386.h - internal: 386+/CPUID/RDTSC-dependent helpers.
 *
 * Everything in cpu386.c is compiled at a higher CPU target than the
 * rest of dinspect (see the Makefile) because Watcom's inline assembler
 * refuses 386+/Pentium mnemonics (PUSHFD, EAX, CPUID, RDTSC) at the
 * project's default 8086-baseline target -- it's not just a codegen
 * hint, it gates what the assembler will accept at all.
 *
 * That's safe: these functions are only ever CALLED (from cpu.c, kept
 * at 8086-baseline) after cpu.c's own always-safe, 16-bit-only checks
 * have already confirmed the CPU actually supports the tier being
 * probed next. The machine code existing in this object file is
 * harmless as long as it's never executed on hardware that can't run
 * it -- which is exactly what that call gating guarantees.
 */

#ifndef DOSFETCH_CPU386_H
#define DOSFETCH_CPU386_H

/* AC-bit (EFLAGS bit 18) toggle test. Only call once is_386_or_later()
 * (cpu.c, safe on any 8086+) has confirmed 386+ -- PUSHFD/POPFD don't
 * exist before the 386.
 */
int is_486_or_later(void);

/* ID-bit (EFLAGS bit 21) toggle test. Only call once is_486_or_later()
 * has confirmed 486+.
 */
int has_cpuid(void);

/* Raw CPUID instruction. Only call once has_cpuid() has confirmed
 * CPUID is actually available (not every 486 that reaches this stage
 * has it).
 */
void cpuid_call(unsigned long leaf, unsigned long *a, unsigned long *b,
                 unsigned long *c, unsigned long *d);

/* Whether this real-mode program is actually running as a virtual-8086
 * task under a protected-mode monitor (EMM386, QEMM, a Windows DOS box,
 * ...) rather than in true real mode. Reads CR0's PE bit via SMSW, which
 * is unprivileged and never traps. Only call once is_386_or_later() has
 * confirmed 386+ (V86 mode doesn't exist before that anyway).
 */
int in_v86_mode(void);

/* RDTSC-based clock speed estimate in MHz, across a 4-BIOS-tick window.
 * Only call once CPUID leaf 1's EDX bit 4 (TSC) has confirmed the
 * time-stamp counter is present AND in_v86_mode() has confirmed we are
 * in true real mode. RDTSC under a V86 monitor is not safe on every
 * machine: on the vcctrl rig's Pentium OverDrive 83 under MS-DOS 6.22
 * EMM386 NOEMS it never returns at all (see ensure_probed() in cpu.c).
 */
unsigned long measure_mhz_via_tsc(void);

#endif
