# Run make from src. All host build products live outside the distribution.
B ?= /tmp/nt3scprt-build
CROSS ?= i686-w64-mingw32-
WINCC ?= $(CROSS)gcc
WINDRES ?= $(CROSS)windres
WINFLAGS = -Os -std=gnu11 -march=i386 -ffreestanding -fno-builtin -fno-stack-protector -mno-stack-arg-probe -mno-sse -fno-ident -Wall -Wextra -I$(B)
LDFLAGS = -nostdlib -Wl,--subsystem,windows:3.10,--major-os-version,3,--minor-os-version,10,--disable-dynamicbase,--disable-nxcompat,--no-insert-timestamp,-s
all: ../bin/SETUP.EXE ../bin/CHECK.EXE ../bin/RELIO.EXE ../bin/LSITEST.EXE
$(B):
	mkdir -p $(B)
$(B)/blob.h: port.c native.s port.ld mkblob.py | $(B)
	gcc -m32 -march=i386 -Os -fpie -ffreestanding -fno-builtin -fno-strict-aliasing -fno-stack-protector -fno-jump-tables -fno-asynchronous-unwind-tables -fno-unwind-tables -c port.c -o $(B)/port.o
	as --32 native.s -o $(B)/native.o
	ld -m elf_i386 -T port.ld $(B)/port.o $(B)/native.o -o $(B)/port.elf
	objcopy -O binary $(B)/port.elf $(B)/port.bin
	python3 mkblob.py $(B)
$(B)/app.o: app.rc app.ico remove.ico | $(B)
	$(WINDRES) -i app.rc -o $(B)/app.o
../bin/SETUP.EXE: setup.c store.c shell.c manage.c adapt.c adapt.h common.h scsiid.c scsiid.h optical.c classmap.h patch.c patch.h hash.c mem.c $(B)/blob.h $(B)/app.o
	$(WINCC) $(WINFLAGS) $(LDFLAGS) -Wl,-e,_start setup.c store.c shell.c manage.c adapt.c patch.c hash.c optical.c scsiid.c mem.c $(B)/app.o -o $@ -luser32 -lkernel32 -ladvapi32 -lcomdlg32

../bin/CHECK.EXE: check.c scsiid.c scsiid.h mem.c
	$(WINCC) $(WINFLAGS) $(LDFLAGS) -Wl,-e,_start check.c scsiid.c mem.c -o $@ -luser32 -lkernel32 -ladvapi32

../bin/RELIO.EXE: relio.c relbase.h block.inc scsiid.c scsiid.h mem.c
	$(WINCC) $(WINFLAGS) $(LDFLAGS) -Wl,-e,_start relio.c scsiid.c mem.c -o $@ -luser32 -lkernel32 -ladvapi32

../bin/LSITEST.EXE: lsitest.c relbase.h block.inc scsiid.c scsiid.h mem.c
	$(WINCC) $(WINFLAGS) $(LDFLAGS) -Wl,-e,_start lsitest.c scsiid.c mem.c -o $@ -luser32 -lkernel32 -ladvapi32

# Optional Linux-side verification with Python pefile and local NT binaries.
verify: $(B)/verify.so

$(B)/verify.so: adapt.c adapt.h patch.c patch.h $(B)/blob.h
	gcc -O2 -fPIC -shared -I$(B) adapt.c patch.c -o $@
