# Shared build settings for the WGL software renderers.
MESA_SRC := $(BUILD)/mesa-$(ARCH)-source
MESA_BUILD := $(BUILD)/mesa-$(ARCH)
MESA_OUT := $(OUT)
MESA_JOBS ?= 2
MESA_CPU := $(if $(filter x86,$(ARCH)),i686,x86-64)
MESA_FAMILY := $(if $(filter x86,$(ARCH)),x86,x86_64)
MESA_DLL := $(MESA_BUILD)/src/gallium/targets
PTHREAD_SRC := $(BUILD)/winpthreads-12-source
PTHREAD_BUILD := $(BUILD)/winpthreads-12-$(ARCH)
ifeq ($(ARCH),x86)
PTHREAD_DLL := $(PTHREAD_BUILD)/.libs/libwinpthread-1.dll
else
PTHREAD_DLL := $(shell $(CC) -print-file-name=libwinpthread-1.dll)
endif

.PHONY: softpipe
softpipe: $(MESA_BUILD)/build.ninja $(MESA_OUT)/woglbase.exe $(PTHREAD_DLL)
	ninja -C $(MESA_BUILD) -j$(MESA_JOBS) src/gallium/targets/libgl-gdi/opengl32.dll
	mkdir -p $(MESA_OUT)
	$(STRIP) -o $(MESA_OUT)/opengl32.dll $(MESA_DLL)/libgl-gdi/opengl32.dll
	$(STRIP) -o $(MESA_OUT)/libgallium_wgl.dll $(MESA_DLL)/wgl/libgallium_wgl.dll
	$(STRIP) -o $(MESA_OUT)/libwinpthread-1.dll "$(PTHREAD_DLL)"

ifeq ($(ARCH),x86)
$(PTHREAD_SRC)/.prepared: vendor/mingw-w64_12.0.0.orig.tar.bz2 winpthreads-12.patch
	printf '%s  %s\n' 'cc41898aac4b6e8dd5cffd7331b9d9515b912df4420a3a612b5ea2955bbeed2f' '$<' | sha256sum --check
	mkdir -p $(PTHREAD_SRC)
	tar -xf $< --strip-components=3 -C $(PTHREAD_SRC) mingw-w64-v12.0.0/mingw-w64-libraries/winpthreads
	patch --batch -p1 -d $(PTHREAD_SRC) < winpthreads-12.patch
	touch $@

$(PTHREAD_BUILD)/Makefile: $(PTHREAD_SRC)/.prepared
	mkdir -p $(PTHREAD_BUILD)
	cd $(PTHREAD_BUILD) && CC="$(CC)" CFLAGS="-O2 -march=i486" LDFLAGS=-static-libgcc \
	    "$(abspath $(PTHREAD_SRC))/configure" --host=$(TRIPLE) --enable-shared --disable-static

$(PTHREAD_DLL): $(PTHREAD_BUILD)/Makefile
	$(MAKE) -C $(PTHREAD_BUILD) libwinpthread.la
endif

$(MESA_OUT)/woglbase.exe: test_wgl.c test_output.h
	mkdir -p $(MESA_OUT)
	$(CC) $(ARCH_FLAGS) -Os -fno-builtin -nostdlib $(TEST_PLATFORM) \
	    -Wl,--entry,$(if $(filter x86,$(ARCH)),_wgl_test_entry@0,wgl_test_entry),--no-insert-timestamp \
	    -s $< -lkernel32 -luser32 -o $@

$(MESA_SRC)/.prepared: vendor/mesa-21.3.9.tar.xz mesa-21.3.9.patch llvmpipe-glsl460.patch llvmpipe-workers.patch mesa_condition.h mesa_wgl_exports.py
	printf '%s  %s\n' '91fe6f20339884e37a7c47bfba42fb1cd397512c9ed5ad423de369f047cd8b5c' '$<' | sha256sum --check
	mkdir -p $(MESA_SRC)
	tar -xf $< --strip-components=1 -C $(MESA_SRC)
	patch --batch -p1 -d $(MESA_SRC) < mesa-21.3.9.patch
	patch --batch -p1 -d $(MESA_SRC) < llvmpipe-glsl460.patch
	patch --batch -p1 -d $(MESA_SRC) < llvmpipe-workers.patch
	cp mesa_condition.h $(MESA_SRC)/include/c11/
	python3 mesa_wgl_exports.py $(MESA_SRC)/src/gallium/targets/libgl-gdi
	touch $@

$(MESA_BUILD)/cross.ini: mesa-cross.ini.in mesa.mk
	mkdir -p $(MESA_BUILD)/pkgconfig
	sed -e 's|@CC@|$(CC)|g' -e 's|@CXX@|$(CXX)|g' -e 's|@TRIPLE@|$(TRIPLE)|g' \
	    -e 's|@FAMILY@|$(MESA_FAMILY)|g' -e 's|@CPU@|$(MESA_CPU)|g' \
	    -e 's|@PKGDIR@|$(abspath $(MESA_BUILD))/pkgconfig|g' $< > $@

MESA_OPTIONS := --wrap-mode=nodownload -Dbuildtype=release -Doptimization=1 \
    -Dplatforms=windows -Dsse2=false -Ddri-drivers= -Dgallium-drivers=swrast -Dvulkan-drivers= \
    -Dgallium-nine=false -Dgallium-vdpau=disabled \
    -Dgallium-va=disabled -Dglx=disabled -Degl=disabled -Dgbm=disabled \
    -Dgles1=disabled -Dgles2=disabled -Dzstd=disabled -Dzlib=disabled -Dshared-glapi=disabled

$(MESA_BUILD)/build.ninja: $(MESA_SRC)/.prepared $(MESA_BUILD)/cross.ini mesa.mk
	meson setup $(MESA_BUILD) $(MESA_SRC) --cross-file $(MESA_BUILD)/cross.ini \
	    $(MESA_OPTIONS) -Dllvm=disabled
