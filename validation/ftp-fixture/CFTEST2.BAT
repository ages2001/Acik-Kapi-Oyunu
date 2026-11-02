@echo off
C:\RLTTEST\PMTEST4.EXE startup ConflictBoot
if errorlevel 1 goto fail
C:\RLTTEST\PMTEST4.EXE execute [CreateGroup("Startup",0)]
if errorlevel 1 goto fail
C:\RLTTEST\PMTEST4.EXE query Startup>C:\RLTTEST\DB0.TXT
if errorlevel 1 goto fail
C:\RLTTEST\PMTEST4.EXE execute [CreateGroup("ConflictBoot",0)]
if errorlevel 1 goto fail
C:\RLTTEST\PMTEST4.EXE query ConflictBoot>C:\RLTTEST\DC0.TXT
if errorlevel 1 goto fail
C:\RLTTEST\PMTEST4.EXE sync C:\WINNT31\SYSTEM32\RLTSETUP.DLL>C:\RLTTEST\DFAIL.TXT
if errorlevel 10 goto fail
if not errorlevel 9 goto fail
C:\RLTTEST\PMTEST4.EXE execute [CreateGroup("Startup",0)]
if errorlevel 1 goto fail
C:\RLTTEST\PMTEST4.EXE query Startup>C:\RLTTEST\DB1.TXT
if errorlevel 1 goto fail
C:\RLTTEST\PMTEST4.EXE execute [CreateGroup("ConflictBoot",0)]
if errorlevel 1 goto fail
C:\RLTTEST\PMTEST4.EXE query ConflictBoot>C:\RLTTEST\DC1.TXT
if errorlevel 1 goto fail
echo PASS expected conflict refusal>C:\RLTTEST\DRESULT.TXT
goto end
:fail
echo FAIL conflict test>C:\RLTTEST\DRESULT.TXT
:end
type C:\RLTTEST\DRESULT.TXT
