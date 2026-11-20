# App-local DirectInput, DirectMusic and DirectPlay providers.
MEDIA_MODULES := dinput dinput8 dplayx dmusic
.PHONY: media
media: $(WINE_BUILD)/Makefile $(OUT)/wxmedia.exe
	PATH="$(WINE_PATH)" $(MAKE) -C $(WINE_BUILD) LDFLAGS=-static-libgcc \
	    $(addprefix dlls/,$(MEDIA_MODULES)) libs/wine
	mkdir -p $(OUT)/media
	set -e; for module in $(MEDIA_MODULES); do \
	    $(STRIP) -o $(OUT)/media/$$module.dll $(WINE_BUILD)/dlls/$$module/$$module.dll; \
	done
	$(STRIP) -o $(OUT)/media/libwine.dll $(WINE_BUILD)/libs/wine/libwine.dll
