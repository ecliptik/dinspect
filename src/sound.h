/* sound.h - sound card detection
 *
 * Every hardware probe here is either non-blocking by construction (a
 * single write+read, never a wait-for-device loop) or bounded with a
 * hard iteration cap that reports a clear failure instead of hanging.
 * Nothing here can wait indefinitely for a device that isn't present --
 * see disk.h for why that discipline matters on an unattended run.
 */

#ifndef DOSFETCH_SOUND_H
#define DOSFETCH_SOUND_H

#include <stddef.h>

/* Formats the raw BLASTER environment variable into buf, or "not set". */
void get_blaster_env(char *buf, size_t buflen);

/* Formats "OPL2/3 detected" or "not detected" into buf, via the classic
 * AdLib timer status test on port 0x388. A single fixed-length
 * sequence, not a wait loop -- always completes quickly.
 */
void get_opl_status(char *buf, size_t buflen);

/* Formats "DSP v<major>.<minor>" if a Sound Blaster responds at the
 * BLASTER-declared base port, "not probed (BLASTER not set)" if there's
 * no BLASTER env var to get a port from (deliberately not guessed --
 * probing an arbitrary port with no evidence a card is there is exactly
 * the kind of ungated risk this project avoids), or "no response
 * (TIMEOUT)" if the declared port didn't answer within a bounded
 * number of polls.
 */
void get_sb_dsp_version(char *buf, size_t buflen);

/* Formats "detected" or "not detected" for an MPU-401 UART at the
 * standard port 0x330 (bounded polling; never hangs).
 */
void get_mpu401_status(char *buf, size_t buflen);

#endif
