LLVM_ARCHIVE := vendor/llvm-12.0.1.src.tar.xz
LLVM_SRC := $(BUILD)/llvm-12-source
LLVM_HOST := $(BUILD)/llvm-12-host
LLVM_TARGET := $(BUILD)/llvm-12-$(ARCH)
LLVM_MODULES := LLVMX86CodeGen LLVMX86AsmParser LLVMX86Disassembler LLVMMCJIT LLVMCoroutines LLVMBitWriter LLVMipo
LLVM_TRIPLE := $(if $(filter x86,$(ARCH)),i686,x86_64)-w64-windows-gnu
LLVM_OPTIONS := -G Ninja -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=X86 \
    -DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_DOCS=OFF -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_INCLUDE_BENCHMARKS=OFF -DLLVM_ENABLE_TERMINFO=OFF -DLLVM_ENABLE_ZLIB=OFF \
    -DLLVM_ENABLE_LIBXML2=OFF -DLLVM_ENABLE_Z3_SOLVER=OFF -DLLVM_ENABLE_BINDINGS=OFF \
    -DLLVM_ENABLE_LIBPFM=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
LLVM_MESA := $(BUILD)/mesa-llvmpipe-$(ARCH)
LLVM_DLL := $(LLVM_MESA)/src/gallium/targets

$(LLVM_SRC)/.prepared: $(LLVM_ARCHIVE) llvm-12.0.1.patch
	printf '%s  %s\n' '7d9a8405f557cefc5a21bf5672af73903b64749d9bc3a50322239f56f34ffddf' '$<' | sha256sum --check
	mkdir -p $(LLVM_SRC)
	tar -xf $< --strip-components=1 -C $(LLVM_SRC)
	patch --batch -p1 -d $(LLVM_SRC) < llvm-12.0.1.patch
	touch $@

$(LLVM_HOST)/bin/llvm-tblgen: $(LLVM_SRC)/.prepared
	cmake -S $(LLVM_SRC) -B $(LLVM_HOST) $(LLVM_OPTIONS)
	cmake --build $(LLVM_HOST) --parallel $(MESA_JOBS) --target llvm-tblgen

$(LLVM_TARGET)/.libraries: $(LLVM_HOST)/bin/llvm-tblgen llvm.mk
	cmake -S $(LLVM_SRC) -B $(LLVM_TARGET) $(LLVM_OPTIONS) \
	    -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=$(MESA_FAMILY) \
	    -DCMAKE_C_COMPILER="$(CC)" -DCMAKE_CXX_COMPILER="$(CXX)" \
	    $(if $(filter x86,$(ARCH)),-DCMAKE_C_FLAGS_RELEASE="-O1 -DNDEBUG" -DCMAKE_CXX_FLAGS_RELEASE="-O1 -DNDEBUG") \
	    -DCMAKE_RC_COMPILER=$(TRIPLE)-windres -DLLVM_HOST_TRIPLE=$(LLVM_TRIPLE) \
	    -DLLVM_DEFAULT_TARGET_TRIPLE=$(LLVM_TRIPLE) -DLLVM_TABLEGEN="$(abspath $<)"
	cmake --build $(LLVM_TARGET) --parallel $(MESA_JOBS) --target $(LLVM_MODULES)
	touch $@

$(LLVM_MESA)/build.ninja: $(LLVM_TARGET)/.libraries $(MESA_SRC)/.prepared $(MESA_BUILD)/cross.ini llvm.mk
	mkdir -p $(LLVM_MESA)
	printf '%s\n' '[binaries]' "cmake = 'cmake'" '[built-in options]' \
	    "cmake_prefix_path = ['$(abspath $(LLVM_TARGET))']" > $(LLVM_MESA)/llvm.ini
	meson setup $(LLVM_MESA) $(MESA_SRC) --cross-file $(MESA_BUILD)/cross.ini \
	    --cross-file $(LLVM_MESA)/llvm.ini $(MESA_OPTIONS) -Dllvm=enabled -Dshared-llvm=disabled

.PHONY: llvmpipe
llvmpipe: $(LLVM_MESA)/build.ninja $(PTHREAD_DLL) $(OUT)/woglbase.exe $(OUT)/wogltest.exe
	ninja -C $(LLVM_MESA) -j$(MESA_JOBS) src/gallium/targets/libgl-gdi/opengl32.dll
	mkdir -p $(OUT)/llvmpipe
	$(STRIP) -o $(OUT)/llvmpipe/opengl32.dll $(LLVM_DLL)/libgl-gdi/opengl32.dll
	$(STRIP) -o $(OUT)/llvmpipe/libgallium_wgl.dll $(LLVM_DLL)/wgl/libgallium_wgl.dll
	$(STRIP) -o $(OUT)/llvmpipe/libwinpthread-1.dll "$(PTHREAD_DLL)"
