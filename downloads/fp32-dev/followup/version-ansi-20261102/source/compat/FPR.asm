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
str0: db 'urlmon.dll!CopyStgMedium',0
section .text
