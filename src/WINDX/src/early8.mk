all: $(OUT)/wxd3d81.exe

$(OUT)/wxd3d81.exe: test_d3d81.c test_output.h | $(OUT)
	$(CC) $(ARCH_FLAGS) -Os -std=gnu11 -fno-builtin -nostdlib $(TEST_PLATFORM) \
	    -Wl,--entry,$(if $(filter x86,$(ARCH)),_d3d81_entry@0,d3d81_entry),--no-insert-timestamp \
	    -s $< -luser32 -lkernel32 -o $@
