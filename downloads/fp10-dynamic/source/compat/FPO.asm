bits 32
section .text
extern _FpMissing@8
global _fn0
_fn0:
mov eax,[esp]
push eax
push str0
call _FpMissing@8
int3
section .rdata
str0: db 'ole32.dll!OleFlushClipboard',0
section .text
global _fn1
_fn1:
mov eax,[esp]
push eax
push str1
call _FpMissing@8
int3
section .rdata
str1: db 'ole32.dll!OleIsCurrentClipboard',0
section .text
global _fn2
_fn2:
mov eax,[esp]
push eax
push str2
call _FpMissing@8
int3
section .rdata
str2: db 'ole32.dll!OleSetClipboard',0
section .text
global _fn7
_fn7:
mov eax,[esp]
push eax
push str7
call _FpMissing@8
int3
section .rdata
str7: db 'ole32.dll!ReleaseStgMedium',0
section .text
global _fn8
_fn8:
mov eax,[esp]
push eax
push str8
call _FpMissing@8
int3
section .rdata
str8: db 'ole32.dll!CreateBindCtx',0
section .text
global _fn11
_fn11:
mov eax,[esp]
push eax
push str11
call _FpMissing@8
int3
section .rdata
str11: db 'ole32.dll!OleGetClipboard',0
section .text
global _fn13
_fn13:
mov eax,[esp]
push eax
push str13
call _FpMissing@8
int3
section .rdata
str13: db 'ole32.dll!PropVariantClear',0
section .text
