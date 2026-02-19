update-initramfs -u -k all
grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=ubuntu
grub-install --target=x86_64-efi --efi-directory=/boot/efi --removable
update-grub

