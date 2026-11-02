bits 32
section .text
extern _FpMissing@8
global _fn58
_fn58:
mov eax,[esp]
push eax
push str58
call _FpMissing@8
int3
section .rdata
str58: db 'KERNEL32.dll!QueueUserAPC',0
section .text
global _fn70
_fn70:
mov eax,[esp]
push eax
push str70
call _FpMissing@8
int3
section .rdata
str70: db 'KERNEL32.dll!GetNumberFormatW',0
section .text
global _fn71
_fn71:
mov eax,[esp]
push eax
push str71
call _FpMissing@8
int3
section .rdata
str71: db 'KERNEL32.dll!GetCurrencyFormatW',0
section .text
global _fn76
_fn76:
mov eax,[esp]
push eax
push str76
call _FpMissing@8
int3
section .rdata
str76: db 'KERNEL32.dll!EnumSystemLocalesW',0
section .text
global _fn184
_fn184:
mov eax,[esp]
push eax
push str184
call _FpMissing@8
int3
section .rdata
str184: db 'KERNEL32.dll!CancelWaitableTimer',0
section .text
global _fn185
_fn185:
mov eax,[esp]
push eax
push str185
call _FpMissing@8
int3
section .rdata
str185: db 'KERNEL32.dll!SetWaitableTimer',0
section .text
global _fn186
_fn186:
mov eax,[esp]
push eax
push str186
call _FpMissing@8
int3
section .rdata
str186: db 'KERNEL32.dll!CreateWaitableTimerA',0
section .text
