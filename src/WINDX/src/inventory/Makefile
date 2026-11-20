ARCH ?= x86
ifeq ($(ARCH),x86)
CC := i686-w64-mingw32-gcc
FLAGS := -march=i486 -mcrtdll=crtdll -Wl,--entry,_inventory_entry@0,--major-os-version,3,--minor-os-version,10,--subsystem,console:3.10
else ifeq ($(ARCH),x64)
CC := x86_64-w64-mingw32-gcc
FLAGS := -Wl,--entry,inventory_entry,--major-os-version,5,--minor-os-version,2,--subsystem,console:5.2
else
$(error ARCH must be x86 or x64)
endif
OUT ?= ../../bin/$(ARCH)
BUILD ?= ../../build/inventory-$(ARCH)

$(OUT)/wxinv.exe: install.c runtime.h device.h display.h $(BUILD)/device8.o $(BUILD)/device9.o
	mkdir -p $(OUT)
	$(CC) -Os -ffreestanding -fno-builtin -nostdlib -std=gnu11 -s $(FLAGS) -Wl,--no-insert-timestamp $< $(BUILD)/device8.o $(BUILD)/device9.o -ladvapi32 -lgdi32 -luser32 -lkernel32 -lgcc -o $@

$(BUILD)/device8.o: legacy_device.c
	mkdir -p $(BUILD)
	$(CC) -Os -ffreestanding -fno-builtin -std=gnu11 $(if $(filter x86,$(ARCH)),-march=i486) -DWX_PROBE_D3D8 -c $< -o $@

$(BUILD)/device9.o: legacy_device.c
	mkdir -p $(BUILD)
	$(CC) -Os -ffreestanding -fno-builtin -std=gnu11 $(if $(filter x86,$(ARCH)),-march=i486) -c $< -o $@
