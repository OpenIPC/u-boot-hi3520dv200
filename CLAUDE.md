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

## MVP status — build is green

CI builds `u-boot-hi3520dv200-universal.bin` (104,911 B, `mini-boot.bin` LZMA-wrapped artifact) cleanly from a fresh clone. Structure matches the SDK V2.0.5.1 prebuilt: 64-byte ARM vector header → 4800 bytes register-init blob → LZMA-compressed inner U-Boot payload.

Size delta vs SDK prebuilt (228,344 B) is from feature footprint — vanilla `hi3520d_config` builds a smaller U-Boot than the vendor SDK's post-processed config. Not a correctness problem; will narrow as OpenIPC patches add the env/cmd/mtdparts overrides.

## Remaining MVP gaps

1. **OpenIPC env / prompt patches** — sibling repos override `CONFIG_SYS_PROMPT`, `CONFIG_BOOTARGS`, `CONFIG_BOOTCOMMAND`, mtdparts, etc. via a shared `hi-common.h` included from each per-SoC config. This repo still has the vendor's `hisilicon # ` prompt and minimal env. **Mtdparts layout is settled**: clone the IP-camera fleet's `mtdparts=hi_sfc:256k(boot),64k(env),2048k(kernel),5120k(rootfs),-(rootfs_data)` for 8 MiB flash (`OpenIPC/firmware`'s `hi3536cv100_lite_defconfig` / `hi3536dv100_lite_defconfig` confirm DVRs and IP cameras share the same layout — no DVR-specific deviation).
2. **`qemu_smoke` gate** — blocked on [`widgetii/qemu-hisilicon#89`](https://github.com/widgetii/qemu-hisilicon/issues/89). The qemu-hisilicon `hi3520dv200` machine boots Linux fine via `-kernel`, but U-Boot flash boot produces only UART padding — verified with both our build and the SDK's prebuilt, so it's a model gap, not a build issue. Same class as the closed #59 (av100) and #79 (Goke V4) silent-UART gaps; the existing `qemu_smoke` step from the IP-camera fleet drops in once #89 lands.
3. **Partition-fit gate** — clone the fleet-wide 256 KiB ceiling. One step, same as every other repo in the fleet.
4. **Publish to `OpenIPC/firmware/latest`** — gated on (2). No `u-boot-hi3520dv200-universal.bin` currently in `firmware/latest`.

## Vendor patches applied (deviations from verbatim vendor source)

These were needed to make the vendor source actually build under CI:

- **`Makefile`** (top-level): `mini-boot.bin` prereq + `BINIMAGE=` arg switched from `full-boot.bin` to `u-boot.bin`. Vendor recipe expected an external wrapper script to `cp u-boot.bin full-boot.bin`; that script isn't in the SDK. Same fix `u-boot-hi3519v101` applies.
- **`arch/arm/cpu/hi3520d/compressed/Makefile`**: pulled `product/hiddrtv200/ddrtraining.c` (+ `ddrtraining.h` + `ddrtv200.h`) into `SSRC` so the symlink-staging rule lands them next to the wrapper sources, then added `ddrtraining.o` to `COBJS`. Vendor build linked `libhiddrtv200.a` externally; the compressed-stage Makefile didn't already wire it.
- **`arch/arm/cpu/hi3520d/compressed/startup.c`**: inlined a `memset` (verbatim from `lib/string.c`) and a no-op `printf` stub. `ddrtraining.c` calls both; pulling `lib/string.c` whole drags in `kstrdup` → `malloc` which the wrapper doesn't have, so inlining the one function we actually need is the lighter touch.
- **`reg_info_hi3520dv200.bin1` + `.bin2`** (new files): two 2400-byte register-init blobs extracted byte-verbatim from `Hi3520D_SDK_V2.0.5.1`'s prebuilt `u-boot_hi3520d.bin` via the same `dd skip=64/2464 count=2400` operation the vendor's `mkboot-hi3520d.sh` applies in reverse. `.bin1` is bit-identical to `reg_info_Hi3520D-bvt_No1_660_330_660_ddr_innerFEPHY.bin` padded to 2400 bytes (NUL fill); `.bin2` is a related second-stage init not shipped as a standalone file in the SDK directory.

## Source layout (familiar from u-boot-hi3519v101)

- `arch/arm/cpu/hi3520d/` — SoC startup, low-level init, DDR training (`ddrphy_train_route.S`)
- `arch/arm/cpu/hi3520d/compressed/` — second-stage LZMA self-extractor that becomes `mini-boot.bin`
- `arch/arm/cpu/hi3520d/hi3520d/` — SoC-within-CPU subdir (cache, reset, timer)
- `board/hi3520d/board.c` — board init
- `include/configs/hi3520d.h` — vendor config header (no OpenIPC overrides yet)
- `include/asm/arch-hi3520d/platform.h` — register-base map (`0x2xxxxxxx`)
