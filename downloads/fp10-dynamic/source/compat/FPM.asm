bits 32
section .text
extern _FpMissing@8
global _fn4
_fn4:
mov eax,[esp]
push eax
push str4
call _FpMissing@8
int3
section .rdata
str4: db 'WINMM.dll!mixerGetID',0
section .text
global _fn9
_fn9:
mov eax,[esp]
push eax
push str9
call _FpMissing@8
int3
section .rdata
str9: db 'WINMM.dll!mixerClose',0
section .text
global _fn10
_fn10:
mov eax,[esp]
push eax
push str10
call _FpMissing@8
int3
section .rdata
str10: db 'WINMM.dll!mixerGetLineControlsA',0
section .text
global _fn11
_fn11:
mov eax,[esp]
push eax
push str11
call _FpMissing@8
int3
section .rdata
str11: db 'WINMM.dll!mixerGetLineInfoA',0
section .text
global _fn12
_fn12:
mov eax,[esp]
push eax
push str12
call _FpMissing@8
int3
section .rdata
str12: db 'WINMM.dll!mixerGetDevCapsA',0
section .text
global _fn13
_fn13:
mov eax,[esp]
push eax
push str13
call _FpMissing@8
int3
section .rdata
str13: db 'WINMM.dll!mixerOpen',0
section .text
global _fn14
_fn14:
mov eax,[esp]
push eax
push str14
call _FpMissing@8
int3
section .rdata
str14: db 'WINMM.dll!mixerGetControlDetailsA',0
section .text
global _fn18
_fn18:
mov eax,[esp]
push eax
push str18
call _FpMissing@8
int3
section .rdata
str18: db 'WINMM.dll!mixerSetControlDetails',0
section .text
