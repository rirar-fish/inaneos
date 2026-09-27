#!/bin/sh
# 64MB disk, one empty FAT32 partition
set -e
IMG=disk.img
rm -f "$IMG"
qemu-img create -f raw "$IMG" 64M >/dev/null
sfdisk "$IMG" >/dev/null <<'EOF'
label: dos
start=2048, type=c
EOF
MTOOLSRC=$(mktemp)
export MTOOLSRC
printf 'drive z: file="%s" offset=1048576\n' "$(pwd)/$IMG" > "$MTOOLSRC"
mformat -F z: >/dev/null
mdir z:
rm -f "$MTOOLSRC"
