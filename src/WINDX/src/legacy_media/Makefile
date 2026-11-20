# Build the native WinMM and Win32 input backends with WINDX Wine headers.
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

ARCH ?= x86
BUILD ?= ../../build
BIN ?= ../../bin
SOURCE := $(BUILD)/winmm-wine-1.2.3
OBJECTS := $(BUILD)/winmm-$(ARCH)
OUTPUT := $(BIN)/$(ARCH)/legacy_media
WINE_SOURCE ?= $(BUILD)/wine-2.0.5-$(ARCH)-source
WINE_BUILD ?= $(BUILD)/wine-2.0.5-$(ARCH)
ARCHIVE := ../vendor/wine-1.2.3.tar.bz2
WINE_FILES := buffer capture dsound dsound_convert dsound_main duplex mixer primary propset regsvr sound3d
INPUT_ARCHIVE := ../vendor/wine-2.0.5.tar.xz
INPUT_SOURCE := $(BUILD)/wininput-wine-2.0.5
INPUT_MODULE := $(INPUT_SOURCE)/dlls/dinput
INPUT_OBJECTS := $(BUILD)/wininput-$(ARCH)
INPUT_FILES := config data_formats device effect_linuxinput joystick joystick_linux joystick_linuxinput joystick_osx keyboard mouse
INPUT_OBJS := $(addprefix $(INPUT_OBJECTS)/,$(addsuffix .o,$(INPUT_FILES) input_support))
SRC_MODULE := $(SOURCE)/dlls/dsound
OBJS := $(addprefix $(OBJECTS)/,$(addsuffix .o,$(WINE_FILES) native_wave native_lock native_guid))

ifeq ($(ARCH),x86)
TRIPLE := i686-w64-mingw32
CPU := -march=i486 -mcrtdll=crtdll
CRT := -L$(BUILD)/crt-i486/lib32
VERSION := 3.10
OS_VERSION := -Wl,--major-os-version,3,--minor-os-version,10
DLL_ENTRY := _DllMain@12
TEST_ENTRY := _sound_entry@0
DEFINITION := dsound.def
INPUT_DEFINITION := dinput.def
INPUT8_DEFINITION := dinput8.def
INPUT_ENTRY := _input_entry@0
INPUT_TEST_CRT := -lcrtdll
else ifeq ($(ARCH),x64)
TRIPLE := x86_64-w64-mingw32
VERSION := 5.2
OS_VERSION := -Wl,--major-os-version,5,--minor-os-version,2
DLL_ENTRY := DllMain
TEST_ENTRY := sound_entry
DEFINITION := $(OBJECTS)/dsound64.def
INPUT_DEFINITION := $(INPUT_OBJECTS)/dinput64.def
INPUT8_DEFINITION := $(INPUT_OBJECTS)/dinput864.def
INPUT_ENTRY := input_entry
INPUT_TEST_CRT := -lmsvcrt
else
$(error ARCH must be x86 or x64)
endif

CC := $(TRIPLE)-gcc
FLAGS := $(CPU) -Os -std=gnu11 -fno-strict-aliasing -ffunction-sections -fdata-sections \
    -D__WINESRC__ -D_REENTRANT -DNDEBUG -DWAVE_DIRECTSOUND=0x0080 \
    -Iinclude -I. -I$(SOURCE)/include -I$(WINE_BUILD)/include -I$(WINE_SOURCE)/include

INPUT_FLAGS := $(CPU) -Os -std=gnu11 -fno-strict-aliasing -ffunction-sections -fdata-sections \
    -D__WINESRC__ -D_REENTRANT -DNDEBUG -Iinclude -I. -I$(INPUT_MODULE) \
    -I$(WINE_BUILD)/include -I$(WINE_SOURCE)/include
DLL_FLAGS := $(CPU) $(CRT) -shared -nostartfiles -static-libgcc -s $(OS_VERSION) \
    -Wl,--gc-sections,--entry,$(DLL_ENTRY),--subsystem,windows:$(VERSION),--no-insert-timestamp
RC := $(TRIPLE)-windres

.PHONY: all
all: $(addprefix $(OUTPUT)/,dsound.dll dinput.dll dinput8.dll wxsound.exe wxinput.exe)

$(SOURCE)/.prepared: $(ARCHIVE) wine-1.2.3.patch
	printf '%s  %s\n' '3fd8d3f2b466d07eb90b8198cdc9ec3005917a4533db7b8c6c69058a2e57c61f' $< | sha256sum --check
	mkdir -p $(SOURCE)
	tar -xf $< --strip-components=1 -C $(SOURCE) wine-1.2.3/dlls/dsound \
	    wine-1.2.3/include/dsdriver.h wine-1.2.3/include/wine/unicode.h wine-1.2.3/include/wine/list.h
	patch -d $(SOURCE) -p1 --batch < wine-1.2.3.patch
	touch $@

$(INPUT_SOURCE)/.prepared: $(INPUT_ARCHIVE) wine-2.0.5-input.patch
	printf '%s  %s\n' '2885bcf8324e3e7e354c0ab01e5020c82c9c9676d2255f7bfd52b3739a00e5f3' $< | sha256sum --check
	mkdir -p $(INPUT_SOURCE)
	tar -xf $< --strip-components=1 -C $(INPUT_SOURCE) wine-2.0.5/dlls/dinput
	patch -d $(INPUT_SOURCE) -p1 --batch < wine-2.0.5-input.patch
	touch $@

$(OBJECTS) $(INPUT_OBJECTS) $(OUTPUT):
	mkdir -p $@

$(addprefix $(OBJECTS)/,$(addsuffix .o,$(WINE_FILES))): $(OBJECTS)/%.o: $(SOURCE)/.prepared native_wave.h native_lock.h include/wine/debug.h Makefile | $(OBJECTS)
	$(CC) $(FLAGS) -MMD -MP -c $(SRC_MODULE)/$*.c -o $@

$(OBJECTS)/native_wave.o $(OBJECTS)/native_lock.o $(OBJECTS)/native_guid.o: $(OBJECTS)/%.o: %.c native_wave.h native_lock.h $(SOURCE)/.prepared Makefile | $(OBJECTS)
	$(CC) $(FLAGS) -MMD -MP -c $< -o $@

$(OBJECTS)/dsound64.def: dsound.def | $(OBJECTS)
	sed -E 's/=[^ ]+//' $< > $@

$(INPUT_OBJECTS)/dinput64.def: dinput.def | $(INPUT_OBJECTS)
	sed -E 's/=[^ ]+//' $< > $@

$(INPUT_OBJECTS)/dinput864.def: dinput8.def | $(INPUT_OBJECTS)
	sed -E 's/=[^ ]+//' $< > $@

$(addprefix $(INPUT_OBJECTS)/,$(addsuffix .o,$(INPUT_FILES) dinput_main)): $(INPUT_OBJECTS)/%.o: $(INPUT_SOURCE)/.prepared keyboard_sample.inc mouse_sample.inc include/wine/debug.h Makefile | $(INPUT_OBJECTS)
	$(CC) $(INPUT_FLAGS) -MMD -MP -c $(INPUT_MODULE)/$*.c -o $@

$(INPUT_OBJECTS)/dinput8_main.o: $(INPUT_SOURCE)/.prepared input8.inc include/wine/debug.h Makefile | $(INPUT_OBJECTS)
	$(CC) $(INPUT_FLAGS) -DWX_DINPUT8 -MMD -MP -c $(INPUT_MODULE)/dinput_main.c -o $@

$(INPUT_OBJECTS)/input_support.o: input_support.c $(INPUT_SOURCE)/.prepared Makefile | $(INPUT_OBJECTS)
	$(CC) $(INPUT_FLAGS) -MMD -MP -c $< -o $@

$(INPUT_OBJECTS)/config_res.o: $(INPUT_SOURCE)/.prepared | $(INPUT_OBJECTS)
	$(RC) -I$(INPUT_MODULE) -i $(INPUT_MODULE)/dinput.rc -o $@

$(OBJECTS)/sound_version.o: media_version.rc | $(OBJECTS)
	$(RC) -DMEDIA_DESCRIPTION='"WINDX DirectSound for Windows Multimedia"' -DMEDIA_FILENAME='"dsound.dll"' -i $< -o $@

$(INPUT_OBJECTS)/input_version.o: media_version.rc | $(INPUT_OBJECTS)
	$(RC) -DMEDIA_DESCRIPTION='"WINDX DirectInput"' -DMEDIA_FILENAME='"dinput.dll"' -i $< -o $@

$(INPUT_OBJECTS)/input8_version.o: media_version.rc | $(INPUT_OBJECTS)
	$(RC) -DMEDIA_DESCRIPTION='"WINDX DirectInput 8"' -DMEDIA_FILENAME='"dinput8.dll"' -i $< -o $@

$(OUTPUT)/dsound.dll: $(OBJS) $(DEFINITION) $(OBJECTS)/sound_version.o | $(OUTPUT)
	$(CC) $(DLL_FLAGS) $^ -ldxguid -luuid -lwinmm -ladvapi32 -luser32 -lkernel32 -o $@

INPUT_SHARED := $(INPUT_OBJS) $(OBJECTS)/native_guid.o $(INPUT_OBJECTS)/config_res.o
INPUT_LIBS := -luuid -lcomctl32 -ladvapi32 -luser32 -lkernel32

$(OUTPUT)/dinput.dll: $(INPUT_SHARED) $(INPUT_OBJECTS)/dinput_main.o $(INPUT_DEFINITION) $(INPUT_OBJECTS)/input_version.o | $(OUTPUT)
	$(CC) $(DLL_FLAGS) $^ $(INPUT_LIBS) -o $@

$(OUTPUT)/dinput8.dll: $(INPUT_SHARED) $(INPUT_OBJECTS)/dinput8_main.o $(INPUT8_DEFINITION) $(INPUT_OBJECTS)/input8_version.o | $(OUTPUT)
	$(CC) $(DLL_FLAGS) $^ $(INPUT_LIBS) -o $@

$(OUTPUT)/wxinput.exe: test_input.c ../test_output.h Makefile | $(OUTPUT)
	$(CC) $(CPU) $(CRT) -Os -nostdlib -fno-builtin -s $(OS_VERSION) \
	    -Wl,--entry,$(INPUT_ENTRY),--subsystem,console:$(VERSION),--no-insert-timestamp \
	    $< -ldinput -ldxguid -luser32 -lkernel32 $(INPUT_TEST_CRT) -o $@

$(OUTPUT)/wxsound.exe: test_sound.c ../test_output.h Makefile | $(OUTPUT)
	$(CC) $(CPU) -Os -nostdlib -fno-builtin -s $(OS_VERSION) \
	    -Wl,--entry,$(TEST_ENTRY),--subsystem,console:$(VERSION),--no-insert-timestamp \
	    $< -lwinmm -luser32 -lkernel32 -o $@

-include $(OBJS:.o=.d) $(INPUT_OBJS:.o=.d) $(INPUT_OBJECTS)/dinput_main.d $(INPUT_OBJECTS)/dinput8_main.d
