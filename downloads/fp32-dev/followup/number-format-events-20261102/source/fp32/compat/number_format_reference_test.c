/* Compare the forced NT3.1 implementation against the original NT3.51 API. */
#ifdef FP_TEST_PRODUCTION
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static int(WINAPI*fp_GetNumberFormatW)(LCID,DWORD,LPCWSTR,const NUMBERFMTW*,LPWSTR,int);
static int(WINAPI*fp_GetCurrencyFormatW)(LCID,DWORD,LPCWSTR,const CURRENCYFMTW*,LPWSTR,int);
#if FP_TEST_PRODUCTION == 10
#define PROXY "C:\\FP32\\NF10K.DLL"
#define LOG "C:\\FP32\\NUM10.LOG"
#else
#define PROXY "C:\\FP32\\NF32K.DLL"
#define LOG "C:\\FP32\\NUM32.LOG"
#endif
#else
#define FP_FORCE_LEGACY_FORMAT
#include "../../compat/number_format.c"
#define LOG "C:\\FP32\\FMTREF.LOG"
#endif
#include "number_format_locale_fixture.h"
static FormatLocaleFixture fixtures[3];static BOOL fixture_loaded;
static HANDLE logh,reference;static BOOL native_reference;static DWORD checks,failures;
static int WINAPI read_reference(LCID l,DWORD f,LPCWSTR v,const void *fmt,LPWSTR out,int cap){DWORD h[3],n;if(!ReadFile(reference,h,sizeof(h),&n,0)||n!=sizeof(h)||h[2]>256){SetLastError(13);return -1;}if(!ReadFile(reference,out,h[2]*2,&n,0)||n!=h[2]*2){SetLastError(13);return -1;}SetLastError(h[1]);return (int)h[0];}
static WCHAR a[256],b[256];
static void line(const char*s,DWORD n){char z[256];DWORD w;wsprintfA(z,"%s %lu\r\n",s,n);WriteFile(logh,z,lstrlenA(z),&w,0);FlushFileBuffers(logh);}
static void compare(int actual,int expected,DWORD err,DWORD native_err,DWORD index){
 if(native_reference){DWORD h[3],n;h[0]=expected;h[1]=native_err;h[2]=lstrlenW(b)+1;WriteFile(reference,h,sizeof(h),&n,0);WriteFile(reference,b,h[2]*2,&n,0);}
 checks++;if(actual!=expected||(!actual&&err!=native_err)||(actual&&lstrcmpW(a,b))){char x[800],sa[256],sb[256];DWORD w;failures++;
 WideCharToMultiByte(CP_ACP,0,a,-1,sa,256,0,0);WideCharToMultiByte(CP_ACP,0,b,-1,sb,256,0,0);
 wsprintfA(x,"Mismatch %lu actual %d/%lu [%s] native %d/%lu [%s]\r\n",index,actual,err,sa,expected,native_err,sb);WriteFile(logh,x,lstrlenA(x),&w,0);FlushFileBuffers(logh);}}
void WINAPI entry(void){
 typedef int(WINAPI*Number)(LCID,DWORD,LPCWSTR,const NUMBERFMTW*,LPWSTR,int);
 typedef int(WINAPI*Currency)(LCID,DWORD,LPCWSTR,const CURRENCYFMTW*,LPWSTR,int);
 Number native_number=(void*)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),"GetNumberFormatW");
 Currency native_currency=(void*)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),"GetCurrencyFormatW");
 const WCHAR *values[]={L"0",L"-0",L"0.004",L"-0.004",L"0.005",L"-0.005",L"999.995",L"-999.995",L"123456789.987654321",L"00001234.56000",L".5",L"1.",L"",L".",L"+1",L"--1",L"1,2",L"1.2.3",L"-000.000",L"123",L"1234"};
 UINT groups[]={0,3,1,2,4,9,5,101};LCID locales[]={0x409,0x407,0x41f};
 NUMBERFMTW f;CURRENCYFMTW c;HANDLE spec;DWORD got;DWORD i,j,k,l,index=0,e,ne;int n,nn;
 logh=CreateFileA(LOG,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
#ifdef FP_TEST_PRODUCTION
 {HMODULE proxy=LoadLibraryA(PROXY);if(proxy){fp_GetNumberFormatW=(void*)GetProcAddress(proxy,"GetNumberFormatW");fp_GetCurrencyFormatW=(void*)GetProcAddress(proxy,"GetCurrencyFormatW");}if(!fp_GetNumberFormatW||!fp_GetCurrencyFormatW){line("Production proxy failed to load",GetLastError());CloseHandle(logh);ExitProcess(1);}}
#endif
 native_reference=native_number&&native_currency;
 reference=CreateFileA("C:\\FP32\\FMTBASE.BIN",native_reference?GENERIC_WRITE:GENERIC_READ,FILE_SHARE_READ,0,native_reference?CREATE_ALWAYS:OPEN_EXISTING,0,0);
 if(reference==INVALID_HANDLE_VALUE){line("Reference file error",GetLastError());CloseHandle(logh);ExitProcess(1);}
 if(!native_reference){native_number=(Number)read_reference;native_currency=(Currency)read_reference;}
 spec=CreateFileA("C:\\FP32\\NLSSPEC.BIN",GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);if(spec!=INVALID_HANDLE_VALUE){fixture_loaded=ReadFile(spec,fixtures,sizeof(fixtures),&got,0)&&got==sizeof(fixtures);CloseHandle(spec);}
 line("Recorded NT3.1 locale inputs",fixture_loaded);
 f.NumDigits=2;f.LeadingZero=1;f.Grouping=3;f.lpDecimalSep=L".";f.lpThousandSep=L",";f.NegativeOrder=1;
 for(k=0;k<3;k++)for(j=0;j<8;j++)for(i=0;i<21;i++){
  f.Grouping=groups[j];a[0]=b[0]=0;SetLastError(0);n=fp_GetNumberFormatW(locales[k],0,values[i],&f,a,256);e=GetLastError();SetLastError(0);nn=native_number(locales[k],0,values[i],&f,b,256);ne=GetLastError();compare(n,nn,e,ne,index++);}
 f.Grouping=3;
 for(k=0;k<5;k++)for(j=0;j<2;j++)for(i=0;i<10;i++){
  f.NegativeOrder=k;f.LeadingZero=j;f.NumDigits=i;a[0]=b[0]=0;SetLastError(0);n=fp_GetNumberFormatW(0x409,0,L"-0.00995",&f,a,256);e=GetLastError();SetLastError(0);nn=native_number(0x409,0,L"-0.00995",&f,b,256);ne=GetLastError();compare(n,nn,e,ne,index++);}
 c.NumDigits=2;c.LeadingZero=1;c.Grouping=3;c.lpDecimalSep=L".";c.lpThousandSep=L",";c.lpCurrencySymbol=L"XYZ";
 for(k=0;k<16;k++)for(j=0;j<4;j++)for(i=0;i<10;i++){
  c.NegativeOrder=k;c.PositiveOrder=j;a[0]=b[0]=0;SetLastError(0);n=fp_GetCurrencyFormatW(0x409,0,values[i],&c,a,256);e=GetLastError();SetLastError(0);nn=native_currency(0x409,0,values[i],&c,b,256);ne=GetLastError();compare(n,nn,e,ne,index++);}
 for(k=0;k<3;k++)for(i=0;i<10;i++){
  BOOL explicit_input=native_reference&&fixture_loaded;DWORD flags=explicit_input?0:LOCALE_NOUSEROVERRIDE;
  if(explicit_input){number_fixture(&fixtures[k],&f);currency_fixture(&fixtures[k],&c);}
  a[0]=b[0]=0;SetLastError(0);n=fp_GetNumberFormatW(locales[k],flags,values[i],explicit_input?&f:0,a,256);e=GetLastError();SetLastError(0);nn=native_number(locales[k],flags,values[i],explicit_input?&f:0,b,256);ne=GetLastError();compare(n,nn,e,ne,index++);
  a[0]=b[0]=0;SetLastError(0);n=fp_GetCurrencyFormatW(locales[k],flags,values[i],explicit_input?&c:0,a,256);e=GetLastError();SetLastError(0);nn=native_currency(locales[k],flags,values[i],explicit_input?&c:0,b,256);ne=GetLastError();compare(n,nn,e,ne,index++);}
 /* Invalid format fields, separator validation and buffer-size contracts. */
 for(k=0;k<34;k++){
  DWORD flags=0;int cap=256;f.NumDigits=2;f.LeadingZero=1;f.Grouping=3;f.NegativeOrder=1;f.lpDecimalSep=L".";f.lpThousandSep=L",";
  if(k<5)f.NumDigits=k==4?0xffffffff:k*3;
  else if(k<8)f.LeadingZero=k-5;
  else if(k<15)f.NegativeOrder=k-8;
  else if(k<19){static WCHAR *seps[]={L"",L"::",L"abcd",L"abcde"};f.lpDecimalSep=seps[k-15];}
  else if(k<23){static WCHAR *seps[]={L"",L"::",L"123",L"abcde"};f.lpThousandSep=seps[k-19];}
  else if(k==23)flags=1;
  else if(k==24)flags=LOCALE_NOUSEROVERRIDE;
  else cap=(int)k-25;
  a[0]=b[0]=0;SetLastError(0);n=fp_GetNumberFormatW(0x409,flags,L"-1234.567",&f,a,cap);e=GetLastError();SetLastError(0);nn=native_number(0x409,flags,L"-1234.567",&f,b,cap);ne=GetLastError();compare(n,nn,e,ne,index++);
 }
 for(k=0;k<12;k++){
  c.NumDigits=2;c.LeadingZero=1;c.Grouping=3;c.NegativeOrder=1;c.PositiveOrder=0;c.lpDecimalSep=L".";c.lpThousandSep=L",";c.lpCurrencySymbol=L"XYZ";
  if(k<6){static WCHAR *symbols[]={L"",L"$",L"abcdef",L"abcdefg",L"123",L"a b"};c.lpCurrencySymbol=symbols[k];}
  else if(k<9)c.PositiveOrder=k-3;
  else c.NegativeOrder=k+6;
  a[0]=b[0]=0;SetLastError(0);n=fp_GetCurrencyFormatW(0x409,0,L"-1234.567",&c,a,256);e=GetLastError();SetLastError(0);nn=native_currency(0x409,0,L"-1234.567",&c,b,256);ne=GetLastError();compare(n,nn,e,ne,index++);
 }
 line("Checks",checks);line("Failures",failures);CloseHandle(reference);CloseHandle(logh);ExitProcess(failures?2:0);
}
