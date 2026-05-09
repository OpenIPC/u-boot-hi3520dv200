# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

OpenIPC fork of HiSilicon's vendor U-Boot 2010.06 for the Hi3520Dv200 DVR SoC. Cortex-A9, V1-era `0x2xxxxxxx` peripheral map, DDR base `0x80000000`. Imported as MVP — vendor source unmodified. Designed to share the build/CI shape of `u-boot-hi3519v101` (legacy mkconfig + `mini-boot.bin` LZMA self-extractor).

## Vendor source quirks

- **Single-target tree.** Only `hi3520d_config` exists in the top-level Makefile. No per-revision (`v100` vs `v200`) branching anywhere — the source already targets v200 silicon (see `arch/arm/cpu/hi3520d/start.S` "DDR Training v200" marker).
- **Naming.** Directories and configs use the family name `hi3520d`, not `hi3520dv200`. Do not rename — the vendor build chain is wired to those paths.
- **Toolchain.** `arch/arm/cpu/hi3520d/compressed/Makefile` pins `CROSS_COMPILE = arm-hisiv200-linux-`. Available as `arm-hisiv200-linux.tgz` in `OpenIPC/toolchains` `v1` release. The vendor toolchain ships gnueabi-suffixed binaries (`arm-hisiv200-linux-gnueabi-*`); CI creates short-name symlinks for the prefix the vendor Makefile expects.
- **U-Boot pre-Kbuild build system.** `mkconfig` shell script generates `config.mk` and `include/config.h` per target. Same generation as `u-boot-hi3519v101`/`hi3516cv200`/`cv300`/`av100`.

## Build (vendor, when set up)

Without OpenIPC env patches:

```sh
make hi3520d_config
make CROSS_COMPILE=arm-hisiv200-linux- -j$(nproc)
make CROSS_COMPILE=arm-hisiv200-linux- mini-boot.bin
```

`mini-boot.bin` is the LZMA-wrapped artifact the bootrom expects (~250 KiB). Build it with the same two-step pattern as `u-boot-hi3519v101`: `u-boot.bin` first, then recurse into `arch/arm/cpu/hi3520d/compressed/`.

## Open questions (MVP gaps)

Everything below is unresolved. The CI build step will fail until each is addressed.

1. **`reg_info_hi3520dv200.bin`** — DDR/PLL register-init blob baked into `mini-boot.bin` via `cp reg_info_<soc>.bin .reg`. Vendor SDKs ship this separately; it isn't in `u-boot-2010.06.tgz`. Source unknown.
2. **OpenIPC env / prompt patches** — sibling repos override `CONFIG_SYS_PROMPT`, `CONFIG_BOOTARGS`, `CONFIG_BOOTCOMMAND`, mtdparts, and friends via a shared `hi-common.h` included from each per-SoC config. This repo doesn't have those yet — `include/configs/hi3520d.h` keeps the vendor's `hisilicon # ` prompt and minimal env.
3. **mtdparts layout** — DVRs typically have larger flash and different partition geometry than the IP-camera fleet's `256k(boot),64k(env),2048k(kernel),5120k(rootfs),...`. The right hi3520dv200 layout needs verification against shipping hardware.
4. **`qemu_smoke` gate** — `widgetii/qemu-hisilicon` has an `hi3520dv200` machine with full Linux boot test. U-Boot flash boot from this machine isn't exercised yet; that step lands once `mini-boot.bin` builds cleanly.
5. **Partition-fit gate** — pending mtdparts decision (item 3). The fleet-wide 256-KiB ceiling is for IP-camera flash; DVRs may have larger boot regions.
6. **Publish to `OpenIPC/firmware/latest`** — gated on (1)-(5). No `u-boot-hi3520dv200-universal.bin` is currently in `firmware/latest`.

## Source layout (familiar from u-boot-hi3519v101)

- `arch/arm/cpu/hi3520d/` — SoC startup, low-level init, DDR training (`ddrphy_train_route.S`)
- `arch/arm/cpu/hi3520d/compressed/` — second-stage LZMA self-extractor that becomes `mini-boot.bin`
- `arch/arm/cpu/hi3520d/hi3520d/` — SoC-within-CPU subdir (cache, reset, timer)
- `board/hi3520d/board.c` — board init
- `include/configs/hi3520d.h` — vendor config header (no OpenIPC overrides yet)
- `include/asm/arch-hi3520d/platform.h` — register-base map (`0x2xxxxxxx`)
