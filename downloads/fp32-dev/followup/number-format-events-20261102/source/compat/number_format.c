/* Decimal-string locale formatting for NT 3.1. No floating-point conversion.
   Native reference tests use matching locale inputs across NT releases. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "number_negative_locales.h"
static BOOL fail(DWORD error){SetLastError(error);return FALSE;}
static BOOL locale_text(LCID locale,DWORD flags,LCTYPE type,WCHAR *out,int cap)
{return GetLocaleInfoW(locale,type|(flags&LOCALE_NOUSEROVERRIDE),out,cap)!=0;}
static BOOL locale_uint(LCID locale,DWORD flags,LCTYPE type,UINT *out)
{WCHAR b[32];UINT n=0;int i;if(!locale_text(locale,flags,type,b,32))return FALSE;
 for(i=0;b[i];i++){if(b[i]<'0'||b[i]>'9'||n>0x19999999)return fail(ERROR_INVALID_PARAMETER);n=n*10+b[i]-'0';}*out=n;return TRUE;}
static BOOL negative_order(LCID locale,DWORD flags,UINT *out)
{
    WCHAR language[16],text[16];UINT id=0,i;HKEY key;DWORD size,type;
    if(locale_uint(locale,flags,LOCALE_INEGNUMBER,out))return TRUE;
    /* NT3.1 predates this NLS field. Preserve an explicit current-user
       override, otherwise use a measured locale default, never guess. */
    if(!locale_text(locale,flags,LOCALE_ILANGUAGE,language,16))return FALSE;
    for(i=0;language[i];i++){WCHAR c=language[i];if(c>='0'&&c<='9')id=id*16+c-'0';else if(c>='a'&&c<='f')id=id*16+c-'a'+10;else if(c>='A'&&c<='F')id=id*16+c-'A'+10;else return fail(ERROR_INVALID_PARAMETER);}
    if(!(flags&LOCALE_NOUSEROVERRIDE)&&id==(GetUserDefaultLCID()&0xffff)&&RegOpenKeyExW(HKEY_CURRENT_USER,L"Control Panel\\International",0,KEY_QUERY_VALUE,&key)==0){
        size=sizeof(text);type=0;i=RegQueryValueExW(key,L"iNegNumber",0,&type,(BYTE*)text,&size);RegCloseKey(key);
        if(i==0&&type==REG_SZ&&size>=4&&size<=sizeof(text)&&text[0]>='0'&&text[0]<='4'&&text[1]==0){*out=text[0]-'0';return TRUE;}
    }
    for(i=0;i<sizeof(negative_locales)/sizeof(negative_locales[0]);i++)if(negative_locales[i].language==id){*out=negative_locales[i].order;return TRUE;}
    return fail(ERROR_INVALID_PARAMETER);
}
static BOOL valid_separator(LPCWSTR text,UINT limit,BOOL empty)
{UINT i;if(!text)return FALSE;for(i=0;text[i];i++)if(i>=limit||(text[i]>='0'&&text[i]<='9'))return FALSE;return empty||i!=0;}
static BOOL locale_group(LCID locale,DWORD flags,LCTYPE type,UINT *out)
{WCHAR b[32];UINT n=0;int i;if(!locale_text(locale,flags,type,b,32))return FALSE;
 for(i=0;b[i];i++){if(b[i]==';')continue;if(b[i]<'0'||b[i]>'9'||n>0x19999999)return fail(ERROR_INVALID_PARAMETER);if(b[i]=='0'){*out=n;return TRUE;}n=n*10+b[i]-'0';}
 *out=n*10;return TRUE;}
static int copy_text(WCHAR *out,int at,const WCHAR *s){while(*s)out[at++]=*s++;return at;}
static int render(LCID locale,DWORD flags,LPCWSTR value,UINT digits,UINT leading,
                  UINT grouping,LPCWSTR decimal,LPCWSTR thousand,UINT negative,
                  LPCWSTR currency,UINT positive,LPWSTR output,int capacity)
{
    WCHAR minus[32],valid[16],*integer=0,*number=0,*result=0;
    DWORD n=0,whole=0,fraction=0,i,j,sign=0,point=0,first=0,integer_len,carry;
    DWORD boundaries[10],groups=0,repeat=1,g=grouping,sep_len,decimal_len,symbol_len=0;
    SIZE_T alloc;ULONGLONG total;int at=0,r=0;const WCHAR *format,*scan;
    static const WCHAR *negative_number[]={L"(n)",L"-n",L"- n",L"n-",L"n -"};
    static const WCHAR *positive_money[]={L"$n",L"n$",L"$ n",L"n $"};
    static const WCHAR *negative_money[]={L"($n)",L"-$n",L"$-n",L"$n-",L"(n$)",L"-n$",L"n-$",L"n$-",L"-n $",L"-$ n",L"n $-",L"$ n-",L"$ -n",L"n- $",L"($ n)",L"(n $)"};
    if(!value||capacity<0||(!output&&capacity)||!valid_separator(decimal,3,FALSE)||!valid_separator(thousand,3,TRUE)||(currency&&!valid_separator(currency,5,TRUE))||digits>9||leading>1||negative>(currency?15:4)||(currency&&positive>3)){
        SetLastError(ERROR_INVALID_PARAMETER);return 0;
    }
    /* Validate the locale even when callers supply their own separators. */
    if(!locale_text(locale,flags,LOCALE_ILANGUAGE,valid,16)||!locale_text(locale,flags,LOCALE_SNEGATIVESIGN,minus,32))return 0;
    if(value[0]=='-'){sign=1;first=1;}
    for(i=first;value[i];i++){
        if(value[i]=='.'&&!point){point=1;continue;}
        if(value[i]<'0'||value[i]>'9'){SetLastError(ERROR_INVALID_PARAMETER);return 0;}
        n++;if(point)fraction++;else whole++;
    }
    if(grouping){if(g%10==0){repeat=0;g/=10;}while(g){if(g%10==0||groups>=10){SetLastError(ERROR_INVALID_PARAMETER);return 0;}boundaries[groups++]=g%10;g/=10;}}
    sep_len=lstrlenW(thousand);decimal_len=lstrlenW(decimal);if(currency)symbol_len=lstrlenW(currency);
    /* Worst case: grouping every digit, an increment, and sign/currency. */
    total=((ULONGLONG)whole+2)*(sep_len+1)+digits+decimal_len+symbol_len+40;
    if(total>0x3fffffff){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return 0;}
    alloc=(SIZE_T)total;
    integer=HeapAlloc(GetProcessHeap(),0,((SIZE_T)whole+3)*sizeof(WCHAR));
    number=HeapAlloc(GetProcessHeap(),0,alloc*sizeof(WCHAR));
    result=HeapAlloc(GetProcessHeap(),0,alloc*sizeof(WCHAR));
    if(!integer||!number||!result){SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto done;}
    integer[0]='0';for(i=0;i<whole;i++)integer[i+1]=value[first+i];integer_len=whole+1;
    /* Fraction starts immediately after the decimal point, if present. */
    j=first+whole+(point?1:0);
    carry=fraction>digits && value[j+digits]>='5';
    for(i=digits;i>0;i--){UINT d=i<=fraction?value[j+i-1]-'0':0;d+=carry;number[i-1]='0'+d%10;carry=d/10;}
    for(i=integer_len;i>0&&carry;i--){UINT d=integer[i-1]-'0'+carry;integer[i-1]='0'+d%10;carry=d/10;}
    first=0;while(first+1<integer_len&&integer[first]=='0')first++;
    /* Windows suppresses a negative sign when rounding produces zero. */
    if(sign){BOOL zero=TRUE;for(i=first;i<integer_len;i++)if(integer[i]!='0')zero=FALSE;for(i=0;i<digits;i++)if(number[i]!='0')zero=FALSE;if(zero)sign=0;}
    /* Temporarily keep the rounded fractional digits at the end of result. */
    for(i=0;i<digits;i++)result[i]=number[i];
    at=0;
    if(leading||integer_len-first>1||integer[first]!='0'||digits==0){
        DWORD next=groups?boundaries[groups-1]:0,gi=groups;
        /* Mark separator boundaries from the right in the integer buffer. */
        for(i=integer_len;i>first;i--){
            DWORD right=integer_len-i;
            if(i>first&&next&&right==next){integer[i]|=0x100;if(gi>1){gi--;next+=boundaries[gi-1];}else if(repeat)next+=boundaries[0];else next=0;}
        }
        for(i=first;i<integer_len;i++){if(integer[i]&0x100)at=copy_text(number,at,thousand);number[at++]=integer[i]&0xff;}
    }
    if(digits){at=copy_text(number,at,decimal);for(i=0;i<digits;i++)number[at++]=result[i];}number[at]=0;
    format=currency?(sign?negative_money[negative]:positive_money[positive]):(sign?negative_number[negative]:L"n");
    at=0;for(scan=format;*scan;scan++){if(*scan=='n')at=copy_text(result,at,number);else if(*scan=='-')at=copy_text(result,at,minus);else if(*scan=='$')at=copy_text(result,at,currency);else result[at++]=*scan;}result[at]=0;
    if(!capacity){r=at+1;goto done;}if(capacity<=at){SetLastError(ERROR_INSUFFICIENT_BUFFER);goto done;}
    for(i=0;i<=(DWORD)at;i++)output[i]=result[i];r=at+1;
 done:if(result)HeapFree(GetProcessHeap(),0,result);if(number)HeapFree(GetProcessHeap(),0,number);if(integer)HeapFree(GetProcessHeap(),0,integer);return r;
}
int WINAPI fp_GetNumberFormatW(LCID locale,DWORD flags,LPCWSTR value,const NUMBERFMTW *format,LPWSTR output,int capacity)
{
    typedef int(WINAPI*Native)(LCID,DWORD,LPCWSTR,const NUMBERFMTW*,LPWSTR,int);
    Native native=(void*)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),"GetNumberFormatW");
    NUMBERFMTW f;WCHAR decimal[32],thousand[32];
    /* Native NT3.51 regression did not complete for NULL numeric input.
       Validate modern API arguments before forwarding to that provider. */
    if(!value||capacity<0||(!output&&capacity)||(format&&(!format->lpDecimalSep||!format->lpThousandSep))){SetLastError(ERROR_INVALID_PARAMETER);return 0;}
#ifndef FP_FORCE_LEGACY_FORMAT
    if(native&&((GetVersion()&255)>=5||!format||format->Grouping<=9))return native(locale,flags,value,format,output,capacity);
#endif
    if((flags&~LOCALE_NOUSEROVERRIDE)||(flags&&format)){SetLastError(ERROR_INVALID_FLAGS);return 0;}
    if(!format){if(!locale_uint(locale,flags,LOCALE_IDIGITS,&f.NumDigits)||!locale_uint(locale,flags,LOCALE_ILZERO,&f.LeadingZero)||!negative_order(locale,flags,&f.NegativeOrder)||!locale_group(locale,flags,LOCALE_SGROUPING,&f.Grouping)||!locale_text(locale,flags,LOCALE_SDECIMAL,decimal,32)||!locale_text(locale,flags,LOCALE_STHOUSAND,thousand,32))return 0;f.lpDecimalSep=decimal;f.lpThousandSep=thousand;format=&f;}
    return render(locale,flags,value,format->NumDigits,format->LeadingZero,format->Grouping,format->lpDecimalSep,format->lpThousandSep,format->NegativeOrder,0,0,output,capacity);
}
int WINAPI fp_GetCurrencyFormatW(LCID locale,DWORD flags,LPCWSTR value,const CURRENCYFMTW *format,LPWSTR output,int capacity)
{
    typedef int(WINAPI*Native)(LCID,DWORD,LPCWSTR,const CURRENCYFMTW*,LPWSTR,int);
    Native native=(void*)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),"GetCurrencyFormatW");
    CURRENCYFMTW f;WCHAR decimal[32],thousand[32],symbol[64];
    if(!value||capacity<0||(!output&&capacity)||(format&&(!format->lpDecimalSep||!format->lpThousandSep||!format->lpCurrencySymbol))){SetLastError(ERROR_INVALID_PARAMETER);return 0;}
#ifndef FP_FORCE_LEGACY_FORMAT
    if(native&&((GetVersion()&255)>=5||!format||format->Grouping<=9))return native(locale,flags,value,format,output,capacity);
#endif
    if((flags&~LOCALE_NOUSEROVERRIDE)||(flags&&format)){SetLastError(ERROR_INVALID_FLAGS);return 0;}
    if(!format){if(!locale_uint(locale,flags,LOCALE_ICURRDIGITS,&f.NumDigits)||!locale_uint(locale,flags,LOCALE_ILZERO,&f.LeadingZero)||!locale_uint(locale,flags,LOCALE_INEGCURR,&f.NegativeOrder)||!locale_uint(locale,flags,LOCALE_ICURRENCY,&f.PositiveOrder)||!locale_group(locale,flags,LOCALE_SMONGROUPING,&f.Grouping)||!locale_text(locale,flags,LOCALE_SMONDECIMALSEP,decimal,32)||!locale_text(locale,flags,LOCALE_SMONTHOUSANDSEP,thousand,32)||!locale_text(locale,flags,LOCALE_SCURRENCY,symbol,64))return 0;f.lpDecimalSep=decimal;f.lpThousandSep=thousand;f.lpCurrencySymbol=symbol;format=&f;}
    if(!format->lpCurrencySymbol){SetLastError(ERROR_INVALID_PARAMETER);return 0;}
    return render(locale,flags,value,format->NumDigits,format->LeadingZero,format->Grouping,format->lpDecimalSep,format->lpThousandSep,format->NegativeOrder,format->lpCurrencySymbol,format->PositiveOrder,output,capacity);
}
