# Flash Player legacy port — R2 development snapshot

This is an unfinished development snapshot, not a completed port.
The original archive contains the NT 3.1 development setup, its exact runtime
payload, separate experimental runtime, sources, reports, screenshots, supplied
player inputs/drivers, and the 13 successfully transferred SWF samples.

The installer is currently NT 3.1 only. The separate experimental graphics
runtime passed 24 guest checks. Mario input and performance remain unresolved.
GPU acceleration, the other operating-system ports, and the FP32 port are not
implemented. Read `FP10WORK/README.TXT` inside the ZIP before use.

The ZIP is split here only to fit GitHub's per-file Git limit; no archive bytes
were removed. The publication workflow reconstructs and verifies it, then uploads
one `FP10R2.zip` release asset as a prerelease. `RELEASE.json`, when present,
records the asset URL and the workflow's successful download/hash verification.

For manual reconstruction on a modern Windows host, download all five `.001`
through `.005` files and `JOIN.cmd` into one directory, then run `JOIN.cmd`.
On Linux/macOS, run:

```sh
cat FP10R2.zip.001 FP10R2.zip.002 FP10R2.zip.003 FP10R2.zip.004 FP10R2.zip.005 > FP10R2.zip
sha256sum -c SHA256.txt
```

Expected size: 130252841 bytes.
SHA-256: `40422b8d13510f272d979844b731ba5261cbabddefdaf055618836a00b85af94`.
