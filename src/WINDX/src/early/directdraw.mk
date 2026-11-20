$(OBJ)/ddraw_versions.o: early/versions.cpp early/versions.h | $(OBJ)
	$(CXX) $(ARCH_FLAGS) -Os -fno-exceptions -fno-rtti -fno-builtin -std=c++11 -c $< -o $@

$(OUT)/early/ddraw.dll: ddraw_gdi.c ddraw_gdi.def early/surface_dc.inc early/palette.inc early/clipper.inc early/private_data.inc early/versions.h $(OBJ)/ddraw_versions.o | $(OUT)/early
	$(CC) $(ARCH_FLAGS) -Os -fno-builtin -nostdlib -shared $(PLATFORM_FLAGS) \
	    -Wl,--entry,0,--kill-at,--enable-stdcall-fixup,--no-insert-timestamp \
	    -s ddraw_gdi.c $(OBJ)/ddraw_versions.o ddraw_gdi.def -luuid -lgdi32 -luser32 -lkernel32 -o $@

all: $(OUT)/early/wxddver.exe

$(OUT)/early/wxddver.exe: test_legacy_ddraw.cpp test_output.h | $(OUT)/early
	$(CXX) $(ARCH_FLAGS) -Os -std=c++11 -fno-builtin -fno-exceptions -fno-rtti -nostdlib $(TEST_PLATFORM) \
	    -Wl,--entry,$(if $(filter x86,$(ARCH)),_legacy_draw_entry@0,legacy_draw_entry),--no-insert-timestamp \
	    -s $< -lgdi32 -luser32 -lkernel32 -o $@
