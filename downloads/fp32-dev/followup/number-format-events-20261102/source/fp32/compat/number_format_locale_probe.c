#define FP_FORCE_LEGACY_FORMAT
#include "../../compat/number_format.c"
#include "number_format_locale_fixture.h"
static FormatLocaleFixture fixtures[3];
void WINAPI entry(void){DWORD ids[]={0x409,0x407,0x41f};DWORD i,n,flags=LOCALE_NOUSEROVERRIDE;HANDLE file,log;char text[100];BOOL ok=TRUE;
 for(i=0;i<3;i++){FormatLocaleFixture *v=&fixtures[i];v->locale=ids[i];
 ok=ok&&locale_uint(ids[i],flags,LOCALE_IDIGITS,&v->ndigits)&&locale_uint(ids[i],flags,LOCALE_ILZERO,&v->nleading)&&negative_order(ids[i],flags,&v->nnegative)&&locale_group(ids[i],flags,LOCALE_SGROUPING,&v->ngroup)&&locale_text(ids[i],flags,LOCALE_SDECIMAL,v->ndecimal,32)&&locale_text(ids[i],flags,LOCALE_STHOUSAND,v->nthousand,32);
 ok=ok&&locale_uint(ids[i],flags,LOCALE_ICURRDIGITS,&v->cdigits)&&locale_uint(ids[i],flags,LOCALE_ILZERO,&v->cleading)&&locale_uint(ids[i],flags,LOCALE_INEGCURR,&v->cnegative)&&locale_uint(ids[i],flags,LOCALE_ICURRENCY,&v->cpositive)&&locale_group(ids[i],flags,LOCALE_SMONGROUPING,&v->cgroup)&&locale_text(ids[i],flags,LOCALE_SMONDECIMALSEP,v->cdecimal,32)&&locale_text(ids[i],flags,LOCALE_SMONTHOUSANDSEP,v->cthousand,32)&&locale_text(ids[i],flags,LOCALE_SCURRENCY,v->symbol,64);}
 file=CreateFileA("C:\\FP32\\NLSSPEC.BIN",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(file==INVALID_HANDLE_VALUE)ok=FALSE;else{ok=ok&&WriteFile(file,fixtures,sizeof(fixtures),&n,0)&&n==sizeof(fixtures);CloseHandle(file);}
 log=CreateFileA("C:\\FP32\\NLSSPEC.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(log!=INVALID_HANDLE_VALUE){wsprintfA(text,"Locales 3\r\nFixture bytes %lu\r\nFailures %lu\r\n",sizeof(fixtures),ok?0:1);WriteFile(log,text,lstrlenA(text),&n,0);CloseHandle(log);}ExitProcess(ok?0:1);}
