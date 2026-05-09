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

The CI build step will fail until each is addressed.

1. **Register-init blobs `.reg1` + `.reg2`** — `arch/arm/cpu/hi3520d/compressed/Makefile`'s `regfile` rule expects **two** blobs (not the single `.reg` the IP-camera fleet uses). They're DD'd into `mini-boot.bin` at byte offsets [64..2464) and [2464..4864), each 2400 bytes, padded with `conv=sync`. Vendor SDK ships these separately; not in `u-boot-2010.06.tgz`. Without them the build fails the regfile check.
2. **OpenIPC env / prompt patches** — sibling repos override `CONFIG_SYS_PROMPT`, `CONFIG_BOOTARGS`, `CONFIG_BOOTCOMMAND`, mtdparts, etc. via a shared `hi-common.h` included from each per-SoC config. This repo doesn't have those yet — `include/configs/hi3520d.h` keeps the vendor's `hisilicon # ` prompt and minimal env. **Mtdparts layout is settled**: every existing OpenIPC HiSi u-boot uses `mtdparts=hi_sfc:256k(boot),64k(env),2048k(kernel),5120k(rootfs),-(rootfs_data)` for 8 MiB flash regardless of IP-camera vs DVR form factor (`OpenIPC/firmware`'s `hi3536cv100_lite_defconfig` / `hi3536dv100_lite_defconfig` are both 8 MiB with the same layout — no DVR-specific deviation).
3. **`qemu_smoke` gate** — `widgetii/qemu-hisilicon` has an `hi3520dv200` machine. U-Boot flash boot isn't exercised yet; lands once `mini-boot.bin` builds cleanly.
4. **Partition-fit gate** — clone the fleet-wide 256 KiB ceiling once `mini-boot.bin` builds. Same `(boot)` size on all OpenIPC HiSi mtdparts.
5. **Publish to `OpenIPC/firmware/latest`** — gated on (1)-(4). No `u-boot-hi3520dv200-universal.bin` currently in `firmware/latest`.

## Vendor patches already applied (deviations from verbatim vendor source)

The CI bringup needed these to move past trivial blockers:

- **`Makefile`** (top-level): `mini-boot.bin` prereq + `BINIMAGE=` arg switched from `full-boot.bin` to `u-boot.bin`. The vendor recipe expected an external wrapper script to `cp u-boot.bin full-boot.bin`; that script isn't in the SDK. Same fix `u-boot-hi3519v101` applies.
- **`arch/arm/cpu/hi3520d/compressed/Makefile`**: added `product/hiddrtv200/ddrtraining.c` to `SSRC` and `ddrtraining.o` to `COBJS`. `start.S` references `ddrt_entry`; vendor build presumably linked `libhiddrtv200.a` externally, but the compressed-stage Makefile didn't.

## Source layout (familiar from u-boot-hi3519v101)

- `arch/arm/cpu/hi3520d/` — SoC startup, low-level init, DDR training (`ddrphy_train_route.S`)
- `arch/arm/cpu/hi3520d/compressed/` — second-stage LZMA self-extractor that becomes `mini-boot.bin`
- `arch/arm/cpu/hi3520d/hi3520d/` — SoC-within-CPU subdir (cache, reset, timer)
- `board/hi3520d/board.c` — board init
- `include/configs/hi3520d.h` — vendor config header (no OpenIPC overrides yet)
- `include/asm/arch-hi3520d/platform.h` — register-base map (`0x2xxxxxxx`)
