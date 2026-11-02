# Generated Realtek identity inventory

This generated file compares source identities only. Runtime evidence and open work are maintained separately in SOURCE-AUDIT.md.

Linux reference: `65ba8f34ec886ec72b2c07ddc8cf3d815c32869d`. Exact paths and SHA-256 values are in `source-manifest.json`.

## PCI identities

| Device ID | Selected backend | Declared name | Evidence |
| --- | --- | --- | --- |
| `8029` | `RTL_BACKEND_NE2000` | Realtek RTL8029(AS) PCI Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8129` | `RTL_BACKEND_8139` | Realtek RTL8129 Fast Ethernet | Ambiguous legacy/current ID usage; see SOURCE-AUDIT.md |
| `8138` | `RTL_BACKEND_8139` | Realtek RTL8139B CardBus Fast Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8139` | `RTL_BACKEND_8139` | Realtek RTL8139 Family Fast Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8130` | `RTL_BACKEND_8139` | Realtek RTL8139D Fast Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8100` | `RTL_BACKEND_8139` | Realtek RTL8100 Fast Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8169` | `RTL_BACKEND_CPLUS` | Realtek RTL8169/8110 Gigabit Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8167` | `RTL_BACKEND_CPLUS` | Realtek RTL8110SC/8169SC Gigabit Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8168` | `RTL_BACKEND_CPLUS` | Realtek RTL8168/8111 PCIe Gigabit Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8161` | `RTL_BACKEND_CPLUS` | Realtek RTL8168 PCIe Gigabit Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8162` | `RTL_BACKEND_CPLUS` | Realtek RTL8168 PCIe Gigabit Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8136` | `RTL_BACKEND_CPLUS` | Realtek RTL810xE PCIe Fast Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8101` | `RTL_BACKEND_CPLUS` | Realtek RTL8101E/8102E PCIe Fast Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8411` | `RTL_BACKEND_CPLUS` | Realtek RTL8411 PCIe Gigabit Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `2502` | `RTL_BACKEND_CPLUS` | Realtek RTL8168 PCIe Gigabit Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `2600` | `RTL_BACKEND_CPLUS` | Realtek RTL8168 PCIe Gigabit Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8125` | `RTL_BACKEND_CPLUS` | Realtek RTL8125 2.5GbE Family Controller | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `3000` | `RTL_BACKEND_CPLUS` | Realtek RTL8125 2.5GbE Controller | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `5000` | `RTL_BACKEND_CPLUS` | Realtek RTL8125 2.5GbE Controller | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `0e10` | `RTL_BACKEND_CPLUS` | Realtek RTL8125 2.5GbE Controller | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8126` | `RTL_BACKEND_CPLUS` | Realtek RTL8126 5GbE Family Controller | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8127` | `RTL_BACKEND_CPLUS` | Realtek RTL8127 10GbE Family Controller | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |

## C+ revision signatures

| Mask / XID | Linux identity | Current classification |
| --- | --- | --- |
| `7cf/6c9` | RTL8127A (MAC version 80) | Realtek RTL8127A 10GbE Controller |
| `7cf/64a` | RTL8126A (MAC version 70) | Realtek RTL8126A 5GbE Controller |
| `7cf/649` | RTL8126A (MAC version 70) | Realtek RTL8126A 5GbE Controller |
| `7cf/681` | RTL8125BP (MAC version 66) | Realtek RTL8125BP 2.5GbE Controller |
| `7cf/708` | RTL8125CP (MAC version 65) | Realtek RTL8125CP 2.5GbE Controller |
| `7cf/68b` | RTL9151A (MAC version 64) | Realtek RTL9151A 2.5GbE Controller |
| `7cf/68a` | RTL8125K (MAC version 64) | Realtek RTL8125K 2.5GbE Controller |
| `7cf/689` | RTL8125D (MAC version 64) | Realtek RTL8125D 2.5GbE Controller |
| `7cf/688` | RTL8125D (MAC version 64) | Realtek RTL8125D 2.5GbE Controller |
| `7cf/641` | RTL8125B (MAC version 63) | Realtek RTL8125B 2.5GbE Controller |
| `7cf/609` | RTL8125A (MAC version 61) | Realtek RTL8125A 2.5GbE Controller |
| `7cf/54b` | RTL8168fp/RTL8117 (MAC version 52) | Realtek RTL8168fp/8117 PCIe Gigabit Ethernet |
| `7cf/54a` | RTL8168fp/RTL8117 (MAC version 52) | Realtek RTL8168fp/8117 PCIe Gigabit Ethernet |
| `7cf/502` | RTL8168ep/8111ep (MAC version 51) | Realtek RTL8168ep/8111ep PCIe Gigabit Ethernet |
| `7cf/541` | RTL8168h/8111h (MAC version 46) | Realtek RTL8168h/8111h PCIe Gigabit Ethernet |
| `7cf/6c0` | RTL8168M (MAC version 46) | Realtek RTL8168M Gigabit Ethernet |
| `7cf/5c8` | RTL8411b (MAC version 44) | Realtek RTL8411b PCIe Gigabit Ethernet |
| `7cf/509` | RTL8168gu/8111gu (MAC version 42) | Realtek RTL8168gu/8111gu PCIe Gigabit Ethernet |
| `7cf/4c0` | RTL8168g/8111g (MAC version 40) | Realtek RTL8168g/8111g PCIe Gigabit Ethernet |
| `7c8/488` | RTL8411 (MAC version 38) | Realtek RTL8411 PCIe Gigabit Ethernet |
| `7cf/481` | RTL8168f/8111f (MAC version 36) | Realtek RTL8168f/8111f PCIe Gigabit Ethernet |
| `7cf/480` | RTL8168f/8111f (MAC version 35) | Realtek RTL8168f/8111f PCIe Gigabit Ethernet |
| `7c8/2c8` | RTL8168evl/8111evl (MAC version 34) | Realtek RTL8168evl/8111evl PCIe Gigabit Ethernet |
| `7cf/2c1` | RTL8168e/8111e (MAC version 32) | Realtek RTL8168e/8111e PCIe Gigabit Ethernet |
| `7c8/2c0` | RTL8168e/8111e (MAC version 33) | Realtek RTL8168e/8111e PCIe Gigabit Ethernet |
| `7cf/281` | RTL8168d/8111d (MAC version 25) | Realtek RTL8168d/8111d PCIe Gigabit Ethernet |
| `7c8/280` | RTL8168d/8111d (MAC version 26) | Realtek RTL8168d/8111d PCIe Gigabit Ethernet |
| `7cf/28a` | RTL8168dp/8111dp (MAC version 28) | Realtek RTL8168dp/8111dp PCIe Gigabit Ethernet |
| `7cf/28b` | RTL8168dp/8111dp (MAC version 31) | Realtek RTL8168dp/8111dp PCIe Gigabit Ethernet |
| `7cf/3c9` | RTL8168cp/8111cp (MAC version 23) | Realtek RTL8168cp/8111cp PCIe Gigabit Ethernet |
| `7cf/3c8` | RTL8168cp/8111cp (MAC version 18) | Realtek RTL8168cp/8111cp PCIe Gigabit Ethernet |
| `7c8/3c8` | RTL8168cp/8111cp (MAC version 24) | Realtek RTL8168cp/8111cp PCIe Gigabit Ethernet |
| `7cf/3c0` | RTL8168c/8111c (MAC version 19) | Realtek RTL8168c/8111c PCIe Gigabit Ethernet |
| `7cf/3c2` | RTL8168c/8111c (MAC version 20) | Realtek RTL8168c/8111c PCIe Gigabit Ethernet |
| `7cf/3c3` | RTL8168c/8111c (MAC version 21) | Realtek RTL8168c/8111c PCIe Gigabit Ethernet |
| `7c8/3c0` | RTL8168c/8111c (MAC version 22) | Realtek RTL8168c/8111c PCIe Gigabit Ethernet |
| `7c8/380` | RTL8168b/8111b (MAC version 17) | Realtek RTL8168b/8111b PCIe Gigabit Ethernet |
| `7c8/448` | RTL8106e (MAC version 39) | Realtek RTL8106e PCIe Fast Ethernet |
| `7c8/440` | RTL8402 (MAC version 37) | Realtek RTL8402 PCIe Fast Ethernet |
| `7cf/409` | RTL8105e (MAC version 29) | Realtek RTL8105e PCIe Fast Ethernet |
| `7c8/408` | RTL8105e (MAC version 30) | Realtek RTL8105e PCIe Fast Ethernet |
| `7cf/349` | RTL8102e (MAC version 8) | Realtek RTL8102e PCIe Fast Ethernet |
| `7cf/249` | RTL8102e (MAC version 8) | Realtek RTL8102e PCIe Fast Ethernet |
| `7cf/348` | RTL8102e (MAC version 7) | Realtek RTL8102e PCIe Fast Ethernet |
| `7cf/248` | RTL8102e (MAC version 7) | Realtek RTL8102e PCIe Fast Ethernet |
| `7cf/240` | RTL8401 (MAC version 14) | Realtek RTL8401 PCIe Fast Ethernet |
| `7c8/348` | RTL8102e/RTL8103e (MAC version 9) | Realtek RTL8102e/8103e PCIe Fast Ethernet |
| `7c8/248` | RTL8102e/RTL8103e (MAC version 9) | Realtek RTL8102e/8103e PCIe Fast Ethernet |
| `7c8/340` | RTL8101e/RTL8100e (MAC version 10) | Realtek RTL8101e/8100e PCIe Fast Ethernet |
| `fc8/980` | RTL8169sc/8110sc (MAC version 6) | Realtek RTL8169sc/8110sc Gigabit Ethernet |
| `fc8/180` | RTL8169sc/8110sc (MAC version 5) | Realtek RTL8169sc/8110sc Gigabit Ethernet |
| `fc8/100` | RTL8169sb/8110sb (MAC version 4) | Realtek RTL8169sb/8110sb Gigabit Ethernet |
| `fc8/040` | RTL8110s (MAC version 3) | Realtek RTL8110s Gigabit Ethernet |
| `fc8/008` | RTL8169s (MAC version 2) | Realtek RTL8169s Gigabit Ethernet |

The extended `0x7c8`/secondary-XID path is not implemented. Table agreement does not validate MAC initialization, firmware or PHY workarounds.

## USB identities

| PID | Declared name | Current state |
| --- | --- | --- |
| `8150` | Realtek RTL8150 USB 10/100 Fast Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8151` | Realtek RTL8151 USB 10/100 Fast Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8152` | Realtek RTL8152B USB 10/100 Fast Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8153` | Realtek RTL8153/8153B/C/D/E USB Gigabit Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8154` | Realtek RTL8154/8154B USB 2.0 Gigabit Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8155` | Realtek RTL8156 USB 2.5G Ethernet (8155 bridge) | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8156` | Realtek RTL8156/8156B(G) USB 2.5G Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8157` | Realtek RTL8157 USB 5G Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |
| `8159` | Realtek RTL8159 USB 10G Ethernet | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |

Table agreement alone does not validate MAC/PHY initialization, firmware, OS behavior or hardware. Consult SOURCE-AUDIT.md and STATUS.md for maintained coverage.

