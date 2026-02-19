# ANCHOR
Research prototype PoC, Preliminary Steps, Automation & Full Artifact is WIP

Test / reference setup: Ubuntu 25.10, Kernel 6.17.0-14-generic, INTEL(R) XEON(R) SILVER 4509Y   
The creation of the image also works on non-TDX enabled hardware.
 
## Baseline
For general TDX setup, follow the instructions here `https://github.com/canonical/tdx`

### Create base TD
Tested with `b24078e631ce8dedbec16c2f33e2a0d0972f9023` and Ubuntu 24.04 / Linux 6.8.0-107
```
git clone https://github.com/canonical/tdx.git 
cd tdx/guest-tools/image
sudo ./create-td-image.sh -v 24.04
```

## ANCHOR
We assume that we already have a valid qcow2 image, like the one in the Baseline step above.   
In this guide, we preserve the original image by creating a new one. Despite this, we recommend to create a backup of the original image.   

## Create ANCHOR image
```
qemu-img create -f qcow2 encrypted.qcow2 50G
```

execute `./run_notd_unencrypted`    
execute `partition_enc_rootfs.sh` or `partition.sh` in VM, the latter one will not encrypt the rootfs    
execute: `partition2.sh` in VM. (Two seperate scripts because of `chroot`)   
reboot vm

execute `./run_notd_encrypted` -> At this stage, there is a simple password prompt to decrypt the rootFS if encryption enabled before    

Create Trust Anchor partition:  
```
echo -n "123456" | cryptsetup luksFormat /dev/vdb2 --integrity hmac_sha256 --batch-mode --key-file=-
cryptsetup open /dev/vda2 c
mkdir -p mnt && mount /dev/mapper/c mnt
cp /boot/vmlinuz-6.8.0-107-generic /mnt/
cp /boot/initrd.img-6.8.0-107-generic /mnt/
# copy Trust Anchor and its initramfs to in the TD /root/. Alternatively mount the boot partition of the qcow2 with qemu-nbd 
cp /root/bzImage /boot/vmlinuz-6.8.0-107-generic
cp /root/initramfs.cpio.gz /boot/initrd.img-6.8.0-107-generic
``` 
Now the system boots into the Trust Anchor. The initramfs init script automatically performs all operations of the protocol.
