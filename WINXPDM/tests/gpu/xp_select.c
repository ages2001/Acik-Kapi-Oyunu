/* Match each actual XP display INF against a temporary device information element.
 * No device is registered and no package is installed.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <windows.h>
#include <setupapi.h>
#include <stdio.h>
#include <string.h>

static const GUID display = {0x4d36e968,0xe325,0x11ce,{0xbf,0xc1,0x08,0x00,0x2b,0xe1,0x03,0x18}};

static DWORD match(const char *id, const char *inf, char section[256], DWORD *rank, const char **phase)
{
    HDEVINFO set = SetupDiCreateDeviceInfoList(&display,NULL);
    SP_DEVINFO_DATA dev = {sizeof(dev)};
    SP_DEVINSTALL_PARAMS_A params = {sizeof(params)};
    SP_DRVINFO_DATA_A selected = {sizeof(selected)};
    SP_DRVINSTALL_PARAMS details = {sizeof(details)};
    PSP_DRVINFO_DETAIL_DATA_A info = NULL;
    char ids[256] = {0};
    DWORD count = 0, error = ERROR_SUCCESS;
    if (set == INVALID_HANDLE_VALUE) return GetLastError();
    if (lstrlenA(id) > (int)sizeof(ids)-2 || lstrlenA(inf) >= MAX_PATH) { error = ERROR_INVALID_DATA; goto done; }
    lstrcpyA(ids,id);
    *phase = "create-element";
    if (!SetupDiCreateDeviceInfoA(set,"Display",&display,NULL,NULL,DICD_GENERATE_ID,&dev)) goto error;
    *phase = "hardware-id";
    if (!SetupDiSetDeviceRegistryPropertyA(set,&dev,SPDRP_HARDWAREID,(BYTE *)ids,lstrlenA(ids)+2)) goto error;
    *phase = "get-parameters";
    if (!SetupDiGetDeviceInstallParamsA(set,&dev,&params)) goto error;
    params.Flags |= DI_ENUMSINGLEINF | DI_QUIETINSTALL;
    lstrcpyA(params.DriverPath,inf);
    *phase = "set-parameters";
    if (!SetupDiSetDeviceInstallParamsA(set,&dev,&params)) goto error;
    *phase = "build-compatible-list";
    if (!SetupDiBuildDriverInfoList(set,&dev,SPDIT_COMPATDRIVER)) goto error;
    *phase = "select-best";
    if (!SetupDiSelectBestCompatDrv(set,&dev)) goto error;
    *phase = "get-selection";
    if (!SetupDiGetSelectedDriverA(set,&dev,&selected)) goto error;
    *phase = "rank";
    if (!SetupDiGetDriverInstallParamsA(set,&dev,&selected,&details)) goto error;
    *phase = "detail-size";
    SetupDiGetDriverInfoDetailA(set,&dev,&selected,NULL,0,&count);
    if (!count) goto error;
    if (count < sizeof(*info)) count = sizeof(*info);
    info = LocalAlloc(LPTR,count);
    if (!info) { error = ERROR_NOT_ENOUGH_MEMORY; goto done; }
    *phase = "section";
    info->cbSize = sizeof(*info);
    if (!SetupDiGetDriverInfoDetailA(set,&dev,&selected,info,count,NULL)) { error = GetLastError(); goto done; }
    lstrcpynA(section,info->SectionName,256);
    *rank = details.Rank;
    *phase = "selected";
    goto done;
error:
    error = GetLastError();
done:
    if (info) LocalFree(info);
    SetupDiDestroyDeviceInfoList(set);
    return error;
}

int main(void)
{
    char dir[MAX_PATH], windir[MAX_PATH], path[MAX_PATH], line[1024], section[256];
    FILE *in, *out;
    char *slash;
    unsigned cases = 0, failures = 0;
    if (!GetModuleFileNameA(NULL,dir,sizeof(dir)) || !(slash=strrchr(dir,'\\')) ||
        !GetWindowsDirectoryA(windir,sizeof(windir))) return 2;
    *slash=0;
    if (!SetCurrentDirectoryA(dir) || !(in=fopen("CASES.TSV","r"))) return 2;
    out=fopen("SELECT.TSV","w");
    if (!out) { fclose(in); return 2; }
    fputs("pci_id\thardware_id\tpackage\tinf\texpected_section\tselected_section\trank\tphase\terror\n",out);
    while (fgets(line,sizeof(line),in)) {
        char *field[5],*cursor=line;
        DWORD error,rank=0; const char *phase="input";
        int i,n;
        line[strcspn(line,"\r\n")]=0;
        for (i=0;i<5 && cursor;i++) {
            field[i]=cursor; cursor=strchr(cursor,'\t');
            if (cursor) *cursor++=0;
        }
        if (i!=5 || cursor) { fclose(in); fclose(out); return 2; }
        n=field[3][0]=='@' ? snprintf(path,sizeof(path),"%s\\inf\\%s",windir,field[3]+1) :
                             snprintf(path,sizeof(path),"%s\\%s",dir,field[3]);
        section[0]=0;
        error=n < 0 || n >= (int)sizeof(path) ? ERROR_BUFFER_OVERFLOW : match(field[1],path,section,&rank,&phase);
        ++cases;
        if (error || lstrcmpiA(section,field[4])) ++failures;
        fprintf(out,"%s\t%s\t%s\t%s\t%s\t%s\t%lu\t%s\t%08lx\n",field[0],field[1],field[2],field[3],field[4],section,rank,phase,error);
        fflush(out);
    }
    if (ferror(in) || ferror(out)) ++failures;
    fclose(in);
    if (fclose(out)) ++failures;
    return failures || !cases;
}
