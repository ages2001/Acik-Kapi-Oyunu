bits 32
section .text
extern _FpMissing@8
global _fn11
_fn11:
mov eax,[esp]
push eax
push str11
call _FpMissing@8
int3
section .rdata
str11: db 'GDI32.dll!GetICMProfileA',0
section .text
global _fn58
_fn58:
mov eax,[esp]
push eax
push str58
call _FpMissing@8
int3
section .rdata
str58: db 'GDI32.dll!EnumFontFamiliesExW',0
section .text
