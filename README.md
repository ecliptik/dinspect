# dinspect

A small DOS program that prints a system inventory — OS, CPU, memory,
disks, video, sound, network, and more — either to the screen (with a
neofetch-style ASCII logo) or as a plain-text report to a file.

This project was 100% built agentically using [Claude Code](https://docs.anthropic.com/en/docs/claude-code).

Ported from the original [leahneukirchen/dosfetch](https://github.com/leahneukirchen/dosfetch)
written in Turbo Pascal 8 to C, built with Open Watcom for 16-bit
real-mode DOS (`src/`). Runs in Real-mode with no DPMI dependency,
supporting the widest range of DOS-capable hardware and boot
configurations.

![Screenshot of dinspect](screenshot-c.png)

## Features

- **System**: DOS vendor/version, shell (`COMSPEC`)
- **CPU**: vendor, family/model/stepping, and feature flags via
  CPUID where available; clock speed via TSC on Pentium-class+ in true
  real mode, or a labeled loop-throughput estimate on earlier CPUs and
  under a V86 memory manager such as EMM386 (RDTSC under one has hung
  real hardware)
- **Memory**: base (conventional) and extended memory
- **Disks**: floppy drive count, and free/total space for every
  hard-disk-class drive letter from C: onward
- **Video**: adapter OEM string, VBE version, and video memory via
  VESA BIOS Extensions; chipset make/model via a PCI configuration-space
  scan, independent of whatever answers the VBE OEM string
- **Sound**: `BLASTER` environment variable, AdLib/OPL2/3 detection,
  Sound Blaster DSP version, MPU-401 presence
- **PicoGUS**: presence and current emulation mode (GUS/SB/AdLib/MPU/
  etc.) for this RP2040-based ISA sound card, if installed
- **Network**: packet driver presence (by interrupt vector), and IP/
  gateway configuration from mTCP's `CONFIG.MTC` if `MTCPCFG` is set
- **Runtime**: how long detection took, to help external tooling
  calibrate polling delays

## Usage

```
dinspect [options]

  --no-logo          Do not draw the ASCII logo
  --plain            Monochrome screen output, no logo (for OCR capture)
  --no-color         Disable color output; keeps the logo and layout
  -o, --out FILE     Also write a plain-text report to FILE
  --show-undetected  Include fields that couldn't be detected (UNKNOWN,
                     not detected, etc.) instead of omitting them
  -h, --help         Show this help
```

With `-o`, the report file is truncated to zero bytes before any probing
starts and written in full only once every probe has finished. An empty
report therefore means the run did not complete (hang, crash, reboot),
never a stale copy from an earlier run -- anything that fetches the report
by name after a timed run should treat an empty file as a failed run.

## Building

You need [Open Watcom](http://www.openwatcom.org/) installed, with the
`WATCOM` environment variable set (as its own installer normally
does). Alternatively, vendor a copy under `tools/watcom` (gitignored,
not included in the repository) and the Makefile will fall back to it
automatically:

```
make
```

This produces `dinspect.exe`. Object files land in `build/`; `make
clean` removes both.

## Copying

Originally written by Leah Neukirchen <leah@vuxu.org>.

To the extent possible under law, the creator(s) of this work have
waived all copyright and related or neighboring rights to this work.
See [LICENSE](LICENSE) (CC0 1.0 Universal), and
[THIRD-PARTY.md](THIRD-PARTY.md) for where outside material was
consulted (and why it doesn't affect that CC0 status).
