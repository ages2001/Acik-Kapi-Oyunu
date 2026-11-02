/* ANSI entry points needed by the original WinInet DLL on legacy Unicode NT. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static UINT codepage(LCID locale,DWORD flags){WCHAR text[16];UINT cp=0;int i;
 if(flags&LOCALE_USE_CP_ACP)return CP_ACP;
 if(GetLocaleInfoW(locale,LOCALE_IDEFAULTANSICODEPAGE,text,16))for(i=0;text[i]>='0'&&text[i]<='9';i++)cp=cp*10+text[i]-'0';
 return cp?cp:CP_ACP;}
static int format(BOOL date,LCID locale,DWORD flags,const SYSTEMTIME*time,LPCSTR pattern,LPSTR out,int capacity)
{
 WCHAR*wide_pattern=0,*result=0;UINT cp;int n,bytes,answer=0;DWORD error=0;
 if(capacity<0||(!out&&capacity)){SetLastError(ERROR_INVALID_PARAMETER);return 0;}
 cp=codepage(locale,flags);flags&=~LOCALE_USE_CP_ACP;
 if(pattern){n=MultiByteToWideChar(cp,0,pattern,-1,0,0);if(!n)return 0;
  wide_pattern=HeapAlloc(GetProcessHeap(),0,n*sizeof(WCHAR));if(!wide_pattern){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return 0;}
  if(!MultiByteToWideChar(cp,0,pattern,-1,wide_pattern,n))goto done;}
 n=date?GetDateFormatW(locale,flags,time,wide_pattern,0,0):GetTimeFormatW(locale,flags,time,wide_pattern,0,0);
 if(!n)goto done;
 result=HeapAlloc(GetProcessHeap(),0,n*sizeof(WCHAR));if(!result){SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto done;}
 n=date?GetDateFormatW(locale,flags,time,wide_pattern,result,n):GetTimeFormatW(locale,flags,time,wide_pattern,result,n);
 if(!n)goto done;
 bytes=WideCharToMultiByte(cp,0,result,n,0,0,0,0);if(!bytes)goto done;
 if(!capacity){answer=bytes;goto done;}
 if(capacity<bytes){SetLastError(ERROR_INSUFFICIENT_BUFFER);goto done;}
 answer=WideCharToMultiByte(cp,0,result,n,out,capacity,0,0);
done:error=GetLastError();if(result)HeapFree(GetProcessHeap(),0,result);if(wide_pattern)HeapFree(GetProcessHeap(),0,wide_pattern);if(!answer)SetLastError(error);return answer;
}
int WINAPI fp_GetDateFormatA(LCID locale,DWORD flags,const SYSTEMTIME*time,LPCSTR pattern,LPSTR out,int capacity)
{return format(TRUE,locale,flags,time,pattern,out,capacity);}
int WINAPI fp_GetTimeFormatA(LCID locale,DWORD flags,const SYSTEMTIME*time,LPCSTR pattern,LPSTR out,int capacity)
{return format(FALSE,locale,flags,time,pattern,out,capacity);}
LPSTR WINAPI fp_lstrcpynA(LPSTR out,LPCSTR source,int count)
{int i;if(count<=0)return out;if(!out||!source){SetLastError(ERROR_INVALID_PARAMETER);return 0;}for(i=0;i<count-1&&source[i];i++)out[i]=source[i];out[i]=0;return out;}
