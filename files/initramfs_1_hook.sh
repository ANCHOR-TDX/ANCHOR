set -e

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

. /usr/share/initramfs-tools/hook-functions

copy_exec /usr/sbin/kexec /usr/sbin/kexec
copy_exec /root/trustauthority-cli /usr/bin/trustauthority-cli
copy_exec /root/CA_cert.pem /CA_cert.pem
copy_exec /root/client /root/client
