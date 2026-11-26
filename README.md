# Retired: hi3520dv200 is built in OpenIPC/u-boot

This repository is archived. U-Boot for Hi3520DV200 is now built from the same tree as
every other HiSilicon and Goke SoC OpenIPC supports: the `openipc-hisilicon`
branch of [OpenIPC/u-boot](https://github.com/OpenIPC/u-boot/tree/openipc-hisilicon),
with its CI on the `openipc` branch. That build is published as
`u-boot-hi3520dv200-nor.bin` in the
[OpenIPC/firmware `latest`](https://github.com/OpenIPC/firmware/releases/tag/latest)
release; it finds the flash layout at boot, fixed slots or the split NOR image
(OpenIPC/u-boot#77).

The U-Boot 2010.06 image this repository built, `u-boot-hi3520dv200-universal.bin`,
stays on `latest` as it is: qemu-bench CI and defib still use it, and devices
in the field run it. It is no longer rebuilt.

The upstream U-Boot documentation is in `README`; the history here stays
readable for reference.
