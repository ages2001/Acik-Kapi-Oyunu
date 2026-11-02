# ANSI version verification follow-up

VerifyVersionInfoA now evaluates the requested version, build, platform, service pack, suite and product-type conditions through the existing Unicode condition evaluator. It preserves the numeric fields, does not inspect the ignored CSD text, and reports normal invalid-parameter or older-version errors.

The exact packaged F32K.DLL passed 20 native checks on each of NT3.1 UP and NT3.51 UP. Checks cover current/future versions, hierarchical major/minor comparisons, build comparison, platform/product/service-pack mismatch, AND/OR suite semantics, ignored unterminated CSD bytes, input preservation and invalid structures/masks/operators. ANSI and Unicode results agree for the actual OS version. Test alias V32K.DLL is byte-for-byte identical.

Current diagnostic import counts: **FP32 87 / FP10 53**. Neither port is complete; partial behavior, dynamically resolved APIs and untested OS support are excluded. The prior mixer checkpoint remains available separately. This ZIP updates only the kernel component and includes its source snapshot and native probe. Use disposable installations and preserve existing files; do not replace Windows system DLLs. It is not a complete installer. Full gameplay regression with the accumulated components is separate and not certified by these API tests.

Build in the prepared LLVM/MinGW/pefile workspace with the original reference inputs:

```
python fp32/analysis/build_nt31.py
python analysis/build_guest_probe.py fp32/compat/version_verify_test.c fp32/build/VERANSI.EXE
```

Copy F32K.DLL as C:\FP32\V32K.DLL on the test guest, run VERANSI.EXE and inspect C:\FP32\VERANSI.LOG.
