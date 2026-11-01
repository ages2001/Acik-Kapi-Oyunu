# Requested game inputs

Super Mario 63 (2010), supplied by the user, remains the first gameplay test.
The following additional games were retrieved on 2026-11-01. Downloading and
validating their SWF containers does not establish compatibility with the port.

| Game | Selected version/source |
| --- | --- |
| Final Fantasy Sonic | [X6](https://archive.org/details/ffsx6.) |
| MMS RPG | Episodes [1](https://www.newgrounds.com/portal/view/560560), [2](https://www.newgrounds.com/portal/view/561277), [3](https://www.newgrounds.com/portal/view/562563), [4](https://www.newgrounds.com/portal/view/563665) |
| Sonic RPG Eps 10 | [1.3.6](https://archive.org/details/sonic-rpg-10) |
| Mega Man X Virus Mission | [Original game](https://archive.org/details/2667-mega-man-x-virus-mission), not Virus Mission 2 |
| Takeover | [Flash version](https://archive.org/details/takeover-flash-game) |

MMS files were located through the Flashpoint database and downloaded directly
from Newgrounds' uploads. Archive records identify the other downloaded files;
their provenance is not a claim of publisher authentication. See game-inputs.json
for exact download URLs, byte counts, SWF versions and SHA-256 hashes.

All eight downloaded SWFs have valid signatures and their decompressed lengths
match their headers. Sonic RPG 10 is 284,501,034 bytes (SWF 15); the current 64 MiB
guest is not a suitable basis for assuming this game will fit in memory.

Game files are kept separately in fp32/test-games. The player development ZIP
includes this source manifest, not copies of the games. Test order is SM63, X6,
MMS, Sonic RPG 10, Mega Man X Virus Mission, Takeover. Full gameplay, audio,
save persistence and performance remain to be checked.
