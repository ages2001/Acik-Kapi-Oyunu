# DHCPNT31 — Windows NT 3.1 DHCP lease utility

DHCPNT31 requests a lease through Windows Sockets 1.1 and saves static TCP/IP
registry values for the next restart. Windows NT 3.1 has no built-in DHCP
client. This utility does not change the active address, run as a service, or
renew a lease continuously.

## Build

```sh
bash build.sh
```

The executable is freestanding, targets the Pentium instruction baseline and
PE subsystem 3.10, and imports KERNEL32, ADVAPI32 and WSOCK32. It does not need
a C runtime DLL. `bin/DHCPNT31.EXE` is a development candidate; distribution
packages must be rebuilt and their installer sizes updated before release.

## Commands

```text
DHCPNT31 show
DHCPNT31 52-54-00-ab-cd-ef
DHCPNT31 52-54-00-ab-cd-ef /apply
DHCPNT31 /auto /apply
```

`show` lists bound adapters and their saved TCP/IP settings. An explicit MAC
requests a lease and asks before saving it; `/apply` saves without that prompt.
The explicit single-MAC form addresses the first eligible adapter in registry
enumeration order. `/auto` discovers MACs published in each adapter's registry
parameters and processes only adapters with an explicit EnableDHCP DWORD of 1.
For multiple adapters, the client requires valid, distinct, non-overlapping
saved IPv4 subnets and binds the selected local address. Ambiguous or
unconfigured multi-adapter networks are rejected before transmission. The
saved address must also exist on the running system for the bind to succeed.

The client writes `IPAddress`, `SubnetMask`, `DefaultGateway` and `NameServer`
under `Services\<adapter>\Parameters\Tcpip` using NT 3.1's string format. It
also updates the global resolver configuration. A successful apply appends a
record to `C:\DHCPNT31.LOG`. A restart is needed for saved address changes.

The old OEM setup Run value did not execute in the actual NT 3.1 logon test.
The current installer maintains one owned item in the installing user's
Program Manager Startup group. The v9 package passes native Update/Configure,
disable/removal, custom-group migration and repeat invocation, and refusal to
replace an unrelated same-title command. Cold-logon packet captures show one
PCI DHCP exchange and no disabled-USB DHCP packets; disabled cold logon shows
no DHCP traffic for over 60 seconds. First-driver-path setup with DHCP No also
passes. See the exact v9 proofs in `../../validation/STATUS.md`. This remains
an installing-user logon utility, not an all-user DHCP service.

## Validation

Earlier single-interface QEMU tests completed DHCP discovery, request and
registry application. The November 1 packet-bounds candidate additionally
checks received length, BOOTREPLY identity, Ethernet hardware type and length,
transaction ID, client MAC, magic cookie and bounded option records. ACKs must
identify the selected server and offered address. Its production parser passes
truncation/header/identity/option tests and 65,536 fuzz datagrams under
ASan/UBSan. Actual exchange tests for each candidate are recorded separately
in `../../validation/STATUS.md` and the frozen binary manifests.

The bound-source candidate completed DISCOVER/OFFER/REQUEST/ACK on the
enabled PCI adapter of an actual NT 3.1 guest with two distinct configured
subnets. The disabled USB adapter emitted no DHCP packets. The packet-bounds
candidate also completed an explicit first-adapter exchange. See
`../../validation/dhcpnt31-bind-guest-proof-20261101.json` and the exact
candidate manifests. Winsock 1.1 select bounds each receive wait to five
seconds without relying on unsupported socket timeout options.

These tests do not establish operation on unconfigured or overlapping
networks. A successful exchange also does not establish automatic lease
renewal, live address replacement, direct DNS/TLS access or physical-hardware
compatibility. Complete routing, opt-in and lease-policy validation remains
part of the development work.

Copyright (C) 2026 ages2001.
