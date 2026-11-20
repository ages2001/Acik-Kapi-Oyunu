# Cross-build the WINDX compiler and guest test from the bundled upstream release.
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

ARCH ?= x86
BUILD ?= ../build
BIN ?= ../bin
HEADER_ROOT ?= /usr/include
VERSION := 1.17
SOURCE_HASH := bc61cb9e84d5045cbcaffbdd707940d399d8bf62874663dfe5809a0bfb87e9b6
UPSTREAM := $(BUILD)/vkd3d-$(VERSION)
PREPARED := $(UPSTREAM)/.prepared
GEN := $(BUILD)/parsers
OBJ := $(BUILD)/$(ARCH)
OUT := $(BIN)/$(ARCH)

ifeq ($(ARCH),x86)
TRIPLE := i686-w64-mingw32
ARCH_FLAGS := -march=i486 -mcrtdll=crtdll
PLATFORM_FLAGS := -Wl,--major-os-version,3,--minor-os-version,10,--subsystem,windows:3.10
TEST_PLATFORM := -Wl,--major-os-version,3,--minor-os-version,10,--subsystem,console:3.10
DLL_ENTRY := _windx_dll_main@12
TEST_ENTRY := _test_entry@0
RENDER_ENTRY := _render_entry@0
EXTRA_OBJS := $(OBJ)/nt31_codepage.o
# Distribution CRT archives can contain CMOV instructions even when the DLL
# sources are compiled for i486. Build the two linked archives for that CPU.
CRT_SRC := $(BUILD)/mingw-w64-12.0.0
CRT_BUILD := $(BUILD)/crt-i486
CRT_LIBS := $(CRT_BUILD)/lib32/libmingwex.a $(CRT_BUILD)/lib32/libcrtdll.a
CRT_SEARCH := -L$(CRT_BUILD)/lib32
else ifeq ($(ARCH),x64)
TRIPLE := x86_64-w64-mingw32
PLATFORM_FLAGS := -Wl,--major-os-version,5,--minor-os-version,2,--subsystem,windows:5.2
TEST_PLATFORM := -Wl,--major-os-version,5,--minor-os-version,2,--subsystem,console:5.2
DLL_ENTRY := windx_dll_main
TEST_ENTRY := test_entry
RENDER_ENTRY := render_entry
else
$(error ARCH must be x86 or x64)
endif
CC := $(TRIPLE)-gcc
CXX := $(TRIPLE)-g++
STRIP := $(TRIPLE)-strip

SHADER_FILES := checksum d3d_asm d3dbc dxbc dxil fx glsl hlsl hlsl_codegen \
                hlsl_constant_ops ir msl spirv tpf vkd3d_shader_main
COMMON_FILES := blob debug error memory utf8
GENERATED := hlsl.tab hlsl.yy preproc.tab preproc.yy
OBJECTS := $(addprefix $(OBJ)/,$(addsuffix .o,$(SHADER_FILES) $(COMMON_FILES) \
           $(GENERATED) vkd3d_utils_main dll_entry)) $(EXTRA_OBJS)
INCLUDES := -I. -I$(GEN) -I$(UPSTREAM)/include -I$(UPSTREAM)/include/private \
            -I$(UPSTREAM)/libs/vkd3d-shader -idirafter $(HEADER_ROOT)
# Compile-time declarations are shared with upstream. The linked imports are
# checked against the target system DLLs and exercised in real guest sessions.
COMPILE = $(CC) $(ARCH_FLAGS) -O2 -std=gnu11 -ffunction-sections -fdata-sections \
          -D_WIN32_WINNT=0x0600 -DWINDX_SHADER_DLL $(INCLUDES) -MMD -MP

.PHONY: all
all: $(OUT)/d3dcompiler_43.dll $(OUT)/wxtest.exe $(OUT)/wxrender.exe $(OUT)/wxearly.exe $(OUT)/wx12test.exe $(OUT)/wxmedia.exe $(OUT)/wxdiag.exe $(OUT)/early/wxddraw.exe $(OUT)/early/ddraw.dll

ifeq ($(ARCH),x86)
$(CRT_SRC)/.prepared: vendor/mingw-w64_12.0.0.orig.tar.bz2
	printf '%s  %s\n' 'cc41898aac4b6e8dd5cffd7331b9d9515b912df4420a3a612b5ea2955bbeed2f' '$<' | sha256sum --check
	mkdir -p $(CRT_SRC)
	tar -xf $< --strip-components=1 -C $(CRT_SRC)
	touch $@

$(CRT_BUILD)/Makefile: $(CRT_SRC)/.prepared
	mkdir -p $(CRT_BUILD)
	cd $(CRT_BUILD) && CC="$(CC)" CFLAGS="-Os -march=i486" \
	    "$(abspath $(CRT_SRC))/mingw-w64-crt/configure" --host=$(TRIPLE) \
	    --enable-lib32 --disable-lib64 --with-default-msvcrt=msvcrt

$(CRT_LIBS) &: $(CRT_BUILD)/Makefile
	$(MAKE) -C $(CRT_BUILD) lib32/libmingwex.a lib32/libcrtdll.a
endif

$(PREPARED): vendor/vkd3d-$(VERSION).tar.xz vkd3d-$(VERSION).patch
	printf '%s  %s\n' '$(SOURCE_HASH)' '$<' | sha256sum --check
	mkdir -p $(UPSTREAM)
	tar -xf $< -C $(UPSTREAM) --strip-components=1
	patch -d $(UPSTREAM) -p1 --batch < vkd3d-$(VERSION).patch
	touch $@

$(GEN) $(OBJ) $(OUT):
	mkdir -p $@

$(GEN)/%.tab.c $(GEN)/%.tab.h &: $(PREPARED) | $(GEN)
	bison -d -o $(GEN)/$*.tab.c $(UPSTREAM)/libs/vkd3d-shader/$*.y

$(GEN)/%.yy.c: $(PREPARED) | $(GEN)
	flex -o $@ $(UPSTREAM)/libs/vkd3d-shader/$*.l

$(addprefix $(OBJ)/,$(addsuffix .o,$(GENERATED))): $(OBJ)/%.o: $(GEN)/%.c $(GEN)/hlsl.tab.h $(GEN)/preproc.tab.h | $(OBJ)
	$(COMPILE) -c $< -o $@

$(addprefix $(OBJ)/,$(addsuffix .o,$(SHADER_FILES))): $(OBJ)/%.o: $(PREPARED) $(GEN)/hlsl.tab.h $(GEN)/preproc.tab.h | $(OBJ)
	$(COMPILE) -c $(UPSTREAM)/libs/vkd3d-shader/$*.c -o $@

$(addprefix $(OBJ)/,$(addsuffix .o,$(COMMON_FILES))): $(OBJ)/%.o: $(PREPARED) | $(OBJ)
	$(COMPILE) -c $(UPSTREAM)/libs/vkd3d-common/$*.c -o $@

$(OBJ)/vkd3d_utils_main.o: $(PREPARED) | $(OBJ)
	$(COMPILE) -c $(UPSTREAM)/libs/vkd3d-utils/vkd3d_utils_main.c -o $@

$(OBJ)/%.o: %.c | $(OBJ)
	$(COMPILE) -c $< -o $@

$(OUT)/d3dcompiler_43.dll: $(OBJECTS) compiler.def $(CRT_LIBS) | $(OUT)
	$(CC) $(ARCH_FLAGS) $(CRT_SEARCH) -shared -nostartfiles -static-libgcc $(PLATFORM_FLAGS) \
	    -Wl,--entry,$(DLL_ENTRY),--gc-sections,--kill-at,--enable-stdcall-fixup \
	    -Wl,--disable-auto-import,--disable-runtime-pseudo-reloc,--no-insert-timestamp \
	    $(OBJECTS) compiler.def -luuid -o $@
	$(STRIP) --strip-unneeded $@

$(OUT)/wxtest.exe: test_compiler.c test_output.h | $(OUT)
	$(CC) $(ARCH_FLAGS) -Os -fno-builtin -nostdlib $(TEST_PLATFORM) \
	    -Wl,--entry,$(TEST_ENTRY),--no-insert-timestamp -s $< -lkernel32 -o $@

$(OUT)/wxrender.exe: test_render.c test_d3d10_sdk.c test_output.h | $(OUT)
	$(CC) $(ARCH_FLAGS) -Os -fno-builtin -nostdlib $(TEST_PLATFORM) \
	    -Wl,--entry,$(RENDER_ENTRY),--no-insert-timestamp -s test_render.c test_d3d10_sdk.c -lgdi32 -luser32 -lkernel32 -o $@

-include $(OBJECTS:.o=.d)
include mesa.mk
include early3d.mk
include wine.mk
include compute.mk
include early8.mk

$(OUT)/wxdiag.exe: wxdiag.c | $(OUT)
	$(CC) $(ARCH_FLAGS) -Os -fno-builtin -nostdlib $(PLATFORM_FLAGS) \
	    -Wl,--entry,$(if $(filter x86,$(ARCH)),_diag_entry@0,diag_entry),--no-insert-timestamp \
	    -s $< -lgdi32 -luser32 -lkernel32 -o $@

$(OUT)/wxearly.exe: test_early.c test_d3d8.c test_output.h | $(OUT)
	$(CC) $(ARCH_FLAGS) -Os -fno-builtin -nostdlib $(TEST_PLATFORM) \
	    -Wl,--entry,$(if $(filter x86,$(ARCH)),_early_entry@0,early_entry),--no-insert-timestamp \
	    -s test_early.c test_d3d8.c -luser32 -lkernel32 -o $@

$(OUT)/wx12test.exe: test_d3d12.c test_output.h | $(OUT)
	$(CC) $(ARCH_FLAGS) -Os -fno-builtin -nostdlib $(TEST_PLATFORM) \
	    -Wl,--entry,$(if $(filter x86,$(ARCH)),_d3d12_entry@0,d3d12_entry),--no-insert-timestamp \
	    -s $< -lkernel32 -o $@
include llvm.mk
include d3d12.mk
include redist.mk

$(OUT)/early:
	mkdir -p $@

include early/directdraw.mk

$(OUT)/early/wxddraw.exe: test_ddraw_gdi.c test_output.h | $(OUT)/early
	$(CC) $(ARCH_FLAGS) -Os -fno-builtin -nostdlib $(TEST_PLATFORM) \
	    -Wl,--entry,$(if $(filter x86,$(ARCH)),_directdraw_entry@0,directdraw_entry),--no-insert-timestamp \
	    -s $< -lgdi32 -luser32 -lkernel32 -o $@

$(OUT)/wxmedia.exe: test_media.c test_output.h identity/wx_identity.h | $(OUT)
	$(CC) $(ARCH_FLAGS) -Os -fno-builtin -nostdlib $(TEST_PLATFORM) \
	    -Wl,--entry,$(if $(filter x86,$(ARCH)),_media_entry@0,media_entry),--no-insert-timestamp \
	    -s $< -lole32 -loleaut32 -luuid -lkernel32 -o $@

ifeq ($(ARCH),x86)
.PHONY: classic win95
classic: all softpipe d3d $(CRT_LIBS)
	python3 classic/assemble.py $(OUT) $(CRT_BUILD)/lib32/libcrtdll.a --cc "$(CC)"
	python3 classic/assemble.py $(OUT) $(CRT_BUILD)/lib32/libcrtdll.a --cc "$(CC)" --short-names
win95: all softpipe d3d $(CRT_LIBS)
	python3 classic/relink_win95.py $(OBJ)/win95-links --cc "$(CC)" --mesa $(MESA_BUILD) \
	    --pthread $(PTHREAD_BUILD) --wine $(WINE_BUILD)
	python3 classic/assemble.py $(OUT) $(CRT_BUILD)/lib32/libcrtdll.a --cc "$(CC)" --win95-links $(OBJ)/win95-links
endif
include media.mk
