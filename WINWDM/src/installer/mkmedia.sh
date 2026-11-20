#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d build/disk.XXXXXX)
trap 'rm -f "$work/WINWDM.IMA" "$work/README.TXT"; rmdir "$work"' EXIT
image=$work/WINWDM.IMA
mformat -C -i "$image" -f 1440 -v WDMKIT ::
sed 's/$/\r/' installer/DISK.TXT > "$work/README.TXT"
mcopy -i "$image" "$work/README.TXT" ::
for edition in WIN3 NT3 DESKTOP; do
    mmd -i "$image" "::$edition"
    mcopy -i "$image" "build/$edition/SETUP.EXE" "::$edition"
done
for edition in WIN3 NT3; do
    mcopy -i "$image" "build/$edition/INFO.EXE" "build/$edition/UNINST.EXE" "::$edition"
done
mcopy -i "$image" build/WIN3/*.386 ::WIN3
mcopy -i "$image" build/NT3/*.SYS ::NT3
mv "$image" build/WINWDM.IMA
