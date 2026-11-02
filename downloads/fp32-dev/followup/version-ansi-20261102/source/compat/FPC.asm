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
str0: db 'CRYPT32.dll!CryptGetMessageCertificates',0
section .text
global _fn1
_fn1:
mov eax,[esp]
push eax
push str1
call _FpMissing@8
int3
section .rdata
str1: db 'CRYPT32.dll!CertCreateCertificateContext',0
section .text
global _fn2
_fn2:
mov eax,[esp]
push eax
push str2
call _FpMissing@8
int3
section .rdata
str2: db 'CRYPT32.dll!CertFindCertificateInStore',0
section .text
global _fn3
_fn3:
mov eax,[esp]
push eax
push str3
call _FpMissing@8
int3
section .rdata
str3: db 'CRYPT32.dll!CertVerifySubjectCertificateContext',0
section .text
global _fn4
_fn4:
mov eax,[esp]
push eax
push str4
call _FpMissing@8
int3
section .rdata
str4: db 'CRYPT32.dll!CertFreeCertificateContext',0
section .text
global _fn5
_fn5:
mov eax,[esp]
push eax
push str5
call _FpMissing@8
int3
section .rdata
str5: db 'CRYPT32.dll!CertCloseStore',0
section .text
global _fn6
_fn6:
mov eax,[esp]
push eax
push str6
call _FpMissing@8
int3
section .rdata
str6: db 'CRYPT32.dll!CryptVerifyMessageSignature',0
section .text
