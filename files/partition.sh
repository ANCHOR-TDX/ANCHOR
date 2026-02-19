# RUN INSIDE VM ONLY
# qemu-img create -f qcow2 encrypted.qcow2 50G
#!/bin/bash
# set -e

DST=/dev/vdb

# Wipe existing table
sgdisk --zap-all $DST

# BIOS boot
sgdisk -n 14:2048:10239 -t 14:ef02 $DST

# EFI
sgdisk -n 15:10240:227327 -t 15:ef00 $DST

# Linux extended boot
sgdisk -n 16:227328:2097152 -t 16:ea00 $DST

# Main Linux filesystem TODO hier mehr und unten weniger speicher 
sgdisk -n 1:2099200:75499519 -t 1:8300 $DST

# seperate partition for new kernel + new initramfs
sgdisk -n 2:75499520:0 -t 2:8300 $DST

partprobe $DST

mkfs.vfat -F32 -n UEFI /dev/vdb15 && mkfs.ext4 -L BOOT /dev/vdb16 && mkfs.ext4 -L ROOT /dev/vdb1
mount /dev/vdb1 /mnt && mkdir -p /mnt/boot/efi && mount /dev/vdb16 /mnt/boot && mkdir -p /mnt/boot/efi && mount /dev/vdb15 /mnt/boot/efi
rsync -aAXHvv --info=progress2 --one-file-system exclude=/mnt/** --exclude=/proc/** --exclude=/sys/** --exclude=/dev/** --exclude=/run/** --exclude=/tmp/** --exclude=/lost+found / /mnt
rsync -aAXH /boot/ /mnt/boot/

set -euo pipefail

FSTAB_FILE=/mnt/etc/fstab

# Map devices to UUIDs
ROOT_UUID=$(blkid -s UUID -o value /dev/vdb1)
BOOT_UUID=$(blkid -s UUID -o value /dev/vdb16)
EFI_UUID=$(blkid -s UUID -o value /dev/vdb15)

# Replace LABEL=... with UUID=... in fstab
sed -i \
    -e "s|^LABEL=cloudimg-rootfs|UUID=$ROOT_UUID|" \
    -e "s|^LABEL=BOOT|UUID=$BOOT_UUID|" \
    -e "s|^LABEL=UEFI|UUID=$EFI_UUID|" \
    "$FSTAB_FILE"

echo "Updated $FSTAB_FILE with UUIDs:"
cat "$FSTAB_FILE" # debug

echo "cryptroot UUID=$ROOT_UUID none luks,discard" >> /mnt/etc/crypttab

# chroot into new fs
for i in /dev /dev/pts /proc /sys /run; do mount --bind $i /mnt$i; done
chroot /mnt

update-initramfs -u -k all
grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=ubuntu
grub-install --target=x86_64-efi --efi-directory=/boot/efi --removable
update-grub
