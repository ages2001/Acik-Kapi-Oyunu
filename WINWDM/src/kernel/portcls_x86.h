#ifndef WINWDM_PORTCLS_X86_H
#define WINWDM_PORTCLS_X86_H

#include <ntddk.h>
/* The MinGW kernel headers omit TCHAR from the KS audio volume declaration. */
#if defined(__MINGW32__) && !defined(_TCHAR_DEFINED)
typedef WCHAR TCHAR;
#define _TCHAR_DEFINED
#endif
#include <portcls.h>
#endif
