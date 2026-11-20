/* MinGW's numeric formatting code calls IsDBCSLeadByteEx. NT 3.1
 * already publishes the lead-byte ranges through GetCPInfo. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static BOOL WINAPI codepage_lead_byte(UINT codepage, BYTE byte)
{
    CPINFO info;
    unsigned int i;

    if (!GetCPInfo(codepage, &info))
        return FALSE;
    for (i = 0; i + 1 < sizeof(info.LeadByte) && info.LeadByte[i]; i += 2)
        if (byte >= info.LeadByte[i] && byte <= info.LeadByte[i + 1])
            return TRUE;
    return FALSE;
}

/* Satisfy the import reference in the x86 static library at link time. */
BOOL (WINAPI *const windx_lead_byte)(UINT, BYTE)
    __asm__("__imp__IsDBCSLeadByteEx@8") = codepage_lead_byte;
