CROSS ?= i686-w64-mingw32-
CC32 ?= $(CROSS)gcc
DRIVER_CFLAGS = -m32 -march=i386 -O2 -ffreestanding -fno-builtin -fno-stack-protector -fno-ident -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -Iinclude
NT_DRIVER_LDFLAGS = -nostdlib -Wl,--subsystem,native:3.10,--major-os-version,3,--minor-os-version,10,--entry,_DriverEntry@8,--image-base,0x10000,--section-alignment,4096,--file-alignment,4096,--enable-reloc-section,--disable-auto-import,--disable-dynamicbase,--disable-nxcompat,--exclude-all-symbols,--no-insert-timestamp
DDK_INCLUDE ?= /usr/i686-w64-mingw32/include/ddk
JWLINK ?= jwlink
NASM ?= nasm

.PHONY: all kernel sdk check
all: kernel
TARGET_IDS = 100 105 110 120 130
NT_IMAGES = build/WINWDM.SYS $(foreach v,$(TARGET_IDS),build/W$(v).SYS)
VMM_IMAGES = build/WINWDM.386 $(foreach v,$(TARGET_IDS),build/W$(v).386)
kernel: $(NT_IMAGES) $(VMM_IMAGES)
build/WINWDM.SYS build/WINWDM.386: TARGET_OBJ = build/target600.o
$(foreach v,$(TARGET_IDS),$(eval build/W$(v).SYS build/W$(v).386: TARGET_OBJ = build/target$(v).o))

build/target%.o: kernel/target.c include/wdm_target.h include/protocol.h | build
	$(CC32) $(DRIVER_CFLAGS) -DWK_TARGET_INDEX=WK_INDEX_$* -c $< -o $@
sdk: build/libwinwdm.a build/libwinwdm-vmm.a

build/libwinwdm.a: build/bitmap.o build/nt_csq.o build/nt_remove_lock.o build/nt_irp.o build/nt_scsi.o build/nt_dma.o build/nt_interrupt.o
	$(CROSS)ar rcs $@ $^

build/nt_scsi.o: kernel/scsi.c include/wdm_scsi.h | build
	$(CC32) $(DRIVER_CFLAGS) -I$(DDK_INCLUDE) -D_X86_=1 -c $< -o $@

build/nt_dma.o: kernel/dma.c include/wdm_dma.h | build
	$(CC32) $(DRIVER_CFLAGS) -I$(DDK_INCLUDE) -D_X86_=1 -c $< -o $@

build/nt_interrupt.o: kernel/interrupt.c include/wdm_interrupt.h | build
	$(CC32) $(DRIVER_CFLAGS) -I$(DDK_INCLUDE) -D_X86_=1 -c $< -o $@

build/PORTCLS.SYS: kernel/portcls.c kernel/portcls_x86.h kernel/portcls_dispatch.h kernel/portcls.def | build
	$(CC32) $(DDK_CFLAGS) $(NT_DRIVER_LDFLAGS) -Wl,--kill-at -o $@ kernel/portcls.c kernel/portcls.def -lntoskrnl -lhal

build/ks_intersection.o: kernel/ks_intersection.c kernel/nt_legacy.h | build
	$(CC32) $(DDK_CFLAGS) -c $< -o $@

build/KS.SYS: kernel/ks_property.c kernel/ks.def kernel/nt_legacy.h build/ks_memory.o build/ks_intersection.o | build
	$(CC32) $(DDK_CFLAGS) $(NT_DRIVER_LDFLAGS) -Wl,--kill-at -o $@ kernel/ks_property.c kernel/ks.def $(filter %.o,$^)

build/AUDIO/PORTCLS.SYS build/AUDIO/KS.SYS: build/AUDIO/%.SYS: build/%.SYS
	mkdir -p build/AUDIO
	cp $< $@

build/FEAUDIO/QEMUAUD.INF: installer/QEMUAUD.INF | build
	mkdir -p build/FEAUDIO
	awk '{printf "%s\r\n", $$0}' $< > $@

build/libportcls-legacy.a: kernel/portcls.def | build
	$(CROSS)dlltool -k -d $< -l $@

build/libks-legacy.a: kernel/ks.def | build
	$(CROSS)dlltool -k -d $< -l $@

build/libwinwdm-vmm.a: build/bitmap.o build/vxd_csq.res.o build/vxd_remove_lock.res.o build/vxd_irp.res.o
	$(CROSS)ar rcs $@ $^

build:
	mkdir -p $@

build/bitmap.o: kernel/bitmap.c include/wdm_bitmap.h | build
	$(CC32) $(DRIVER_CFLAGS) -c $< -o $@

build/check.o: kernel/check.c include/wdm_bitmap.h | build
	$(CC32) $(VMM_CFLAGS) -DWK_NATIVE_TYPES -c $< -o $@

# MinGW-w64 v12's power stack union uses a second spelling for NTDDI_VISTA.
DDK_CFLAGS = $(DRIVER_CFLAGS) -I$(DDK_INCLUDE) -D_X86_=1 -D_WIN32_WINNT=0x0500 -DNTDDI_VERSION=0x05000000 -DNTDDI_WINVISTA=NTDDI_VISTA
NT_CFLAGS = $(DDK_CFLAGS) -include kernel/nt_legacy.h
build/ntdriver.o: kernel/control.c include/protocol.h kernel/queue.h kernel/pci_device.h kernel/nt_legacy.h | build
	$(CC32) $(NT_CFLAGS) -c $< -o $@

build/memory.o: kernel/memory.c | build
	$(CC32) $(DRIVER_CFLAGS) -c $< -o $@

build/ks_memory.o: kernel/memory.c | build
	$(CC32) $(DRIVER_CFLAGS) -DWK_LOCAL_MEMSET -c $< -o $@

VMM_CFLAGS = $(DDK_CFLAGS) -D_NTOSKRNL_ -D_NTSYSTEM_ -D_NTHALDLL_ -DWK_VMM

build/vxd_io.o: kernel/vxd_io.c include/protocol.h include/wdm_remove_lock.h kernel/queue.h kernel/resident.h kernel/vmm_event.h | build
	$(CC32) $(VMM_CFLAGS) -c $< -o $@

build/vxd_control.o: kernel/control.c include/protocol.h kernel/queue.h kernel/pci_device.h | build
	$(CC32) $(VMM_CFLAGS) -c $< -o $@

build/nt_csq.o build/nt_queue.o: build/nt_%.o: kernel/%.c include/wdm_csq.h kernel/queue.h include/protocol.h kernel/nt_legacy.h | build
	$(CC32) $(NT_CFLAGS) -c $< -o $@

build/nt_worker.o: kernel/nt_worker.c kernel/queue.h kernel/nt_legacy.h | build
	$(CC32) $(NT_CFLAGS) -c $< -o $@

build/nt_queue.o build/vxd_queue.o: include/wdm_remove_lock.h include/wdm_irp.h
build/nt_pci_bus.o build/vxd_pci_bus.o: kernel/queue.h

build/nt_remove_lock.o: kernel/remove_lock.c include/wdm_remove_lock.h kernel/nt_legacy.h | build
	$(CC32) $(NT_CFLAGS) -c $< -o $@

build/vxd_remove_lock.o: kernel/remove_lock.c include/wdm_remove_lock.h kernel/vmm_event.h | build
	$(CC32) $(VMM_CFLAGS) -c $< -o $@

build/nt_irp.o: kernel/irp.c include/wdm_irp.h kernel/nt_legacy.h | build
	$(CC32) $(NT_CFLAGS) -c $< -o $@

build/vxd_irp.o: kernel/irp.c include/wdm_irp.h | build
	$(CC32) $(VMM_CFLAGS) -c $< -o $@

build/vxd_resident.o: kernel/resident.c kernel/resident.h | build
	$(CC32) $(VMM_CFLAGS) -c $< -o $@

build/vxd_queue.o build/vxd_csq.o: build/vxd_%.o: kernel/%.c include/wdm_csq.h kernel/queue.h include/protocol.h | build
	$(CC32) $(VMM_CFLAGS) -c $< -o $@

build/vxd_worker.o: kernel/vxd_worker.c kernel/queue.h | build
	$(CC32) $(VMM_CFLAGS) -c $< -o $@

build/nt_pci_bus.o build/nt_pci_device.o: build/nt_%.o: kernel/%.c kernel/pci_device.h include/wdm_remove_lock.h kernel/nt_legacy.h include/protocol.h | build
	$(CC32) $(NT_CFLAGS) -c $< -o $@

build/vxd_pci_bus.o build/vxd_pci_device.o: build/vxd_%.o: kernel/%.c kernel/pci_device.h include/wdm_remove_lock.h include/protocol.h | build
	$(CC32) $(VMM_CFLAGS) -c $< -o $@

build/vxd.obj: kernel/vxd.asm | build
	$(NASM) -f win32 $< -o $@

# A Windows 3.x VxD keeps the control procedure and its live state together.
# Preserve section-relative COFF fixups while laying out the resident object.
build/%.res.o: build/%.o
	$(CROSS)objcopy --rename-section '.rdata=.text$$const,alloc,load,code,contents' \
	    --rename-section '.bss=.text$$state,alloc,load,code,contents' \
	    --rename-section '.data=.text$$data,alloc,load,code,contents' $< $@

.SECONDEXPANSION:
$(VMM_IMAGES): $$(TARGET_OBJ) build/vxd.obj build/vxd_io.res.o build/vxd_control.res.o build/vxd_queue.res.o build/vxd_csq.res.o build/vxd_remove_lock.res.o build/vxd_irp.res.o build/vxd_resident.res.o build/vxd_worker.res.o build/vxd_pci_device.res.o build/vxd_pci_bus.res.o build/check.o build/bitmap.o build/memory.o kernel/vxd.lnk
	$(JWLINK) @kernel/vxd.lnk name $@ option map=$(basename $@).map file $(TARGET_OBJ)

$(NT_IMAGES): $$(TARGET_OBJ) build/ntdriver.o build/nt_queue.o build/nt_csq.o build/nt_remove_lock.o build/nt_irp.o build/nt_worker.o build/nt_pci_device.o build/nt_pci_bus.o build/check.o build/bitmap.o build/memory.o
	$(CC32) $(NT_DRIVER_LDFLAGS) -o $@ $^ -lntoskrnl -lhal

build/WKS064.SYS: SCSI_DESCRIPTOR_SIZE = 64
build/WKS076.SYS: SCSI_DESCRIPTOR_SIZE = 76
build/WKS064.SYS build/WKS076.SYS: qa/scsi_registration.c include/wdm_scsi.h | build
	$(CC32) $(DRIVER_CFLAGS) -I$(DDK_INCLUDE) -D_X86_=1 -DWK_SCSI_DESCRIPTOR_SIZE=$(SCSI_DESCRIPTOR_SIZE) $(NT_DRIVER_LDFLAGS) -o $@ $< -lscsiport

build/WKEDUDMA.SYS: qa/edu_dma.c include/wdm_dma.h build/libwinwdm.a | build
	$(CC32) $(DRIVER_CFLAGS) -I$(DDK_INCLUDE) -D_X86_=1 $(NT_DRIVER_LDFLAGS) -o $@ $< build/libwinwdm.a -lntoskrnl -lhal

build/WKEDUIRQ.SYS: qa/edu_dma.c qa/edu_interrupt.c include/wdm_interrupt.h include/wdm_dma.h build/libwinwdm.a | build
	$(CC32) $(DRIVER_CFLAGS) -I$(DDK_INCLUDE) -D_X86_=1 -DWK_EDU_INTERRUPT $(NT_DRIVER_LDFLAGS) -o $@ $(filter %.c,$^) build/libwinwdm.a -lntoskrnl -lhal

build/WKPCINT.SYS: qa/edu_dma.c qa/edu_interrupt.c qa/portcls_interrupt.c kernel/portcls_x86.h include/wdm_interrupt.h include/wdm_dma.h build/libwinwdm.a build/libportcls-legacy.a | build
	$(CC32) $(DDK_CFLAGS) -DWK_EDU_INTERRUPT -DWK_EDU_PORTCLS $(NT_DRIVER_LDFLAGS) -o $@ $(filter %.c,$^) build/libwinwdm.a build/libportcls-legacy.a -lntoskrnl -lhal

build/ks_intersection_test.o: qa/ks_intersection_test.c | build
	$(CC32) $(NT_CFLAGS) -c $< -o $@

build/WKKS.SYS: qa/ks_property_test.c build/ks_intersection_test.o build/libks-legacy.a build/ks_memory.o | build
	$(CC32) $(NT_CFLAGS) $(NT_DRIVER_LDFLAGS) -o $@ $^ -lntoskrnl -lhal

build/WKSPCI.SYS: SCSI_PROBE_FLAGS = -DWK_SCSI_PCI
build/WKSCTRL.SYS: SCSI_PROBE_FLAGS = -DWK_SCSI_CONTROL
build/WKSADPT.SYS build/WKSPCI.SYS build/WKSCTRL.SYS: qa/scsi_registration.c include/wdm_scsi.h build/libwinwdm.a | build
	$(CC32) $(DRIVER_CFLAGS) -I$(DDK_INCLUDE) -D_X86_=1 -DWK_SCSI_DESCRIPTOR_SIZE=80 -DWK_SCSI_ADAPT $(SCSI_PROBE_FLAGS) $(NT_DRIVER_LDFLAGS) -o $@ $< build/libwinwdm.a -lscsiport

.PHONY: package
package: kernel sdk build/AUDIO/PORTCLS.SYS build/AUDIO/KS.SYS build/FEAUDIO/QEMUAUD.INF
	python3 installer/build_gui.py
	mkdir -p build/DESKTOP
	makensis -V2 installer/desktop.nsi

check: build/bitmap_test build/csq_test build/remove_test build/vmm_remove_test build/resident_test build/dma_test build/portcls_dispatch_test
	./build/bitmap_test
	./build/csq_test
	./build/remove_test
	./build/vmm_remove_test
	./build/resident_test
	./build/dma_test
	./build/portcls_dispatch_test

build/portcls_dispatch_test: qa/portcls_dispatch_test.c kernel/portcls_dispatch.h | build
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 $< -o $@

build/dma_test: qa/dma_test.c kernel/dma.c include/wdm_dma.h qa/dma_host/ntddk.h | build
	$(CC) -Iqa/dma_host -Iinclude -std=c11 -Wall -Wextra -Werror -O2 $(filter %.c,$^) -o $@

build/vmm_remove_test: REMOVE_TARGET = -DWK_VMM
build/remove_test build/vmm_remove_test: qa/remove_lock_test.c kernel/remove_lock.c kernel/vmm_event.h include/wdm_remove_lock.h qa/remove_host/wdm.h | build
	$(CC) $(REMOVE_TARGET) -Iqa/remove_host -Iinclude -std=c11 -Wall -Wextra -Werror -O2 -pthread $(filter %.c,$^) -o $@

build/resident_test: qa/resident_test.c kernel/resident.c kernel/target.c include/wdm_target.h kernel/resident.h qa/resident_host/wdm.h | build
	$(CC) -Iqa/resident_host -Ikernel -Iinclude -std=c11 -Wall -Wextra -Werror -O2 -pthread $(filter %.c,$^) -o $@

build/csq_test: qa/csq_test.c kernel/csq.c include/wdm_csq.h qa/csq_host/wdm.h | build
	$(CC) -Iqa/csq_host -Iinclude -std=c11 -Wall -Wextra -Werror -O2 -pthread $(filter %.c,$^) -o $@

build/bitmap_test: qa/bitmap_test.c kernel/bitmap.c kernel/check.c include/wdm_bitmap.h | build
	$(CC) -Iinclude -std=c99 -Wall -Wextra -Werror -O2 $(filter %.c,$^) -o $@

.PHONY: media
media: package
	sh installer/mkmedia.sh

build/KITCHK.EXE: qa/guest_check.c include/protocol.h include/wdm_target.h | build
	INCLUDE="$(WATCOM)/h/nt:$(WATCOM)/h" wcc386 -zq -bt=nt -3r -i=include -fo=build/guest_check.obj $<
	wlink system nt_win option quiet runtime windows=3.10 name $@ file build/guest_check.obj

build/UNTEST.EXE: qa/desktop_remove.c | build
	INCLUDE="$(WATCOM)/h/nt:$(WATCOM)/h" wcc386 -zq -bt=nt -3r -fo=build/desktop_remove.obj $<
	wlink system nt_win option quiet runtime windows=3.10 name $@ file build/desktop_remove.obj

build/ntdriver.o build/vxd_control.o build/vxd_resident.o build/nt_queue.o build/vxd_queue.o: include/wdm_target.h

build/PMCHECK.EXE: qa/program_manager_check.c include/wdm_target.h | build
	INCLUDE="$(WATCOM)/h/nt:$(WATCOM)/h" wcc386 -zq -bt=nt -3r -i=include -fo=build/program_manager_check.obj $<
	wlink system nt_win option quiet runtime windows=3.10 name $@ file build/program_manager_check.obj

build/HDALOAD.EXE: qa/wdmhda_load.c | build
	INCLUDE="$(WATCOM)/h/nt:$(WATCOM)/h" wcc386 -zq -bt=nt -3r -fo=build/wdmhda_load.obj $<
	wlink system nt_win option quiet runtime windows=3.10 name $@ file build/wdmhda_load.obj

build/W9DEVS.EXE: qa/win9x_devices.c | build
	INCLUDE="$(WATCOM)/h/nt:$(WATCOM)/h" wcc386 -zq -bt=nt -3r -fo=build/win9x_devices.obj $<
	wlink system nt_win option quiet runtime windows=3.10 name $@ file build/win9x_devices.obj

build/ICHWAVE.EXE: qa/ich_wave.c | build
	INCLUDE="$(WATCOM)/h/nt:$(WATCOM)/h" wcc386 -zq -bt=nt -3r -fo=build/ich_wave.obj $<
	wlink system nt_win option quiet runtime windows=3.10 name $@ file build/ich_wave.obj library winmm

build/PCTEST.EXE: qa/portcls_service.c | build
	INCLUDE="$(WATCOM)/h/nt:$(WATCOM)/h" wcc386 -zq -bt=nt -3r -fo=build/portcls_service.obj $<
	wlink system nt_win option quiet runtime windows=3.10 name $@ file build/portcls_service.obj

build/NTCHECK.EXE: qa/native_controls.c | build
	INCLUDE="$(WATCOM)/h/nt:$(WATCOM)/h" wcc386 -zq -bt=nt -3r -fo=build/native_controls.obj $<
	wlink system nt_win option quiet runtime windows=3.10 name $@ file build/native_controls.obj

build/W16CHECK.EXE: qa/win16_target_check.c gui/probe.c gui/target.c gui/kit.h include/protocol.h include/wdm_target.h | build
	mkdir -p build/w16check
	cd build/w16check && for source in $(addprefix ../../,$(filter %.c,$^)); do \
	    stem=$${source##*/}; \
	    INCLUDE="$(WATCOM)/h/win:$(WATCOM)/h" wcc -zq -bt=windows -ml -zw -2 -i=../../include -i=../../gui -fo="$${stem%.c}.obj" "$$source" || exit 1; \
	done
	cd build/w16check && wlink system windows option quiet,stack=16000,version=3.0 name ../W16CHECK.EXE file win16_target_check.obj,probe.obj,target.obj
