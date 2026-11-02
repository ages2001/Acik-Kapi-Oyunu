USBNT driver disk - Windows XP / Server 2003 x64

F6 floppy: press F6 when "Press F6 if you need to install a third party
SCSI or RAID driver" appears, then S, insert this disk and choose:
  "USB 3 xHCI, USB4, card readers"  Windows keeps its own drivers for the
                                    USB 1.1 / 2.0 controllers
  "every USB controller"            USBNT drives every USB controller

nLite (and other tools that put text-mode drivers into the setup CD):
add this folder's USBNT.INF (or TXTSETUP.OEM) as a text-mode driver and
pick the same entry.  Every file the driver needs is in this folder; use
the folder of your Windows (2KXP for 32 bit Windows 2000 / XP / 2003,
XP64 for XP / 2003 x64).

USBNT's disks (USB disks and sticks, card readers, NVMe drives behind
Thunderbolt / USB4) can then be installed to and installed from; the USB
keyboard works on.  See README.md of the release.
