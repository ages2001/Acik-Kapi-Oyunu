bits 32
section .text
extern _FpMissing@8
global _fn2
_fn2:
mov eax,[esp]
push eax
push str2
call _FpMissing@8
int3
section .rdata
str2: db 'SHELL32.dll!SHGetDiskFreeSpaceExW',0
section .text
global _fn3
_fn3:
mov eax,[esp]
push eax
push str3
call _FpMissing@8
int3
section .rdata
str3: db 'SHELL32.dll!SHGetFolderPathW',0
section .text
global _fn4
_fn4:
mov eax,[esp]
push eax
push str4
call _FpMissing@8
int3
section .rdata
str4: db 'SHELL32.dll!SHGetSpecialFolderLocation',0
section .text
global _fn5
_fn5:
mov eax,[esp]
push eax
push str5
call _FpMissing@8
int3
section .rdata
str5: db 'SHELL32.dll!SHAppBarMessage',0
section .text
global _fn6
_fn6:
mov eax,[esp]
push eax
push str6
call _FpMissing@8
int3
section .rdata
str6: db 'SHELL32.dll!SHBrowseForFolderW',0
section .text
global _fn7
_fn7:
mov eax,[esp]
push eax
push str7
call _FpMissing@8
int3
section .rdata
str7: db 'SHELL32.dll!SHGetPathFromIDListW',0
section .text
global _fn8
_fn8:
mov eax,[esp]
push eax
push str8
call _FpMissing@8
int3
section .rdata
str8: db 'SHELL32.dll!SHGetFolderPathA',0
section .text
