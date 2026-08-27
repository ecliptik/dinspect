/* critical_error.h - install a silent DOS critical-error (INT 24h)
 * handler for the lifetime of the program.
 *
 * Without this, a "device not ready" style error anywhere in dosfetch
 * (an empty floppy, an empty CD-ROM drive, etc.) shows DOS's
 * interactive "Abort, Retry, Fail?" prompt and waits forever for a
 * keypress that will never come on an unattended run -- exactly the
 * disk-enumeration hang this project has already hit once for real
 * (see disk.h). install_silent_critical_handler() makes every such
 * error resolve itself immediately as a Fail, returning a normal DOS
 * error code to whatever call triggered it instead of blocking.
 *
 * Uses Watcom's own _harderr()/_HARDERR_FAIL runtime library facility
 * (see <dos.h>) rather than a hand-rolled interrupt handler: it's the
 * well-tested mechanism generations of DOS software have used for
 * exactly this purpose, not something bespoke to this project.
 */

/* No matching "restore" call is needed: DOS saves each program's INT
 * 22h/23h/24h vectors in its PSP at load time and restores all three
 * automatically on normal process termination (INT 21h AH=4Ch, which
 * is what returning from main() triggers) -- precisely so a
 * terminated program can never leave a dangling INT 24h vector
 * pointing at freed memory for the next critical error to crash into.
 */
#ifndef DOSFETCH_CRITICAL_ERROR_H
#define DOSFETCH_CRITICAL_ERROR_H

void install_silent_critical_handler(void);

#endif
