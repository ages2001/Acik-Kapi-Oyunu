/* UTC to local wall time for early NT. Uses integer FILETIME arithmetic;
   recurring rules are evaluated in the standard-local calendar year. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static BOOL fail(void){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
static ULONGLONG ticks(const FILETIME *f){return ((ULONGLONG)f->dwHighDateTime<<32)|f->dwLowDateTime;}
static BOOL wall(ULONGLONG value,SYSTEMTIME *out){FILETIME f;f.dwLowDateTime=(DWORD)value;f.dwHighDateTime=(DWORD)(value>>32);return FileTimeToSystemTime(&f,out);}
static BOOL shift(ULONGLONG value,LONGLONG minutes,ULONGLONG *out)
{
    LONGLONG delta=minutes*600000000LL;
    if(value>0x7fffffffffffffffULL)return fail();
    if(delta<0){if(value<(ULONGLONG)-delta)return fail();*out=value-(ULONGLONG)-delta;}
    else{if(value>0x7fffffffffffffffULL-(ULONGLONG)delta)return fail();*out=value+(ULONGLONG)delta;}
    return TRUE;
}
static BOOL leap(WORD year){return !(year%4)&&(year%100||!(year%400));}
static BOOL transition(const SYSTEMTIME *rule,WORD year,LONGLONG previous_bias,ULONGLONG *out)
{
    static const BYTE days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    SYSTEMTIME date=*rule,first;FILETIME f;UINT day,limit;
    if(!rule->wMonth||rule->wMonth>12)return fail();
    if(!rule->wYear){
        if(!rule->wDay||rule->wDay>5||rule->wDayOfWeek>6)return fail();
        date.wYear=year;date.wDay=1;
        if(!SystemTimeToFileTime(&date,&f)||!FileTimeToSystemTime(&f,&first))return fail();
        limit=days[rule->wMonth-1]+(rule->wMonth==2&&leap(year));
        day=1+(7+rule->wDayOfWeek-first.wDayOfWeek)%7+7*(rule->wDay-1);
        if(day>limit)day-=7; /* Week five means the last such weekday. */
        date.wDay=(WORD)day;
    }
    if(!SystemTimeToFileTime(&date,&f))return fail();
    return shift(ticks(&f),previous_bias,out);
}
BOOL WINAPI fp_SystemTimeToTzSpecificLocalTime(const TIME_ZONE_INFORMATION *zone,
                                              const SYSTEMTIME *utc,SYSTEMTIME *local)
{
    typedef BOOL(WINAPI *Native)(const TIME_ZONE_INFORMATION*,const SYSTEMTIME*,SYSTEMTIME*);
    Native native=(Native)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),"SystemTimeToTzSpecificLocalTime");
    TIME_ZONE_INFORMATION info;SYSTEMTIME result,standard;FILETIME f;
    ULONGLONG time,adjusted,start,end;LONGLONG standard_bias,daylight_bias;BOOL daylight;
    /* NT 3.51 has an earlier implementation that rejects explicit dates. */
    if(native&&(GetVersion()&255)>=4)return native(zone,utc,local);
    if(!utc||!local)return fail();
    if(zone){UINT i;for(i=0;i<sizeof(info);i++)((BYTE*)&info)[i]=((const BYTE*)zone)[i];}
    else if(GetTimeZoneInformation(&info)==TIME_ZONE_ID_INVALID)return FALSE;
    if(!SystemTimeToFileTime(utc,&f))return fail();time=ticks(&f);
    if(!info.StandardDate.wMonth&&!info.DaylightDate.wMonth){
        if(!shift(time,-(LONGLONG)info.Bias,&adjusted)||!wall(adjusted,&result))return fail();
    }else{
        if(!info.StandardDate.wMonth||!info.DaylightDate.wMonth||
           (!info.StandardDate.wYear)!=(!info.DaylightDate.wYear))return fail();
        standard_bias=(LONGLONG)info.Bias+info.StandardBias;
        daylight_bias=(LONGLONG)info.Bias+info.DaylightBias;
        if(!shift(time,-standard_bias,&adjusted)||!wall(adjusted,&standard))return fail();
        if(!transition(&info.DaylightDate,standard.wYear,standard_bias,&start)||
           !transition(&info.StandardDate,standard.wYear,daylight_bias,&end))return FALSE;
        daylight=start<end?(time>=start&&time<end):(time>=start||time<end);
        if(!shift(time,-(daylight?daylight_bias:standard_bias),&adjusted)||!wall(adjusted,&result))return fail();
    }
    *local=result;return TRUE;
}
