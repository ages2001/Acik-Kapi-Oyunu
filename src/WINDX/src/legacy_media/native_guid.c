#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "guiddef.h"

/* Class registration needs only the canonical string representation. NT 3.1
 * can run WinMM without having the separate OLE2 runtime installed. */
int WINAPI wx_guid_string(REFGUID guid, WCHAR *str, int capacity)
{
    static const WCHAR hex[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
    BYTE bytes[16];
    unsigned i, o=0;
    if (!guid || !str || capacity < 39) return 0;
    bytes[0]=guid->Data1>>24; bytes[1]=guid->Data1>>16; bytes[2]=guid->Data1>>8; bytes[3]=guid->Data1;
    bytes[4]=guid->Data2>>8; bytes[5]=guid->Data2; bytes[6]=guid->Data3>>8; bytes[7]=guid->Data3;
    for (i=0; i<8; ++i) bytes[8+i]=guid->Data4[i];
    str[o++]='{';
    for (i=0; i<16; ++i) {
        if (i==4 || i==6 || i==8 || i==10) str[o++]='-';
        str[o++]=hex[bytes[i]>>4]; str[o++]=hex[bytes[i]&15];
    }
    str[o++]='}'; str[o++]=0;
    return o;
}
