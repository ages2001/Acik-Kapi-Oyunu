#!/bin/bash
# Build the Windows NT 3.1 MAC-model driver (bin/NT31/RLTKLANT.SYS).
#
# The image is produced to match the inbox NT 3.1 NE2000.SYS PE layout exactly:
# section-alignment == file-alignment == 0x200 ("packed" image, section VA ==
# file offset), native subsystem, no DLL flag, no modern DllCharacteristics.
# NT 3.1 maps the driver image from its backing file; a 0x1000-aligned image
# leaves code pages that were never touched during init non-resident, so the
# RX timer DPC faults on its own code at DISPATCH_LEVEL (bugcheck 0xA).  A
# packed 0x200 image loads the way NT 3.1's own drivers do and keeps code
# resident.
set -e
SRC="$(cd "$(dirname "$0")/../src" && pwd)"
OUT="$(cd "$(dirname "$0")" && pwd)/ndisnt31"; mkdir -p "$OUT"; cd "$OUT"
CC=i686-w64-mingw32-gcc; DT=i686-w64-mingw32-dlltool; LD=i686-w64-mingw32-ld
# -march=pentium: NT 3.1 runs on i586; avoid CMOV/other i686-only opcodes
# (gcc emits CMOV for ternaries at -O2, which traps 0x7F on a real Pentium).
ARCH="-march=pentium -mtune=pentium"
USBSRC="$SRC/../USBNT/src"
rm -f *.o
CF="-c -O2 $ARCH -ffreestanding -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -mno-stack-arg-probe -mno-sse -mno-mmx -DRTL_NT31 -DRTL_NDIS3 -DRTL_DBG -D_X86_ -I$SRC -I$USBSRC -Wall -Wno-unused"
OBJECTS=""
python3 "$SRC/../build/make-fwimages.py" --check
python3 "$SRC/../build/make-usb-phy.py" --check
for f in drvnt31 nt31usb backend_usb usbntclient rtlport rtlcrt fwvm fwbus fwimages physeq; do
    $CC $CF "$SRC/$f.c" -o "$f.o"
    OBJECTS="$OBJECTS $f.o"
done
$CC $CF "$SRC/../third_party/r8169/phy_config.c" -o phy_config.o
OBJECTS="$OBJECTS phy_config.o"
for f in usbpol usbphy rtl815x rtl8150 usbbind usbnic usbplug usbcli usbfw usbfwsha usbfwexec usbfwio usbphyseq; do
    $CC $CF "$USBSRC/$f.c" -o "$f.o"
    OBJECTS="$OBJECTS $f.o"
done
UND=$(i686-w64-mingw32-nm *.o | awk '/ U /{print $2}' | sort -u)
{ echo "LIBRARY NDIS.SYS"; echo EXPORTS; echo "$UND" | grep -E '^_?(Ndis|Eth)' | sed 's/^_//'; } > ndis.def
# KeRaiseIrql/KeLowerIrql are real exports in HAL.DLL but only *forwarders* in
# ntoskrnl.exe (NT 3.1's loader can't resolve a forwarded import) - import them
# straight from HAL.
{ echo "LIBRARY HAL.DLL";  echo EXPORTS; echo "$UND" | grep -E '^_?(READ_PORT|WRITE_PORT|KeStall|KeRaiseIrql|KeLowerIrql|KeAcquireSpinLock|KeReleaseSpinLock)' | sed 's/^_//'; } > hal.def
{ echo "LIBRARY NTOSKRNL.EXE"; echo EXPORTS; echo "$UND" | grep -E '^_?(ExQueueWorkItem|Mm|Io|Ps|Zw|KeDelayExecution|KeInitializeEvent|KeResetEvent|KeSetEvent|KeCancelTimer|KeWaitForSingleObject|Ob|NtWaitForSingleObject)' | sed 's/^_//'; } > ntos.def
$DT --kill-at -d ndis.def -l libndis.a; $DT --kill-at -d hal.def -l libhal.a; $DT --kill-at -d ntos.def -l libntos.a
# --dll keeps base relocations (NT loads the driver at a runtime base); the DLL
# file flag is cleared below.  section-alignment 0x200 == file-alignment -> the
# NE2000-style packed layout.
$LD -o rltklant.sys $OBJECTS -L. -lndis -lhal -lntos \
   --subsystem=native --image-base=0x10000 --section-alignment=0x200 --file-alignment=0x200 \
   --major-subsystem-version 3 --minor-subsystem-version 10 -e _DriverEntry@8 --dll --nostdlib
# Post-process into a NE2000-style flat image that NT 3.1 can load.
#   1. Rewrite to a FLAT layout: PointerToRawData == VirtualAddress for every
#      section, with .bss (and any gap) backed by real zero bytes in the file.
#      ld leaves .bss with PointerToRawData=0 / SizeOfRawData=0, which makes the
#      file offset of every later section (notably .idata) differ from its RVA.
#      NT 3.1 maps a salign==falign image flat from the file, so that shift
#      corrupts the import table and the load silently fails ("service failed").
#      NE2000.SYS/LANCE.SYS have no such shift; this reproduces that layout.
#   2. clear IMAGE_FILE_DLL (0x2000); set the cosmetic bits NE2000 carries so
#      FileCharacteristics==0x838e; zero DllCharacteristics.
#   3. recompute the (NT 3.1-validated) PE header checksum.
python3 - "$OUT/rltklant.sys" <<'PYEOF'
import struct,sys
fn=sys.argv[1]; old=bytearray(open(fn,'rb').read())
pe=struct.unpack('<I',old[0x3c:0x40])[0]; opt=pe+24
nsec=struct.unpack('<H',old[pe+6:pe+8])[0]
osz=struct.unpack('<H',old[pe+20:pe+22])[0]
falign=struct.unpack('<I',old[opt+36:opt+40])[0]
sohdr=struct.unpack('<I',old[opt+60:opt+64])[0]
so=opt+osz
def roundup(x,a): return (x+a-1)&~(a-1)
secs=[]
for i in range(nsec):
    b=so+i*40
    vs=struct.unpack('<I',old[b+8:b+12])[0]; va=struct.unpack('<I',old[b+12:b+16])[0]
    rs=struct.unpack('<I',old[b+16:b+20])[0]; rp=struct.unpack('<I',old[b+20:b+24])[0]
    secs.append([i,b,vs,va,rs,rp])
# new file size = last section VA + its (file-aligned) raw size
total=sohdr
for s in secs:
    rawn=roundup(max(s[2],s[4]),falign)
    total=max(total, s[3]+rawn)
new=bytearray(total)
new[0:sohdr]=old[0:sohdr]                      # PE headers unchanged
for s in secs:
    idx,b,vs,va,rs,rp=s
    nm=old[b:b+8].rstrip(b'\0').decode('latin1')
    rawn=roundup(max(vs,rs),falign)
    if rp and rs:                               # copy initialised section data
        n=min(rs,rawn); new[va:va+n]=old[rp:rp+n]
    # .bss / gaps stay zero (buffer is zero-initialised)
    struct.pack_into('<I',new,b+16,rawn)        # SizeOfRawData
    struct.pack_into('<I',new,b+20,va)          # PointerToRawData = VirtualAddress
    if nm=='.reloc':                            # keep relocations resident:
        c=struct.unpack('<I',new[b+36:b+40])[0] # DriverEntry copies the image to
        struct.pack_into('<I',new,b+36,c&~0x02000000)  # pool and re-applies them
d=new
ch=struct.unpack('<H',d[pe+22:pe+24])[0]
ch&=~0x2000                 # clear IMAGE_FILE_DLL
ch|=0x0008|0x0080|0x8000    # LOCAL_SYMS_STRIPPED | BYTES_REVERSED_LO | _HI
struct.pack_into('<H',d,pe+22,ch)
struct.pack_into('<H',d,opt+70,0)               # DllCharacteristics = 0
# Match NE2000's version stamps exactly.  NT 3.1's image loader appears to
# demand-page images stamped with a "new" OS/subsystem version (OSVer 4.0 /
# SubsysVer 3.10 as emitted by binutils) while loading "1.0" images fully
# resident the old way - so a DPC on a 4.0-stamped image faults 0xA on its own
# (now pageable) code.  Stamp OS 1.0 / Image 0.0 / Subsystem 1.0 like NE2000.
struct.pack_into('<H',d,opt+40,1); struct.pack_into('<H',d,opt+42,0)  # OS 1.0
struct.pack_into('<H',d,opt+44,0); struct.pack_into('<H',d,opt+46,0)  # Image 0.0
struct.pack_into('<H',d,opt+48,1); struct.pack_into('<H',d,opt+50,0)  # Subsys 1.0
cko=opt+64
struct.pack_into('<I',d,cko,0)
s=0
for i in range(0,len(d)&~1,2):
    s+=d[i]|(d[i+1]<<8); s=(s&0xffff)+(s>>16)
if len(d)&1:
    s+=d[-1]; s=(s&0xffff)+(s>>16)
s=(s&0xffff)+(s>>16); s=(s&0xffff)+(s>>16)
s=(s+len(d))&0xffffffff
struct.pack_into('<I',d,cko,s)
open(fn,'wb').write(d)
print("flat image: size=%d FileChar=0x%04x checksum=0x%08x"%(len(d),ch,s))
PYEOF
echo "built $OUT/rltklant.sys"
