bits 32
section .text
extern _FpMissing@8
global _fn3
_fn3:
mov eax,[esp]
push eax
push str3
call _FpMissing@8
int3
section .rdata
str3: db 'WS2_32.dll!WSASocketW',0
section .text
global _fn5
_fn5:
mov eax,[esp]
push eax
push str5
call _FpMissing@8
int3
section .rdata
str5: db 'WS2_32.dll!WSAIoctl',0
section .text
global _fn13
_fn13:
mov eax,[esp]
push eax
push str13
call _FpMissing@8
int3
section .rdata
str13: db 'WS2_32.dll!WSACloseEvent',0
section .text
global _fn25
_fn25:
mov eax,[esp]
push eax
push str25
call _FpMissing@8
int3
section .rdata
str25: db 'WS2_32.dll!WSAAddressToStringA',0
section .text
global _fn26
_fn26:
mov eax,[esp]
push eax
push str26
call _FpMissing@8
int3
section .rdata
str26: db 'WS2_32.dll!WSACreateEvent',0
section .text
global _fn27
_fn27:
mov eax,[esp]
push eax
push str27
call _FpMissing@8
int3
section .rdata
str27: db 'WS2_32.dll!WSAEventSelect',0
section .text
global _fn28
_fn28:
mov eax,[esp]
push eax
push str28
call _FpMissing@8
int3
section .rdata
str28: db 'WS2_32.dll!WSAEnumNetworkEvents',0
section .text
