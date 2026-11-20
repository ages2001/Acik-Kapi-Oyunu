WINE_SRC := $(BUILD)/wine-2.0.5-$(ARCH)-source
WINE_HOST := $(BUILD)/wine-2.0.5-$(ARCH)-tools
WINE_BUILD := $(BUILD)/wine-2.0.5-$(ARCH)
WINE_OUT := $(OUT)/d3d
D3DX9_VERSIONS := 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43
WINE_MODULES := d3d9 d3d11 dxgi wined3d d3dxof $(addprefix d3dx9_,$(D3DX9_VERSIONS)) d3dx11_42 d3dx11_43
D3DX10_VERSIONS := 33 34 35 36 37 38 39 40 41 42 43
WINE_ADDITIONS := ddraw ddrawex d3dim d3drm d3d8 d3d10 d3d10_1 d3d10core dxdiagn \
    d3dcompiler_47 xinput1_1 xinput1_2 xinput1_3 xinput1_4 xinput9_1_0 $(addprefix d3dx10_,$(D3DX10_VERSIONS))
WINE_MODULES += $(WINE_ADDITIONS)
WINE_PATH := $(abspath $(WINE_BUILD)/toolchain):$(PATH)
WINE_CFLAGS := -O2 -std=gnu17 -Wno-implicit-function-declaration
HOST_CC ?= cc

all d3d compute: $(OUT)/wxinv.exe

$(OUT)/wxinv.exe: inventory/install.c inventory/runtime.h inventory/device.h inventory/display.h inventory/legacy_device.c inventory/Makefile
	$(MAKE) -C inventory ARCH=$(ARCH) OUT="$(abspath $(OUT))" BUILD="$(abspath $(BUILD))/inventory-$(ARCH)"

.PHONY: d3d
d3d: $(WINE_BUILD)/Makefile $(WINE_OUT)/wxredist.exe
	PATH="$(WINE_PATH)" $(MAKE) -C $(WINE_BUILD) LDFLAGS=-static-libgcc \
	    $(addprefix dlls/,$(WINE_MODULES)) dlls/d3dcompiler_43 libs/wine
	mkdir -p $(WINE_OUT)
	set -e; for module in $(WINE_MODULES); do \
	    $(STRIP) -o $(WINE_OUT)/$$module.dll $(WINE_BUILD)/dlls/$$module/$$module.dll; \
	done
	$(STRIP) -o $(WINE_OUT)/libwine.dll $(WINE_BUILD)/libs/wine/libwine.dll
	$(STRIP) -o $(WINE_OUT)/wxlegacy.dll $(WINE_BUILD)/dlls/d3dcompiler_43/d3dcompiler_43.dll

$(WINE_OUT)/wxredist.exe: test_redist.c test_output.h
	mkdir -p $(WINE_OUT)
	$(CC) $(ARCH_FLAGS) -Os -fno-builtin -nostdlib $(TEST_PLATFORM) \
	    -Wl,--entry,$(if $(filter x86,$(ARCH)),_redist_entry@0,redist_entry),--no-insert-timestamp \
	    -s $< -lkernel32 -o $@

$(WINE_SRC)/.prepared: vendor/wine-2.0.5.tar.xz wine-2.0.5.patch identity/wx_identity.h identity/wx_firmware.h inventory/runtime.h inventory/display.h inventory/provider_display.h
	printf '%s  %s\n' '2885bcf8324e3e7e354c0ab01e5020c82c9c9676d2255f7bfd52b3739a00e5f3' '$<' | sha256sum --check
	mkdir -p $(WINE_SRC)
	tar -xf $< --strip-components=1 -C $(WINE_SRC)
	patch --batch -p1 -d $(WINE_SRC) < wine-2.0.5.patch
	cp identity/wx_identity.h identity/wx_firmware.h $(WINE_SRC)/dlls/dxdiagn/
	cp inventory/runtime.h $(WINE_SRC)/dlls/dxdiagn/wx_inventory.h
	cp inventory/display.h $(WINE_SRC)/dlls/dxdiagn/wx_display.h
	cp inventory/provider_display.h $(WINE_SRC)/dlls/dxdiagn/wx_provider_display.h
	touch $@

$(WINE_HOST)/.ready: $(WINE_SRC)/.prepared wine.mk
	mkdir -p $(WINE_HOST)
	cd $(WINE_HOST) && CC="$(HOST_CC)" CFLAGS="$(WINE_CFLAGS)" \
	    "$(abspath $(WINE_SRC))/configure" --enable-win64 --without-x --without-freetype
	$(MAKE) -C $(WINE_HOST) tools/winebuild tools/widl tools/winegcc tools/wrc tools/wmc
	touch $@

$(WINE_BUILD)/Makefile: $(WINE_HOST)/.ready
	mkdir -p $(WINE_BUILD)/toolchain
	ln -sf "$$(command -v $(CC))" $(WINE_BUILD)/toolchain/$(TRIPLE)-gcc
	ln -sf "$$(command -v $(CXX))" $(WINE_BUILD)/toolchain/$(TRIPLE)-g++
	cd $(WINE_BUILD) && PATH="$(WINE_PATH)" CC="$(CC)" CXX="$(CXX)" CFLAGS="$(WINE_CFLAGS)" \
	    "$(abspath $(WINE_SRC))/configure" --host=$(TRIPLE) $(if $(filter x64,$(ARCH)),--enable-win64) \
	    --with-wine-tools="$(abspath $(WINE_HOST))" --without-x --without-freetype
