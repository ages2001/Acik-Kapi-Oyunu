FREEOCL_COMMIT := 6d1d5c909d40a1c203e6d56e8a5c0707118caf30
FREEOCL_SRC := $(BUILD)/freeocl-$(ARCH)-source
FREEOCL_BUILD := $(BUILD)/freeocl-$(ARCH)
FREEOCL_PROC := $(if $(filter x86,$(ARCH)),i686,x86_64)
CPU_JOBS ?= 2
CXX := $(TRIPLE)-g++
STRIP := $(TRIPLE)-strip
THREAD_SRC := $(BUILD)/thread-source
THREAD_BUILD := $(BUILD)/thread-x86
ifeq ($(ARCH),x86)
THREAD_DLL := $(THREAD_BUILD)/.libs/libwinpthread-1.dll
else
THREAD_DLL := $(shell $(CC) -print-file-name=libwinpthread-1.dll)
endif

.PHONY: cpu
cpu: $(FREEOCL_BUILD)/build.ninja $(THREAD_DLL)
	cmake --build $(FREEOCL_BUILD) --target FreeOCL --parallel $(CPU_JOBS)
	mkdir -p $(OUT)/cpu/include
	$(STRIP) -o $(OUT)/cpu/wocl_cpu.dll $(FREEOCL_BUILD)/src/libFreeOCL.dll
	$(STRIP) -o $(OUT)/cpu/libwinpthread-1.dll "$(THREAD_DLL)"
	cp -R $(FREEOCL_SRC)/include/FreeOCL $(OUT)/cpu/include/
	cp $(FREEOCL_BUILD)/generated/FreeOCL/config.h $(OUT)/cpu/include/FreeOCL/config.h

$(FREEOCL_SRC)/.ready: vendor/FreeOCL-$(FREEOCL_COMMIT).tar.gz freeocl.patch win_kernel_tls.h win_kernel_compiler.h
	printf '%s  %s\n' '907a721daada0cfb683e9517c68d6e41c5b7b0ad768309bf03f0fae0f2254fb2' '$<' | sha256sum --check
	mkdir -p $(FREEOCL_SRC)
	tar -xf $< -C $(FREEOCL_SRC) --strip-components=1
	patch --batch -p1 -d $(FREEOCL_SRC) < freeocl.patch
	cp win_kernel_tls.h $(FREEOCL_SRC)/include/FreeOCL/
	cp win_kernel_compiler.h $(FREEOCL_SRC)/src/
	touch $@

$(FREEOCL_BUILD)/build.ninja: $(FREEOCL_SRC)/.ready cpu.mk
	cmake -S $(FREEOCL_SRC) -B $(FREEOCL_BUILD) -G Ninja -DCMAKE_BUILD_TYPE=Release \
	    -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=$(FREEOCL_PROC) \
	    -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX) -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	    -DCMAKE_SHARED_LINKER_FLAGS="-static-libgcc -static-libstdc++ -Wl,--kill-at,--disable-stdcall-fixup"

ifeq ($(ARCH),x86)
.PHONY: win95
win95: all cpu
	python3 classic/relink_win95.py $(OUT)/win95/cpu --cc $(CC) --pthread $(THREAD_BUILD) --freeocl $(FREEOCL_BUILD)
	cp -R $(OUT)/cpu/include $(OUT)/win95/cpu/
	cp $(OUT)/OpenCL.dll $(OUT)/ocltest.exe $(OUT)/win95/

$(THREAD_SRC)/.ready: vendor/mingw-w64_12.0.0.orig.tar.bz2 winpthreads-12.patch
	printf '%s  %s\n' 'cc41898aac4b6e8dd5cffd7331b9d9515b912df4420a3a612b5ea2955bbeed2f' '$<' | sha256sum --check
	mkdir -p $(THREAD_SRC)
	tar -xf $< --strip-components=3 -C $(THREAD_SRC) mingw-w64-v12.0.0/mingw-w64-libraries/winpthreads
	patch --batch -p1 -d $(THREAD_SRC) < winpthreads-12.patch
	touch $@

$(THREAD_BUILD)/Makefile: $(THREAD_SRC)/.ready
	mkdir -p $(THREAD_BUILD)
	cd $(THREAD_BUILD) && CC="$(CC)" CFLAGS="-O2 -march=i486" LDFLAGS=-static-libgcc \
	    "$(abspath $(THREAD_SRC))/configure" --host=$(TRIPLE) --enable-shared --disable-static

$(THREAD_DLL): $(THREAD_BUILD)/Makefile
	$(MAKE) -C $(THREAD_BUILD) libwinpthread.la
endif
