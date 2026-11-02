# Remaining work and verification boundaries

The combined 1.3 candidate is not completion of the request for every Realtek
LAN device on every Windows NT version. These are explicit gaps, not inferred
physical support from a passing emulator.

| Area | Current boundary | Evidence needed to close it |
| --- | --- | --- |
| Physical PHY, silicon and firmware MCU | Only QEMU is available. Models check register/packet/patch ordering; they do not execute the device MCU or analog calibration. | Per-revision physical hardware and independent traces/traffic/fault tests. |
| Extended PCI XID | The secondary-XID path, including the pinned RTL9151AS entry, is not implemented. Unknown extended identities fail closed. | Source-defined secondary identification, MAC/PHY/firmware selection and independent negative/positive device tests. |
| SFP/SerDes | Startup is explicitly gated; copper behavior must not be reused as SerDes support. | Separate source-defined reset, link and management paths and hardware/model validation. |
| Full PCI power-loss/resume | Initial start and passive-reset preparation are integrated; complete power/platform combinations are not established. | Per-family power-loss and restoration verification, including failure/retry paths. |
| USBNT controller sleep | Root-device mixed-owner tests pass. Hubs, unknown owners, retained generic-client state and active EHCI companions are rejected before mutation. Injected system IRPs are not actual platform ACPI S3. | Additional supported topology implementations and full-machine sleep testing. |
| Historical attachment latency | Earlier NT3.51 RTL8159 host-time attachment timeouts remain unexplained. Measured coarse/fast/coarse tests and later regressions pass. | A reproducer and causal fix; passing later runs alone is insufficient. |
| Historical XP startup failure | Earlier first-cycle Code31 evidence remains preserved; current repeated matrices pass. | A deterministic old-fail/new-pass reproducer before claiming the historical failure is causally fixed. |
| Device/OS coverage | Five supplied OS fixtures and explicitly documented model profiles are tested. NT3.50, XP x86, every service pack and every Realtek revision are not covered. | Additional OS fixtures and a per-revision cross-product matrix; ID aliases alone are not independent silicon models. |
| NT3.1 DHCP | The utility saves a selected lease for the next restart; continuous renewal and live address replacement are not implemented. Distinct configured multi-adapter subnets are required. | A supported live TCP/IP reconfiguration mechanism and lease lifecycle implementation/tests. |
| Internet use | Local full-payload ICMP/TCP is verified; prior proxy HTTP has separate evidence. | Direct DNS, TLS and browser tests for each intended OS/application. |

Windows 2000 same-port composite reinsertion is verified. An accidental QEMU
port-2 attachment created a new device instance requiring first binding; it is
retained separately and does not count as automatic new-port installation.

New RLTKLANT package assembly uses exact tested drivers, previously tested
Setup helper bytes and INF templates differing only in calculated sizes. A
fresh installation of every final combined legacy package is not claimed.
USBNT's exact 1.3 package upgrade has separate real installed-Windows evidence.
OEM floppy payload readback is not a fresh OS text-mode installation test.

See STATUS.md, SOURCE-AUDIT.md and the exact-hash proof JSON files. Prior
proofs remain historical; their results are not assigned to newer binaries.
