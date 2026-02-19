#!/bin/sh
PREREQ=""

prereqs() {
    echo "$PREREQ"
}

case "$1" in
    prereqs)
        prereqs
        exit 0
        ;;
esac

echo "Init ARDECK" > /dev/kmsg

# setup network
ip link set enp0s1 up
ip addr add 10.0.2.15/24 dev enp0s1
ip route add default via 10.0.2.2

mount -t configfs none /sys/kernel/config

PASS="$(/root/client 2>/dev/null | tr -d '\n\r')"
echo $PASS
[ -n "$PASS" ] || exit 1


echo -n $PASS | cryptsetup open /dev/vda2 cryptroot2 --key-file -

mount /dev/mapper/cryptroot2 /tmp/
kexec -l /tmp/modified_test_kernel_dmcrypt --initrd=/tmp/initrd.img-6.14.0-37-generic --command-line="$(cat /proc/cmdline)"
kexec -e
