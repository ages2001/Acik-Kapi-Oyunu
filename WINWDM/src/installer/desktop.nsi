Unicode false
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "WinVer.nsh"
!include "nsDialogs.nsh"
!include "FileFunc.nsh"
Var Target
Var TargetBox
Name "WINWDM"
OutFile "../build/DESKTOP/SETUP.EXE"
InstallDir "$WINDIR\WINWDM"
RequestExecutionLevel admin
SetCompressor zlib
!define MUI_ICON "../resources/kit.ico"
!define MUI_UNICON "../resources/remove.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "../resources/wizard.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "../resources/wizard.bmp"
!define MUI_WELCOMEPAGE_TITLE "WINWDM"
!define MUI_WELCOMEPAGE_TEXT "Install the WDM software driver with bitmap, filter and cancel-safe queue support.$\r$\n$\r$\nOpen WINWDM Info from the desktop. Use Add/Remove Programs to remove the kit."
!insertmacro MUI_PAGE_WELCOME
Page custom TargetPage TargetLeave
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_TEXT "Open WINWDM Info from the desktop to check the driver. Windows 95 and 98 need a restart to load the VxD."
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!define ARP "Software\Microsoft\Windows\CurrentVersion\Uninstall\WdmKit"
!define VXD "System\CurrentControlSet\Services\VxD\WDMKIT"
Function .onInit
  StrCpy $Target "6.0"
  ${GetParameters} $0
  ClearErrors
  ${GetOptions} $0 "/WDM=" $1
  IfErrors done
  StrCmp $1 "1.00" valid
  StrCmp $1 "1.05" valid
  StrCmp $1 "1.10" valid
  StrCmp $1 "1.20" valid
  StrCmp $1 "1.30" valid
  StrCmp $1 "6.0" valid
  MessageBox MB_ICONSTOP|MB_OK "Unknown WDM version. Choose 1.00, 1.05, 1.10, 1.20, 1.30 or 6.0." /SD IDOK
  SetErrorLevel 2
  Abort
  valid:
    StrCpy $Target $1
  done:
FunctionEnd

Function TargetPage
  !insertmacro MUI_HEADER_TEXT "WDM version" "Choose the WDM target for this computer."
  nsDialogs::Create 1018
  Pop $0
  StrCmp $0 error 0 +2
    Abort
  ${NSD_CreateLabel} 0 5u 100% 11u "WDM target version"
  Pop $0
  ${NSD_CreateDropList} 0 19u 65% 90u ""
  Pop $TargetBox
  ${NSD_CB_AddString} $TargetBox "1.00"
  ${NSD_CB_AddString} $TargetBox "1.05"
  ${NSD_CB_AddString} $TargetBox "1.10"
  ${NSD_CB_AddString} $TargetBox "1.20"
  ${NSD_CB_AddString} $TargetBox "1.30"
  ${NSD_CB_AddString} $TargetBox "6.0"
  ${NSD_CB_SelectString} $TargetBox $Target
  ${NSD_CreateLabel} 0 44u 100% 32u "The 1.x targets use smaller startup checks. Open WINWDM Info after installation to see the active target."
  Pop $0
  nsDialogs::Show
FunctionEnd

Function TargetLeave
  ${NSD_GetText} $TargetBox $Target
FunctionEnd

!macro TargetDriver dir extension
  ${If} $Target == "1.00"
    File /oname=WINWDM.${extension} "../build/${dir}/W100.${extension}"
  ${ElseIf} $Target == "1.05"
    File /oname=WINWDM.${extension} "../build/${dir}/W105.${extension}"
  ${ElseIf} $Target == "1.10"
    File /oname=WINWDM.${extension} "../build/${dir}/W110.${extension}"
  ${ElseIf} $Target == "1.20"
    File /oname=WINWDM.${extension} "../build/${dir}/W120.${extension}"
  ${ElseIf} $Target == "1.30"
    File /oname=WINWDM.${extension} "../build/${dir}/W130.${extension}"
  ${Else}
    File /oname=WINWDM.${extension} "../build/${dir}/WINWDM.${extension}"
  ${EndIf}
!macroend

Section "WDM software driver and WINWDM Info"
  SectionIn RO
  StrCpy $INSTDIR "$WINDIR\WINWDM"
  ReadRegStr $0 HKLM "${ARP}" "UninstallString"
  StrCmp $0 "" +2
    Abort "An uninstall entry for this kit already exists."
  IfFileExists "$DESKTOP\WINWDM Info.lnk" 0 +2
    Abort "A WINWDM Info desktop shortcut already exists."
  IfFileExists "$INSTDIR\*.*" 0 empty
    MessageBox MB_OK|MB_ICONSTOP "The WINWDM folder is already in use. Remove the installed kit before installing again." /SD IDOK
    Abort
  empty:
  ClearErrors
  SetOutPath "$INSTDIR"
  ClearErrors
  File "../build/NT3/INFO.EXE"
  ${If} ${IsNT}
    !insertmacro TargetDriver NT3 SYS
    File "../build/NT3/NTSVC.EXE"
    IfErrors failed
    ExecWait '"$INSTDIR\NTSVC.EXE" install' $0
    IfErrors failed
    StrCmp $0 0 +2
      Goto failed
  ${Else}
    !insertmacro TargetDriver WIN3 386
    IfErrors failed
    ReadRegStr $0 HKLM "${VXD}" "StaticVxD"
    StrCmp $0 "" +2
      Goto failed
    ClearErrors
    WriteRegStr HKLM "${VXD}" "StaticVxD" "$INSTDIR\WINWDM.386"
    IfErrors failed
  ${EndIf}
  ClearErrors
  WriteUninstaller "$INSTDIR\UNINST.EXE"
  CreateShortcut "$DESKTOP\WINWDM Info.lnk" "$INSTDIR\INFO.EXE" "" "$INSTDIR\INFO.EXE" 0
  WriteRegStr HKLM "${ARP}" "DisplayName" "WINWDM"
  WriteRegStr HKLM "${ARP}" "UninstallString" '"$INSTDIR\UNINST.EXE"'
  WriteRegStr HKLM "${ARP}" "DisplayIcon" "$INSTDIR\UNINST.EXE,0"
  IfErrors rollback
  Goto finished
  rollback:
    Delete "$DESKTOP\WINWDM Info.lnk"
    DeleteRegKey HKLM "${ARP}"
    ${If} ${IsNT}
      ExecWait '"$INSTDIR\NTSVC.EXE" remove' $0
      StrCmp $0 0 +2
        Abort "The service is still registered. Driver files were retained."
    ${Else}
      DeleteRegValue HKLM "${VXD}" "StaticVxD"
      DeleteRegKey /ifempty HKLM "${VXD}"
    ${EndIf}
  failed:
    Delete "$INSTDIR\UNINST.EXE"
    Delete "$INSTDIR\INFO.EXE"
    Delete "$INSTDIR\WINWDM.386"
    Delete "$INSTDIR\WINWDM.SYS"
    Delete "$INSTDIR\NTSVC.EXE"
    RMDir "$INSTDIR"
    Abort "Setup could not install the driver or register the kit."
  finished:
SectionEnd
Section "Uninstall"
  ${If} ${IsNT}
    ClearErrors
    ExecWait '"$INSTDIR\NTSVC.EXE" remove' $0
    IfErrors keep
    StrCmp $0 0 +2
      Goto keep
  ${Else}
    DeleteRegValue HKLM "${VXD}" "StaticVxD"
    DeleteRegKey /ifempty HKLM "${VXD}"
  ${EndIf}
  Delete "$DESKTOP\WINWDM Info.lnk"
  DeleteRegKey HKLM "${ARP}"
  Delete /REBOOTOK "$INSTDIR\INFO.EXE"
  Delete /REBOOTOK "$INSTDIR\WINWDM.386"
  Delete /REBOOTOK "$INSTDIR\WINWDM.SYS"
  Delete /REBOOTOK "$INSTDIR\NTSVC.EXE"
  Delete /REBOOTOK "$INSTDIR\UNINST.EXE"
  RMDir /REBOOTOK "$INSTDIR"
  Goto done
  keep:
    Abort "The driver could not be stopped. Close WINWDM Info and retry."
  done:
SectionEnd
