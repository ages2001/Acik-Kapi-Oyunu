# Original executable icon preservation

The launcher retains all 40 RT_ICON and all six RT_GROUP_ICON resources from the
user-supplied FP32 EXE, including original resource IDs and English language ID.
No icon image has been redrawn or recolored. The build verifies every resource
byte against the SHA-256-verified original input; see launcher-icon.json.

Copying only the first group initially produced a blank icon in NT 3.1 native
ExtractIcon. That group contains only 32-bit images. Preserving all original
groups fixed native selection without changing the original image data.
Screenshot 589 compares the final launcher, the red-cross uninstaller, and the
adapted original engine using native ExtractIcon/DrawIcon on NT 3.1 VGA16.
The launcher and engine icons match. Modern Windows rendering has not yet been
tested; the original resource bytes are present for its normal selection.

The program shortcut continues to reference APP\FP32.EXE, icon index 0.
The uninstall shortcut continues to reference UNINST.EXE, icon index 1.
This change concerns FP32. FP10's original-engine icon is retained in its
existing checkpoint; its separate installer is not newly certified here.
