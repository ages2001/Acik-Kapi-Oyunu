.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
ARCH ?= x86
BUILD ?= ../build
OUT := ../bin/$(ARCH)
VERSION := 2026.05.29
LOADER := $(BUILD)/OpenCL-ICD-Loader-$(VERSION)
HEADERS := $(BUILD)/OpenCL-Headers-$(VERSION)
ifeq ($(ARCH),x86)
TRIPLE := i686-w64-mingw32
CPU := -march=i486 -mcrtdll=crtdll
PLATFORM := -Wl,--major-os-version,3,--minor-os-version,10
SUBSYSTEM := 3.10
DLL_ENTRY := _winocl_entry@12
TEST_ENTRY := _test_entry@0
DEF := OpenCL-mingw-i686.def
else ifeq ($(ARCH),x64)
TRIPLE := x86_64-w64-mingw32
PLATFORM := -Wl,--major-os-version,5,--minor-os-version,2
SUBSYSTEM := 5.2
DLL_ENTRY := winocl_entry
TEST_ENTRY := test_entry
DEF := OpenCL.def
else
$(error ARCH must be x86 or x64)
endif
CC := $(TRIPLE)-gcc
FLAGS := $(CPU) -Os -std=gnu11 -I$(HEADERS) -I$(LOADER)/include -I$(LOADER)/loader -I. -DCL_TARGET_OPENCL_VERSION=310
LOADER_FLAGS := -DCL_NO_NON_ICD_DISPATCH_EXTENSION_PROTOTYPES -DCL_SHARED_BUILD -DCL_ENABLE_LOADER_MANAGED_DISPATCH \
                -DOPENCL_ICD_LOADER_VERSION_MAJOR=3 -DOPENCL_ICD_LOADER_VERSION_MINOR=1 -DOPENCL_ICD_LOADER_VERSION_REV=0
CORE := icd.c icd_dispatch.c icd_dispatch_generated.c icd_trace.c windows/icd_windows_envvars.c windows/icd_windows_library.c
.PHONY: all
all: $(OUT)/OpenCL.dll $(OUT)/ocltest.exe
$(LOADER)/.ready: vendor/OpenCL-ICD-Loader-v$(VERSION).tar.gz legacy-trace.patch
	printf '%s  %s\n' '48fd0c5181db7cd046f4f731d5955694892e10998d49d09ee0d997e7e04fd939' '$<' | sha256sum --check
	mkdir -p $(BUILD)
	tar -xf $< -C $(BUILD)
	patch --batch -d $(LOADER) -p1 < legacy-trace.patch
	touch $@
$(HEADERS)/.ready: vendor/OpenCL-Headers-v$(VERSION).tar.gz
	printf '%s  %s\n' 'd9e6c48357de5002da11ce45de600e0c3ffe6ab4f628a3b9fe2b38603161658a' '$<' | sha256sum --check
	mkdir -p $(BUILD)
	tar -xf $< -C $(BUILD)
	touch $@
$(OUT):
	mkdir -p $@
$(OUT)/OpenCL.dll: $(LOADER)/.ready $(HEADERS)/.ready legacy_windows.c | $(OUT)
	$(CC) $(FLAGS) $(LOADER_FLAGS) $(PLATFORM) -shared -nostartfiles -static-libgcc -Wl,--disable-auto-import,--disable-runtime-pseudo-reloc,--entry,$(DLL_ENTRY),--subsystem,windows:$(SUBSYSTEM),--no-insert-timestamp -s \
	    $(addprefix $(LOADER)/loader/,$(CORE)) legacy_windows.c $(LOADER)/loader/windows/$(DEF) -ladvapi32 -o $@
$(OUT)/ocltest.exe: test_compute.c $(HEADERS)/.ready | $(OUT)
	$(CC) $(FLAGS) $(PLATFORM) -fno-builtin -nostdlib -Wl,--entry,$(TEST_ENTRY),--subsystem,console:$(SUBSYSTEM),--no-insert-timestamp -s test_compute.c -lkernel32 -o $@

include cpu.mk
