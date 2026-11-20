D12_SRC := $(BUILD)/vkd3d-$(VERSION)-d3d12-$(ARCH)
D12_BUILD := $(BUILD)/vkd3d-$(VERSION)-device-$(ARCH)
D12_OUT := $(OUT)/d3d12
D12_LIBS := libvkd3d-utils.a libvkd3d.a libvkd3d-shader.a libvkd3d-common.a

.PHONY: d3d12
d3d12: $(D12_OUT)/d3d12.dll $(D12_OUT)/vulkan_lvp.dll $(D12_OUT)/libwinpthread-1.dll $(OUT)/wx12test.exe

$(D12_SRC)/.prepared: vendor/vkd3d-$(VERSION).tar.xz vkd3d-$(VERSION).patch d3d12.patch
	printf '%s  %s\n' '$(SOURCE_HASH)' '$<' | sha256sum --check
	mkdir -p $(D12_SRC)
	tar -xf $< --strip-components=1 -C $(D12_SRC)
	patch --batch -p1 -d $(D12_SRC) < vkd3d-$(VERSION).patch
	patch --batch -p1 -d $(D12_SRC) < d3d12.patch
	touch $@

$(D12_BUILD)/Makefile: $(D12_SRC)/.prepared d3d12.mk
	mkdir -p $(D12_BUILD)/headers
	ln -sfn "$(abspath $(HEADER_ROOT))/vulkan" $(D12_BUILD)/headers/vulkan
	ln -sfn "$(abspath $(HEADER_ROOT))/vk_video" $(D12_BUILD)/headers/vk_video
	ln -sfn "$(abspath $(HEADER_ROOT))/spirv" $(D12_BUILD)/headers/spirv
	cd $(D12_BUILD) && CC="$(CC)" AR="$(TRIPLE)-gcc-ar" RANLIB="$(TRIPLE)-gcc-ranlib" \
	    CPPFLAGS="-I$(abspath $(D12_BUILD))/headers -DWINDX_D3D12_RUNTIME" \
	    CFLAGS="-O1 -std=gnu11 -fno-lto -D_WIN32_WINNT=0x0600" SONAME_LIBVULKAN=vulkan-1.dll \
	    "$(abspath $(D12_SRC))/configure" --host=$(TRIPLE) --disable-tests --disable-demos --disable-shared --enable-static

$(D12_OUT)/d3d12.dll: $(D12_BUILD)/Makefile d3d12.def
	$(MAKE) -C $(D12_BUILD) include/private/vkd3d_version.h include/private/spirv_grammar.h
	$(MAKE) -C $(D12_BUILD) libvkd3d.la libvkd3d-shader.la libvkd3d-utils.la
	mkdir -p $(D12_OUT)
	$(CC) -shared -static-libgcc -Wl,--kill-at,--no-insert-timestamp -s -o $@ d3d12.def \
	    $(D12_BUILD)/libs/vkd3d-utils/libvkd3d_utils_la-vkd3d_utils_main.o \
	    -Wl,--start-group $(addprefix $(D12_BUILD)/.libs/,$(D12_LIBS)) -Wl,--end-group \
	    -lole32 -luuid -ldxguid -lwinpthread

$(D12_OUT)/libwinpthread-1.dll: $(PTHREAD_DLL)
	mkdir -p $(D12_OUT)
	$(STRIP) -o $@ "$<"
