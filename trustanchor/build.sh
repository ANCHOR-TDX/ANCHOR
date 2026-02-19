#!/bin/sh
echo "use provided nix file for build env"

wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.19.10.tar.xz # 466d441a0ea5e04b7023618b7b201bfd60effab225f806fd41ce663484395a1c
cat linux-6.19.10.tar.xz.sha256sum | sha256sum -c
tar xvf linux-6.19.10.tar.xz
mv linux-6.19.10 linux
cp linuxconfig linux/.config
cd linux/
patch arch/x86/kernel/acpi/madt_wakeup.c < ../madt_wakeup.patch
make -j$(nproc) -fdebug-prefix-map

rm -f initramfs.cpio.gz
cd ./initramfs/ && find . | cpio -o --format=newc | gzip > ../initramfs.cpio.gz
