# Original application icon selection

R3 selects application group 200 from the supplied Adobe FP32 executable.
The original five high-color icon frames are retained byte for byte. Two
16/32-pixel, 16-color fallbacks are generated for legacy icon loaders; only
these fallback frames are quantized. The application logo is the dark-red
background with white f. R2 incorrectly allowed the SWF document icon to be
selected by the legacy loader.

The launcher, player-window compatibility module and program shortcut now
select the application icon. The shortcut references FP32.EXE, index 0.
The separate uninstall shortcut uses UNINST.EXE, index 1 (red cross).
See launcher-icon.json for input hashes and resource checks. Native R3 GUI
validation is recorded separately; resource identity alone is not a GUI test.
