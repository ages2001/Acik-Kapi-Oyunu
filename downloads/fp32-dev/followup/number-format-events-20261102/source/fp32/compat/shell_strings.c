/* Unicode substring helpers for early Windows without SHLWAPI. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static WCHAR fold(WCHAR c){return (WCHAR)(ULONG_PTR)CharUpperW((LPWSTR)(ULONG_PTR)c);}
static BOOL starts(LPCWSTR text,LPCWSTR needle){while(*needle){if(!*text||fold(*text)!=fold(*needle))return FALSE;text++;needle++;}return TRUE;}
LPWSTR WINAPI fp_StrStrIW(LPCWSTR text,LPCWSTR needle){if(!text||!needle)return 0;while(*text){if(starts(text,needle))return (LPWSTR)text;text++;}return 0;}
LPWSTR WINAPI fp_StrRStrIW(LPCWSTR text,LPCWSTR end,LPCWSTR needle){LPWSTR result=0;if(!text||!needle)return 0;if(!end)end=text+lstrlenW(text);while(*text&&text<end){if(starts(text,needle))result=(LPWSTR)text;text++;}return result;}
