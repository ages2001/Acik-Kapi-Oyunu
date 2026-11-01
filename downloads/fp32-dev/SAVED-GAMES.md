# Saved games on legacy Windows

## Storage contract

Windows NT 3.xx and Windows 3.1x with Win32s must not be treated as systems
with a native AppData folder. The legacy launcher provisions application-owned
storage at `<installation>\UserData`. It passes the selected directory to its
child through the private `FP32_USERDATA` environment variable. It does not
set the system APPDATA variable or create an operating-system AppData profile.

On NT, the full current-user SID is encoded losslessly in 8-character directory
components below UserData. Account renaming does not change the storage key.
For the planned Win32s runtime, the storage key is `Default`; no NT token APIs
may be assumed on that platform. The current launcher remains an NT build:
this design is not evidence that the Win32s runtime is complete.

Where a native SHGetFolderPathW is available, native folder selection is retained.
On legacy systems the compatibility callback maps only current-user CSIDL_APPDATA
to the application-owned directory. Unsupported requests return real errors.

The FAT short-name adapter is confined to this private directory. It stores
logical Unicode names and verified aliases in FP32MAP.DAT. A hash alone never
identifies a save: collisions select a distinct alias and the full name remains
in the map. Backups must include the **entire UserData tree**, including hidden
mapping files. Moving only an individual physical alias loses its logical name.

Uninstall and updates must preserve UserData. The existing uninstaller deletes
only its payload manifest and installation metadata; UserData must never enter
that manifest. Changing the installation directory requires copying the full
UserData tree to preserve saves. Cross-machine NT SID migration is not automatic.

## Validation requirements

- Real Flash SharedObject write, close/reopen, and Windows reboot/reopen.
- Separate game saves and separate NT user identities.
- Long names, Unicode names, collision detection, and corrupt map errors.
- Preserve saves through uninstall and reinstallation.
- Repeat on each supported OS before claiming support for that OS.

## Current evidence (2026-11-01)

The actual NT 3.1 FAT guest passed 48 native adapter checks, including Unicode
long names, extended-path lookup, canonical name buffer handling, enumeration,
rename/copy/delete, a deliberate hash collision with distinct file contents,
and rejection of corrupt mapping metadata. See reports/nt31-maptest.log.

The original FP32 engine opened a real AS2 SharedObject, wrote counter 1 with
flush() returning true, read 1 after closing/reopening the player, and read 1
after a clean Windows reboot. The physical SOL file was copied only while the
VM was stopped; its AMF counter decodes to 1. See
reports/nt31-save-persistence.json and screenshots 629, 630, 634.

This is one AS2 probe on one NT 3.1 account. Game-specific saves, AS3 saves,
multiple account isolation and other OS runtimes still require validation. No claim of complete cross-OS support.

R3 installer follow-up: the native save-tree probe recorded identical paths,
sizes and CRC32 values for all 37 private storage files immediately after
uninstall. The former SWF association was restored. After Windows restart and
reinstallation of the same R3 package, original FP32 loaded counter 1 from
the same SAVETEST.SWF. See reports/r3-save-uninstall.json and screenshot 678.
