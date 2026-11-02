@echo off
C:\RLTTEST\PMTEST4.EXE startup RltBoot
if errorlevel 1 goto fail
C:\RLTTEST\PMTEST4.EXE twice C:\WINNT31\SYSTEM32\RLTSETUP.DLL>C:\RLTTEST\CUSTOM.TXT
if errorlevel 1 goto fail
C:\RLTTEST\PMTEST4.EXE execute [CreateGroup("Startup",0)]
if errorlevel 1 goto fail
C:\RLTTEST\PMTEST4.EXE query Startup>C:\RLTTEST\CSTART.TXT
if errorlevel 1 goto fail
C:\RLTTEST\PMTEST4.EXE execute [CreateGroup("ConflictBoot",0)]
if errorlevel 1 goto fail
C:\RLTTEST\PMTEST4.EXE query ConflictBoot>C:\RLTTEST\CCONF.TXT
if errorlevel 1 goto fail
C:\RLTTEST\PMTEST4.EXE execute [CreateGroup("RltBoot",0)]
if errorlevel 1 goto fail
C:\RLTTEST\PMTEST4.EXE query RltBoot>C:\RLTTEST\CBOOT.TXT
if errorlevel 1 goto fail
echo PASS custom Startup repeat>C:\RLTTEST\CREPEAT.TXT
goto end
:fail
echo FAIL custom Startup repeat>C:\RLTTEST\CREPEAT.TXT
:end
type C:\RLTTEST\CREPEAT.TXT
