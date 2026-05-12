# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

OpenIPC fork of HiSilicon's vendor U-Boot 2010.06 for the Hi3520Dv200 DVR SoC (Cortex-A9, V1-era `0x2xxxxxxx` peripheral map, DDR base `0x80000000`). Imported from `Hi3520D_SDK_V2.0.5.1`. Builds via the OpenIPC HiSi U-Boot fleet's standard CI shape: build → 256 KiB partition-fit → qemu_smoke against widgetii/qemu-hisilicon's `hi3520dv200` machine → publish to `OpenIPC/firmware/latest`.

## Vendor source quirks

- **Single-target tree.** Only `hi3520d_config` exists in the top-level Makefile. No per-revision (`v100` vs `v200`) branching anywhere — the source already targets v200 silicon (see `arch/arm/cpu/hi3520d/start.S` "DDR Training v200" marker).
- **Naming.** Directories and configs use the family name `hi3520d`, not `hi3520dv200`. Do not rename — the vendor build chain is wired to those paths. The repo and published artifact name use `hi3520dv200` to match `BR2_OPENIPC_SOC_MODEL`.
- **Two regfile slots.** Unlike single-blob siblings (hi3536c=5120B, hi3536d=8192B), `mkboot-hi3520d.sh` lays out two 2400-byte register-init blobs at offsets 64 and 2464, then u-boot.bin from offset 4864. Both blobs are required. `hi3520d.h`'s `ENABLE_HI3520D_BLANK` + `ENABLE_HI3515A_BLANK` reserve the matching 4800-byte blank zone inside u-boot.bin's `start.S` so the dd dance lands the regs in-place.
- **Toolchain.** `arm-hisiv200-linux-` (gcc 4.4.1, glibc 2.11). Available as `arm-hisiv200-linux.tgz` in `OpenIPC/toolchains` v1. The vendor toolchain ships gnueabi-suffixed binaries (`arm-hisiv200-linux-gnueabi-*`); CI creates short-name symlinks for the prefix the vendor Makefiles expect.
- **U-Boot pre-Kbuild build system.** `mkconfig` shell script generates `config.mk` and `include/config.h` per target. Same generation as `u-boot-hi3519v101` / `hi3516cv200` / `hi3516cv300` / `hi3516av100`.
- **`compressed/` mini-boot subdir is unused.** The SDK ships an LZMA self-extractor under `arch/arm/cpu/hi3520d/compressed/` but the vendor's published prebuilt `u-boot_hi3520d.bin` does NOT use it — it's a raw `u-boot.bin` + reg-blob preamble (verified byte-for-byte against the SDK V2.0.5.1 prebuilt). We follow the vendor's artifact pattern; the `compressed/` tree stays in-source for archival but is never built by CI.

## Universal binary shape: raw u-boot.bin + reg blobs (vendor's pattern)

Two valid artifact patterns exist in the OpenIPC HiSi U-Boot fleet, picked per-SoC by what the vendor SDK actually ships as a prebuilt:

1. **Mini-boot LZMA self-extractor.** `compressed/` produces `mini-boot.bin` whose prelude is a self-contained SRAM-fitting SPL with locally-linked DDR training. `u-boot-hi3519v101` et al. ship this — `defib`'s `_detect_spl_size` finds a valid LZMA boundary in 0x4000..0x10000 and uses it as the SPL boundary.
2. **Raw u-boot.bin + reg-blob preamble** (this repo). `defib` finds no valid LZMA in its scan window and falls back to its safe small-SPL profile (`spl_max_size = 0x2300`, 8.7 KB). Bootrom executes a tiny u-boot prefix, then bootrom uploads the full binary to DDR and transfers control there.

Hi3520Dv200's vendor SDK ships pattern 2. Pattern 1 is wrong here because hi3520d's `start.S` `normal_start_flow` path ends with an absolute `mov pc, 0x58000000+ddrt_entry_offset` (a flash-XIP jump into the SFC-mapped region) — works in cold-boot from real flash, faults in defib's fastboot context where SFC isn't mapped. The compressed/ tree was tried first as an MVP and failed hardware testing via defib (issue #1); reverted to vendor's pattern 2026-05-12.

## OpenIPC patches (deviations from verbatim vendor source)

- **`include/configs/hi-common.h`** (new file): cloned verbatim from `u-boot-hi3519v101`. OpenIPC fleet env block (mtdparts, OpenIPC # prompt, env helpers, etc.).
- **`include/configs/hi3520d.h`**: sets `CONFIG_PRODUCTNAME = "hi3520dv200"` so the env's `soc` var matches `OpenIPC/firmware`'s `hi3520dv200_lite_defconfig`. `#include <configs/hi-common.h>` at the end, then `#undef` the heavy commands the SPI-NOR-squashfs flow doesn't need (`CMD_UBI/UBIFS/USB/PCMCIA/FAT/EXT2/FS_GENERIC` + `OSD_ENABLE`) — same trim as `u-boot-hi3536cv100` / `u-boot-hi3536dv100`. Without the trim, u-boot.bin exceeds the 256 KiB boot partition gate (raw, untrimmed = ~310 KiB; trimmed = ~150 KiB).
- **`reg_info_hi3520dv200.bin1` + `.bin2`** (new files): two 2400-byte register-init blobs extracted byte-verbatim from `Hi3520D_SDK_V2.0.5.1`'s prebuilt `u-boot_hi3520d.bin` via `dd skip=64/2464 count=2400` (the inverse of `mkboot-hi3520d.sh`'s dd dance). `.bin1` is bit-identical to `reg_info_Hi3520D-bvt_No1_660_330_660_ddr_innerFEPHY.bin` padded to 2400 bytes (NUL fill); `.bin2` is a related second-stage init not shipped as a standalone file in the SDK directory.

## Build (manual)

```sh
make clean
make hi3520d_config
make CROSS_COMPILE=arm-hisiv200-linux- u-boot.bin -j$(nproc)
# Vendor mkboot-hi3520d.sh-equivalent dd dance:
dd if=u-boot.bin                of=t1 bs=1    count=64    2>/dev/null
dd if=reg_info_hi3520dv200.bin1 of=t2 bs=2400 conv=sync   2>/dev/null
dd if=reg_info_hi3520dv200.bin2 of=t3 bs=2400 conv=sync   2>/dev/null
dd if=u-boot.bin                of=t4 bs=1    skip=4864   2>/dev/null
cat t1 t2 t3 t4 > u-boot-hi3520dv200-universal.bin
rm -f t1 t2 t3 t4
```

The mini-boot phony target (`make mini-boot.bin`) exists in the top-level Makefile and works (recurses into `compressed/`), but its output is unused — the published artifact is the raw u-boot.bin + reg-blobs assembly above.

## Source layout

- `arch/arm/cpu/hi3520d/` — SoC startup, low-level init, DDR training (`ddrphy_train_route.S`)
- `arch/arm/cpu/hi3520d/compressed/` — second-stage LZMA self-extractor; in-source for archival, unused by CI
- `arch/arm/cpu/hi3520d/hi3520d/` — SoC-within-CPU subdir (cache, reset, timer)
- `board/hi3520d/board.c` — board init
- `include/configs/hi3520d.h` — per-SoC config; `#include <configs/hi-common.h>` at the end, plus the SPI-NOR-squashfs feature trim
- `include/configs/hi-common.h` — OpenIPC shared env block (mtdparts, prompt, bootcmd, env helpers); identical across the fleet
- `include/asm/arch-hi3520d/platform.h` — register-base map (`0x2xxxxxxx`)
- `reg_info_hi3520dv200.bin1` + `.bin2` — DDR/PLL register-init blobs, prepended to u-boot.bin via `mkboot-hi3520d.sh`'s dd dance
