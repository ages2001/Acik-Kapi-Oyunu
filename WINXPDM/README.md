# WINXPDM

WINXPDM is a development port of the Windows XP display driver model to older Windows systems. The GeForce and Radeon work covers original driver packages, the supplied XP x86 Maxwell/Pascal driver, and QEMU and Linux tests for Maxwell, Pascal and RDNA 4. Native GPU rendering, DisplayPort HBR3, HDMI 2.0 and extended-desktop results are tracked per Windows generation. [Priority GPUs and feature matrix](tests/gpu/priority.md) · [All audited GPU IDs](tests/gpu/results/xp-pci.tsv)

A second native driver submits vertex and fragment shaders to QEMU's VirtIO GPU. The Windows probes check interpolation, depth and texture sampling. The dual-output test presents two different images. Install the matching package, select **Keep my display driver and install VirtIO 3D**, and choose **Test 3D** in Info. [Windows GPU results and instructions](tests/gpu/windows/README.md)

The reference Windows driver supports PCI 1234:1111. Seven native packages include the English setup wizard, Info and shell-integrated removal. The optional WINRAM integration was tested with 1 GiB on enhanced Windows 3.0a, 3.1, WfW 3.11, 95 OSR2, 98 SE and Me. [Windows guest results](tests/README.md) · [WINRAM memory tests](tests/winram/README.md)

QEMU exposes a linear framebuffer and the Bochs display registers. The display modules share the register programming code. Windows 3.x uses a Win16 display DDI, Windows 9x uses a display DRV and a VxD, and Windows NT uses a video miniport and a GDI display DLL. NT 3.x loads its display DLL into the windowing subsystem. NT 4.0 and later load the kernel-mode DLL. Loading independent vendors’ XPDM binaries on earlier systems also requires the operating-system interfaces those binaries call. The vendor compatibility work is in progress.

## Packages

| Windows guest | Processor | Folder |
| --- | --- | --- |
| Windows 3.0 in 386 enhanced mode | x86 | [WIN30](bin/WIN30) |
| Windows 3.1, 3.11 and Windows for Workgroups 3.11 in 386 enhanced mode | x86 | [WIN31](bin/WIN31) |
| Windows 95, 98 and Millennium Edition | x86 | [WIN9X](bin/WIN9X) |
| Windows NT 3.1, 3.5 and 3.51 | x86 | [NT3X](bin/NT3X) |
| Windows NT 4.0 | x86 | [NT4](bin/NT4) |
| Windows 2000 and Windows XP | x86 | [NT5X86](bin/NT5X86) |
| Windows XP Professional x64 Edition | AMD64 | [XPAMD64](bin/XPAMD64) |

## Installation

1. For the reference display driver, configure QEMU with `-vga std`. For the shader driver, configure `-device virtio-vga-gl,max_outputs=2` with a GL display and use the **Keep my display driver and install VirtIO 3D** option. The tests used a PC machine, an IDE disk and SeaBIOS.
2. Copy the entire package folder to the guest. Run `SETUP.EXE` from that folder. On NT, sign in with an administrator account.
3. Choose **Next**, review the destination, choose **Install**, then **Finish**. Follow any driver selection or signature dialogs from Windows. Restart Windows and open **WINXPDM Info** to read the active resolution and color depth.

On Program Manager systems, open the **WINXPDM** group for **WINXPDM Info** and **Uninstall WINXPDM**. On Explorer systems, Info is on the desktop and removal is available through **Control Panel > Add/Remove Programs**. The uninstall artwork adds a red cross to the monitor icon.

The Win16 wizard records the previous display settings in its private installation directory. The NT 3.x/4.0 installer places the miniport in the Video service group. Windows 9x uses the native display control panel to select the adapter. Windows 2000 and XP use the Windows device installer. Restart when asked. If Windows still has the old display file open, finish the removal by running Uninstall again after restarting.

## Building and testing

The binary packages, corresponding driver source and build commands are in this checkout. The build generates its Windows artwork locally. [Package contents](bin/README.md) · [Source and build instructions](src/README.md) · [Test methods and results](tests/README.md)

WINXPDM incorporates the open-source WINVBE display drivers. The project license is the GNU General Public License, version 3. See the [license](LICENSE) and [third-party notices](licenses/README.md).
