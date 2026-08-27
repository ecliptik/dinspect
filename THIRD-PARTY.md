# Third-Party Notices

dosfetch itself is released under CC0 1.0 Universal (public domain
dedication) — see [LICENSE](LICENSE). This file documents every place
non-dosfetch material was consulted during development, and why none
of it changes that CC0 status.

## PicoGUS protocol (src/picogus.c, src/picogus.h)

PicoGUS's presence-detection and mode-query protocol — port addresses,
magic bytes, command/mode numbers — is documented in PicoGUS's own
source, <https://github.com/polpo/picogus> (GPLv2), specifically
`common/picogus.h` and `pgusinit/pgusinit.c`.

dosfetch's PicoGUS support was written from those documented protocol
facts (port numbers, magic byte values, command/mode enum values), not
by copying PicoGUS's code. Hardware I/O port addresses and protocol
constants are facts about an interface, not copyrightable expression —
the same principle covers the CPUID feature-bit positions and VESA
BIOS Extensions (VBE) structure offsets used elsewhere in dosfetch,
which come from public Intel and VESA specifications. No GPLv2 code is
included in this repository, and no GPLv2 obligations attach to
dosfetch as a result.

## doskutsu (design reference only)

dosfetch is a sibling project to doskutsu, whose `setup/profile.c`
implements broadly similar hardware-detection logic (CPU, memory,
video, sound) under doskutsu's own MIT license. Where dosfetch's
design was informed by doskutsu's general approach — e.g. staged
CPUID detection, INT 15h AX=E801h memory detection, the classic
AdLib/OPL timer test — it was reimplemented independently from the
underlying BIOS/CPUID/hardware interfaces (themselves public,
documented facts), not by copying doskutsu's code, specifically to
keep dosfetch fully CC0.

## Open Watcom (build-time tool only)

dosfetch is built with the Open Watcom C/C++ compiler
(<http://www.openwatcom.org/>), licensed under the Sybase Open Watcom
Public License. Open Watcom is a build-time tool only: it is not
vendored into this repository (a local `tools/watcom/` copy, if
present, is git-ignored — see `.gitignore`), and none of its code is
linked into or distributed with `dosfetch.exe`. Its license governs
the compiler itself, not programs compiled with it, the same way
GCC's license doesn't extend to the programs it compiles.

## Original Pascal implementation (dosfetch.pas)

The original Turbo Pascal implementation this project ports from,
written by Leah Neukirchen, carries its own CC0 waiver in its file
header and is kept in this repository unmodified as a historical
reference. It is under the same CC0 terms as the rest of this project.
