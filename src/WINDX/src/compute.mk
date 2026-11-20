COMPUTE_SRC := $(BUILD)/wine-3.0.5-$(ARCH)-source
COMPUTE_HOST := $(BUILD)/wine-3.0.5-$(ARCH)-tools
COMPUTE_BUILD := $(BUILD)/wine-3.0.5-$(ARCH)
COMPUTE_OUT := $(OUT)/compute
COMPUTE_PATH := $(abspath $(COMPUTE_BUILD)/toolchain):$(PATH)
COMPUTE_CFLAGS := -O1 -std=gnu17 -Wno-error=implicit-function-declaration -Wno-error=incompatible-pointer-types -Wno-error=int-conversion
COMPUTE_MODULES := d3d9 d3d11 dxgi wined3d d3dxof $(WINE_ADDITIONS)

.PHONY: compute
compute: $(COMPUTE_BUILD)/Makefile
	PATH="$(COMPUTE_PATH)" $(MAKE) -C $(COMPUTE_BUILD) LDFLAGS=-static-libgcc \
	    $(addprefix dlls/,$(COMPUTE_MODULES)) libs/wine
	mkdir -p $(COMPUTE_OUT)
	set -e; for module in $(COMPUTE_MODULES); do \
	    $(STRIP) -o $(COMPUTE_OUT)/$$module.dll $(COMPUTE_BUILD)/dlls/$$module/$$module.dll; \
	done
	$(STRIP) -o $(COMPUTE_OUT)/libwine.dll $(COMPUTE_BUILD)/libs/wine/libwine.dll

$(COMPUTE_SRC)/.prepared: vendor/wine-3.0.5.tar.xz wine-3.0.5.patch identity/wx_identity.h identity/wx_firmware.h inventory/runtime.h inventory/display.h inventory/provider_display.h
	printf '%s  %s\n' 'dc429095b564abe45306b957f3f27d426fb37e365b4dee3ec8d61dc66e8ce211' '$<' | sha256sum --check
	mkdir -p $(COMPUTE_SRC)
	tar -xf $< --strip-components=1 -C $(COMPUTE_SRC)
	patch --batch -p1 -d $(COMPUTE_SRC) < wine-3.0.5.patch
	cp identity/wx_identity.h identity/wx_firmware.h $(COMPUTE_SRC)/dlls/dxdiagn/
	cp inventory/runtime.h $(COMPUTE_SRC)/dlls/dxdiagn/wx_inventory.h
	cp inventory/display.h $(COMPUTE_SRC)/dlls/dxdiagn/wx_display.h
	cp inventory/provider_display.h $(COMPUTE_SRC)/dlls/dxdiagn/wx_provider_display.h
	touch $@

$(COMPUTE_HOST)/.ready: $(COMPUTE_SRC)/.prepared compute.mk
	mkdir -p $(COMPUTE_HOST)
	cd $(COMPUTE_HOST) && CC="$(HOST_CC)" CFLAGS="$(COMPUTE_CFLAGS)" \
	    "$(abspath $(COMPUTE_SRC))/configure" --enable-win64 --without-x --without-freetype
	$(MAKE) -C $(COMPUTE_HOST) tools/winebuild tools/widl tools/winegcc tools/wrc tools/wmc
	touch $@

$(COMPUTE_BUILD)/Makefile: $(COMPUTE_HOST)/.ready
	mkdir -p $(COMPUTE_BUILD)/toolchain
	ln -sf "$$(command -v $(CC))" $(COMPUTE_BUILD)/toolchain/$(TRIPLE)-gcc
	ln -sf "$$(command -v $(CXX))" $(COMPUTE_BUILD)/toolchain/$(TRIPLE)-g++
	cd $(COMPUTE_BUILD) && PATH="$(COMPUTE_PATH)" CC="$(CC)" CXX="$(CXX)" CFLAGS="$(COMPUTE_CFLAGS)" \
	    "$(abspath $(COMPUTE_SRC))/configure" --host=$(TRIPLE) $(if $(filter x64,$(ARCH)),--enable-win64) \
	    --with-wine-tools="$(abspath $(COMPUTE_HOST))" --without-x --without-freetype
