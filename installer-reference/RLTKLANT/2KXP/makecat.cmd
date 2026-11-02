@echo off
rem makecat.cmd - regenerate and sign rltklant.cat on Windows (authoritative).
rem
rem The rltklant.cat shipped here is a self-signed TEST catalog built off-box.
rem For a catalog signed with your own certificate, run this on a Windows box
rem that has the WDK (inf2cat) and the SDK (signtool) on PATH.  Run it from
rem this folder (the one holding netrtlk.inf, i386\ and amd64\).
rem
rem 1) inf2cat builds rltklant.cat from the INF for 2000 / XP / XP x64:
inf2cat /driver:. /os:2000,XP_X86,XP_X64,Server2003_X64 /verbose
if errorlevel 1 (echo inf2cat failed & exit /b 1)

rem 2) Create a self-signed code-signing cert the first time (PowerShell):
powershell -Command "if (-not (Get-ChildItem Cert:\CurrentUser\My | ? {$_.Subject -eq 'CN=RLTKLANT Test (ages2001)'})) { New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=RLTKLANT Test (ages2001)' -CertStoreLocation Cert:\CurrentUser\My | Out-Null }"

rem 3) Sign the catalog with that cert (SHA1 for 2000/XP acceptance):
signtool sign /fd sha1 /n "RLTKLANT Test (ages2001)" rltklant.cat
if errorlevel 1 (echo signtool failed & exit /b 1)

echo Done: rltklant.cat signed.
echo To make the signature trusted on the target, import the cert into the
echo Trusted Publishers and Trusted Root stores there (or just accept the
echo "unsigned" prompt - 2000/XP install the driver either way).
