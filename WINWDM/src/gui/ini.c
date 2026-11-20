#include "kit.h"
#include <stdio.h>
#include <string.h>
#include <io.h>

/* SYSTEM.INI allows repeated device keys. Preserve every other entry and
 * comment byte-for-byte. Fixed transaction names also detect interrupted edits. */
BOOL WkEditVxd(const char *driver, BOOL install)
{
    char ini[PATH_CAP], backup[PATH_CAP], staged[PATH_CAP];
    char line[1024], key[1024], device[PATH_CAP+10], *first, *end;
    FILE *in, *out;
    BOOL ok=TRUE, section=FALSE, seen=FALSE;
    int n=GetWindowsDirectory(ini,sizeof(ini));
    if (!n || n>PATH_CAP-14 || strlen(driver)>PATH_CAP-1) return FALSE;
    strcpy(backup,ini); strcat(backup,"\\WKSYS.BAK");
    strcpy(staged,ini); strcat(staged,"\\WKSYS.NEW");
    strcat(ini,"\\SYSTEM.INI");
    strcpy(device,"device="); strcat(device,driver);
    WritePrivateProfileString(NULL,NULL,NULL,ini);
    if (access(backup,0)==0 || access(staged,0)==0) return FALSE;
    in=fopen(ini,"rb");
    if (!in) return FALSE;
    out=fopen(staged,"wb");
    if (!out) { fclose(in); return FALSE; }
    while (fgets(line,sizeof(line),in)) {
        size_t length=strlen(line);
        if (length==sizeof(line)-1 && line[length-1]!='\n') { ok=FALSE; break; }
        strcpy(key,line); first=key;
        while (*first==' ' || *first=='\t') ++first;
        end=first+strlen(first);
        while (end>first && (end[-1]=='\r'||end[-1]=='\n'||end[-1]==' '||end[-1]=='\t')) --end;
        *end=0;
        if (*first=='[') section=!stricmp(first,"[386Enh]");
        if (section && !stricmp(first,device)) continue;
        if (fwrite(line,1,length,out)!=length) { ok=FALSE; break; }
        if (!seen && !stricmp(first,"[386Enh]")) {
            seen=TRUE;
            if (install && fprintf(out,"%s%s\r\n",length && line[length-1]=='\n' ? "" : "\r\n",device)<0) {
                ok=FALSE; break;
            }
        }
    }
    if (ferror(in)) ok=FALSE;
    fclose(in);
    if (fclose(out)) ok=FALSE;
    if (!ok || !seen || rename(ini,backup)) { remove(staged); return FALSE; }
    if (rename(staged,ini)) { rename(backup,ini); remove(staged); return FALSE; }
    remove(backup);
    WritePrivateProfileString(NULL,NULL,NULL,ini);
    return TRUE;
}
