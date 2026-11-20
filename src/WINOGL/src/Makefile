.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
ARCH ?= x86
BUILD ?= ../build
OUT := ../bin/$(ARCH)
ifeq ($(ARCH),x86)
TRIPLE := i686-w64-mingw32
ARCH_FLAGS := -march=i486 -mcrtdll=crtdll
TEST_PLATFORM := -Wl,--major-os-version,3,--minor-os-version,10,--subsystem,console:3.10
ENTRY := _wgl_test_entry@0
else ifeq ($(ARCH),x64)
TRIPLE := x86_64-w64-mingw32
TEST_PLATFORM := -Wl,--major-os-version,5,--minor-os-version,2,--subsystem,console:5.2
ENTRY := wgl_test_entry
else
$(error ARCH must be x86 or x64)
endif
CC := $(TRIPLE)-gcc
CXX := $(TRIPLE)-g++
STRIP := $(TRIPLE)-strip
.PHONY: all
all: softpipe $(OUT)/wogltest.exe
$(OUT)/wogltest.exe: test_core46.c test_output.h test_spirv.h
	mkdir -p $(OUT)
	$(CC) $(ARCH_FLAGS) -Os -fno-builtin -nostdlib $(TEST_PLATFORM) \
	    -Wl,--entry,$(ENTRY),--no-insert-timestamp -s $< -luser32 -lkernel32 -o $@
include mesa.mk
include llvm.mk

ifeq ($(ARCH),x86)
CRT_ARCHIVE ?= $(shell $(CC) -print-file-name=libcrtdll.a)
.PHONY: classic win95
classic: all
	python3 classic/assemble.py $(OUT) "$(CRT_ARCHIVE)" --cc "$(CC)" --opengl
	python3 classic/assemble.py $(OUT) "$(CRT_ARCHIVE)" --cc "$(CC)" --opengl --short-names
win95: all
	python3 classic/relink_win95.py $(BUILD)/win95-links --cc "$(CC)" --mesa $(MESA_BUILD) --pthread $(PTHREAD_BUILD)
	python3 classic/assemble.py $(OUT) "$(CRT_ARCHIVE)" --cc "$(CC)" --opengl --win95-links $(BUILD)/win95-links
endif
