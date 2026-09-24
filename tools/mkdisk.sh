#!/bin/sh
# 64MB disk, one FAT32 partition, sample files
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
echo "hello from fat32" > /tmp/HELLO.TXT
echo "inaneos disk test" > /tmp/README.TXT
echo "nested file ok" > /tmp/NOTE.TXT
mcopy /tmp/HELLO.TXT z: >/dev/null
mcopy /tmp/README.TXT z: >/dev/null
mmd z:DOCS >/dev/null
mcopy /tmp/NOTE.TXT z:DOCS >/dev/null
mdir z:
rm -f "$MTOOLSRC" /tmp/HELLO.TXT /tmp/README.TXT /tmp/NOTE.TXT
