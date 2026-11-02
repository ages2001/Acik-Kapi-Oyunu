#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef FP_TEST_FP10
#define PROXY "C:\\FP32\\NF10K.DLL"
#define LOG "C:\\FP32\\FMTC10.LOG"
#else
#define PROXY "C:\\FP32\\NF32K.DLL"
#define LOG "C:\\FP32\\FMTC32.LOG"
#endif
static HANDLE logh;static DWORD checks,failures;
static void check(BOOL ok,const char *name){char b[200];DWORD n;checks++;if(!ok)failures++;wsprintfA(b,"%s %u\r\n",name,ok?1:0);WriteFile(logh,b,lstrlenA(b),&n,0);FlushFileBuffers(logh);}
void WINAPI entry(void){
 HMODULE dll;int(WINAPI*number)(LCID,DWORD,LPCWSTR,const NUMBERFMTW*,LPWSTR,int);int(WINAPI*currency)(LCID,DWORD,LPCWSTR,const CURRENCYFMTW*,LPWSTR,int);
 NUMBERFMTW f;CURRENCYFMTW c;WCHAR out[160];int n;DWORD used;char text[120];HKEY key;BYTE original[256];DWORD original_size,original_type;LONG old;BOOL changed=FALSE;
 logh=CreateFileA(LOG,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 dll=LoadLibraryA(PROXY);check(dll!=0,"Production kernel proxy loads");if(!dll)goto done;
 number=(void*)GetProcAddress(dll,"GetNumberFormatW");currency=(void*)GetProcAddress(dll,"GetCurrencyFormatW");check(number&&currency,"Number and currency exports resolve");if(!number||!currency)goto done;
 f.NumDigits=2;f.LeadingZero=1;f.Grouping=32;f.lpDecimalSep=L".";f.lpThousandSep=L",";f.NegativeOrder=1;
 n=number(0x409,0,L"12345678.90",&f,out,160);check(n==lstrlenW(L"1,23,45,678.90")+1&&!lstrcmpW(out,L"1,23,45,678.90"),"Modern grouping repeats final group");
 f.Grouping=320;check(number(0x409,0,L"12345678.90",&f,out,160)&&!lstrcmpW(out,L"123,45,678.90"),"Terminal zero stops further grouping");
 f.Grouping=30;check(number(0x409,0,L"12345678.90",&f,out,160)&&!lstrcmpW(out,L"12345,678.90"),"Single nonrepeating group");
 f.Grouping=0;check(number(0x409,0,L"12345678.90",&f,out,160)&&!lstrcmpW(out,L"12345678.90"),"Zero grouping has no separators");
 f.Grouping=3;check(number(0x409,0,L"123456789012345678901234567890.995",&f,out,160)&&!lstrcmpW(out,L"123,456,789,012,345,678,901,234,567,891.00"),"Long decimal rounds without floating point loss");
 f.LeadingZero=0;check(number(0x409,0,L"0.05",&f,out,160)&&!lstrcmpW(out,L".05"),"Leading zero can be omitted");
 f.LeadingZero=1;check(number(0x409,0,L"-0.004",&f,out,160)&&!lstrcmpW(out,L"0.00"),"Rounded zero has no negative sign");
 check(number(0x409,0,L"999.995",&f,out,160)&&!lstrcmpW(out,L"1,000.00"),"Fraction carry propagates into grouped integer");
 check(number(0x409,0,L"1234.567",&f,0,0)==9,"Size query includes terminator");
 SetLastError(0);check(!number(0x409,0,L"1234.567",&f,out,8)&&GetLastError()==ERROR_INSUFFICIENT_BUFFER,"Short output buffer is rejected");
 SetLastError(0);check(!number(0x409,0,0,&f,out,160)&&GetLastError()==ERROR_INVALID_PARAMETER,"Null numeric input is rejected");
 SetLastError(0);check(!number(0x409,0,L"1",&f,0,1)&&GetLastError()==ERROR_INVALID_PARAMETER,"Null nonempty output buffer is rejected");
 SetLastError(0);check(!number(0x409,0,L"1",&f,out,-1)&&GetLastError()==ERROR_INVALID_PARAMETER,"Negative capacity is rejected");
 f.Grouping=101;check(!number(0x409,0,L"1234",&f,out,160)&&GetLastError()==ERROR_INVALID_PARAMETER,"Interior zero grouping is rejected");f.Grouping=3;
 c.NumDigits=2;c.LeadingZero=1;c.Grouping=32;c.lpDecimalSep=L".";c.lpThousandSep=L",";c.NegativeOrder=0;c.PositiveOrder=0;c.lpCurrencySymbol=L"\x20b9";
 check(currency(0x409,0,L"-1234567.895",&c,out,160)&&!lstrcmpW(out,L"(\x20b9" L"12,34,567.90)"),"Unicode currency symbol and modern grouping");
 c.Grouping=320;c.PositiveOrder=3;check(currency(0x409,0,L"12345678.90",&c,out,160)&&!lstrcmpW(out,L"123,45,678.90 \x20b9"),"Currency nonrepeating groups and symbol placement");
 c.lpCurrencySymbol=0;check(!currency(0x409,0,L"1",&c,out,160)&&GetLastError()==ERROR_INVALID_PARAMETER,"Null currency symbol is rejected");
 /* Only the disposable NT3.1 guest needs the missing-field registry fallback. */
 if((GetVersion()&0xffff)==0x0a03&&(GetUserDefaultLCID()&0xffff)==0x409){
  old=RegOpenKeyExW(HKEY_CURRENT_USER,L"Control Panel\\International",0,KEY_QUERY_VALUE|KEY_SET_VALUE,&key);check(old==0,"Open current-user international settings");
  if(!old){original_size=sizeof(original);old=RegQueryValueExW(key,L"iNegNumber",0,&original_type,original,&original_size);check(old==0||old==ERROR_FILE_NOT_FOUND,"Preserve exact prior negative-order value");
   if(old==0||old==ERROR_FILE_NOT_FOUND){WCHAR replacement[2]={'2',0};changed=RegSetValueExW(key,L"iNegNumber",0,REG_SZ,(BYTE*)replacement,sizeof(replacement))==0;check(changed,"Set temporary negative-order override");
    if(changed){check(number(0x409,0,L"-12.34",0,out,160)&&!lstrcmpW(out,L"- 12.34"),"Missing NT3.1 field honors user override");check(number(0x409,LOCALE_NOUSEROVERRIDE,L"-12.34",0,out,160)&&!lstrcmpW(out,L"-12.34"),"NOUSEROVERRIDE uses measured locale default");
     check((old==ERROR_FILE_NOT_FOUND?RegDeleteValueW(key,L"iNegNumber"):RegSetValueExW(key,L"iNegNumber",0,original_type,original,original_size))==0,"Restore original international setting");}
   }RegCloseKey(key);}
 }
 done:wsprintfA(text,"Checks %lu\r\nFailures %lu\r\n",checks,failures);WriteFile(logh,text,lstrlenA(text),&used,0);if(dll)FreeLibrary(dll);CloseHandle(logh);ExitProcess(failures?2:0);
}
