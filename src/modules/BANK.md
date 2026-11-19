# Bank access

The DOS broker reserves a single locked XMS block and leaves low extended memory for the legacy Windows loader. Windows 3.0 Enhanced and Windows/386 use a virtual device or monitor extension to transfer through an aligned page. Standard mode can simulate the real-mode interrupt using DPMI 0300h. The payload transfer window holds 32 KiB.

Use INT 2Fh with AX=D74Dh and BX=4258h. AX=5842h reports success. CX=0 returns the bank capacity in KiB in EDX, also split across CX:DX for clients that preserve only 16-bit register values. SI:DI receives the resident transfer buffer. CX=1 reads and CX=2 writes from byte position BP:DX for SI bytes. The broker checks the end of the request against the bank size. CX=3 returns the physical base for supported clients.

CX=4 returns the free KiB. CX=5 reserves an explicit free range. CX=6 releases a claimed range. For these two calls, EDX is the zero-based first unit and ESI the number of 32 KiB units. An invalid or overlapping reservation fails before changing the bitmap. A successful write claims each newly touched unit once. Both the Windows/386 monitor and the Windows 3.0 VxD update the same bitmap as the DOS broker. Direct physical mappings must reserve their extent before use. Cooperating clients release the range when finished.

The shell observer queries AX=D74Fh, BX=5855h for boot capacity and BX=5856h for live free KiB. The return signature is AX=5842h, EDX is KiB and BX distinguishes XMS (0) from bank storage (1). The XMS reading comes from the active driver's XMS 3 function 88h. Enhanced-mode Windows 3.1 and WfW query DPMI 0500h for physical pages.

WINRAM also uses this service as backing for ordinary private movable Win16 global memory on the supported banked profiles. Applications that use `GlobalAlloc`, `GlobalLock` and `GlobalUnlock` can use that path through the installed Windows kernel entries. [Win16 verification](../verification/win16-global-memory.md)
