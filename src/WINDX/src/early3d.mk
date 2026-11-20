all: $(OUT)/wxold3d.exe

$(OUT)/wxold3d.exe: test_early_d3d.c test_output.h | $(OUT)
	$(CC) $(ARCH_FLAGS) -Os -fno-builtin -nostdlib $(TEST_PLATFORM) \
	    -Wl,--entry,$(if $(filter x86,$(ARCH)),_old3d_entry@0,old3d_entry),--no-insert-timestamp \
	    -s $< -luser32 -lkernel32 -o $@

all: $(OUT)/wxrm.exe

$(OUT)/wxrm.exe: test_retained.c test_output.h | $(OUT)
	$(CC) $(ARCH_FLAGS) -Os -fno-builtin -nostdlib $(TEST_PLATFORM) \
	    -Wl,--entry,$(if $(filter x86,$(ARCH)),_retained_entry@0,retained_entry),--no-insert-timestamp \
	    -s $< -lkernel32 -o $@
