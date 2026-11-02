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
str0: db 'mscms.dll!CreateColorTransformW',0
section .text
global _fn1
_fn1:
mov eax,[esp]
push eax
push str1
call _FpMissing@8
int3
section .rdata
str1: db 'mscms.dll!CloseColorProfile',0
section .text
global _fn2
_fn2:
mov eax,[esp]
push eax
push str2
call _FpMissing@8
int3
section .rdata
str2: db 'mscms.dll!TranslateBitmapBits',0
section .text
global _fn3
_fn3:
mov eax,[esp]
push eax
push str3
call _FpMissing@8
int3
section .rdata
str3: db 'mscms.dll!DeleteColorTransform',0
section .text
global _fn4
_fn4:
mov eax,[esp]
push eax
push str4
call _FpMissing@8
int3
section .rdata
str4: db 'mscms.dll!OpenColorProfileW',0
section .text
