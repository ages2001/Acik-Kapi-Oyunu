/* NT3.1 Network Setup extension. Program Manager AddItem is not idempotent.
 * Inspect each group before changing only this package's DHCP startup item.
 * No group or unrelated item is deleted. The exported entry uses the native
 * Setup LibraryProcedure ABI: BOOL WINAPI (argc, argv, result-string-pointer).
 */
#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <ddeml.h>

#define TITLE "RLTKLANT DHCP"
#define CARDS "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\NetworkCards"
#define SERVICES "SYSTEM\\CurrentControlSet\\Services\\"
#define SETTINGS "Software\\Microsoft\\Windows NT\\CurrentVersion\\Program Manager\\Settings"
#define LIMIT 65536
static char data_buffer[LIMIT], groups[128][256], command[1024], image[MAX_PATH];
static char target[256], wire[2048], field[2048], title[256], long_command[1024];
static DWORD instance;
static HCONV conversation;
static const char *failure;

static int fail(const char *reason) { failure=reason;return 0; }
static int safe_text(const char *s)
{
    unsigned i;
    if(!*s)return 0;
    for(i=0;s[i];i++)if((unsigned char)s[i]<32||s[i]=='"'||s[i]=='['||s[i]==']')return 0;
    return 1;
}
static LONG text_value(HKEY key,const char *name,char *buffer,DWORD capacity)
{
    DWORD kind=0,size=capacity;LONG r;
    r=RegQueryValueExA(key,name,0,&kind,(BYTE*)buffer,&size);
    if(r)return r;
    if(kind!=REG_SZ||!size||size>capacity||buffer[size-1])return ERROR_INVALID_DATA;
    /* Embedded NULs must not hide additional command or group text. */
    if((DWORD)lstrlenA(buffer)+1!=size)return ERROR_INVALID_DATA;
    return 0;
}
static int enabled_adapters(void)
{
    HKEY root,key,parameters;DWORD i;LONG r;int enabled=0;
    char child[128],product[128],service[64],path[160];
    r=RegOpenKeyExA(HKEY_LOCAL_MACHINE,CARDS,0,KEY_READ,&root);
    if(r==ERROR_FILE_NOT_FOUND)return 0;
    if(r)return -1;
    for(i=0;i<256;i++) {
        DWORD size=sizeof(child),kind=0,value=0,j;FILETIME time;
        r=RegEnumKeyExA(root,i,child,&size,0,0,0,&time);
        if(r==ERROR_NO_MORE_ITEMS){r=0;break;}if(r)break;
        r=RegOpenKeyExA(root,child,0,KEY_READ,&key);if(r)break;
        r=text_value(key,"ProductName",product,sizeof(product));
        if(r==ERROR_FILE_NOT_FOUND||(!r&&lstrcmpiA(product,"RLTKLANT"))){RegCloseKey(key);r=0;continue;}
        if(!r)r=text_value(key,"ServiceName",service,sizeof(service));RegCloseKey(key);if(r)break;
        for(j=0;j<8&&service[j];j++)if((service[j]|32)!="rltklant"[j])break;
        if(j!=8||!service[j]){r=ERROR_INVALID_DATA;break;}
        for(;service[j];j++)if(service[j]<'0'||service[j]>'9')break;
        if(service[j]){r=ERROR_INVALID_DATA;break;}
        lstrcpyA(path,SERVICES);lstrcatA(path,service);lstrcatA(path,"\\Parameters");
        r=RegOpenKeyExA(HKEY_LOCAL_MACHINE,path,0,KEY_READ,&parameters);if(r)break;
        size=sizeof(value);r=RegQueryValueExA(parameters,"EnableDHCP",0,&kind,(BYTE*)&value,&size);RegCloseKey(parameters);
        if(r==ERROR_FILE_NOT_FOUND){r=0;continue;}
        if(r||kind!=REG_DWORD||size!=4||value>1){if(!r)r=ERROR_INVALID_DATA;break;}
        if(value)enabled=1;
    }
    RegCloseKey(root);return r||i==256?-1:enabled;
}
/* Parse one bounded CSV field, including doubled quote escaping. Return the
 * delimiter separately so malformed/truncated records cannot become items. */
static int csv_field(const char **cursor,const char *end,char *out,unsigned capacity,int *delimiter)
{
    const char *p=*cursor;unsigned n=0;int quoted=0,closed=0;
    if(p>=end||!capacity)return 0;
    if(*p=='"'){quoted=1;p++;}
    while(p<end) {
        char c=*p++;
        if(quoted&&!closed) {
            if(c=='"') {
                if(p<end&&*p=='"')p++;
                else {closed=1;continue;}
            } else if(c=='\r'||c=='\n'||!c)return 0;
        } else {
            if(c==','||c=='\r'||c=='\n'||!c) {
                if(c=='\r'&&p<end&&*p=='\n')p++;
                out[n]=0;*cursor=p;*delimiter=c==','?',':'\n';return 1;
            }
            if(closed||c=='"')return 0;
        }
        if(n+1>=capacity)return 0;
        out[n++]=c;
    }
    if(quoted&&!closed)return 0;
    out[n]=0;*cursor=p;*delimiter='\n';return 1;
}
static int normalized_command(const char *input,char *out,unsigned capacity)
{
    unsigned n=0;int quoted=*input=='"';char previous=0;
    if(quoted)input++;
    while(*input&&(quoted?*input!='"':*input!=' ')) {
        char c=*input++;
        if((unsigned char)c<32||c=='"')return 0;
        if(c=='/')c='\\';
        if(c=='\\'&&previous=='\\')continue;
        if(n+1>=capacity)return 0;
        out[n++]=c;previous=c;
    }
    if(quoted){if(*input!='"')return 0;input++;if(*input&&*input!=' ')return 0;}
    while(*input){char c=*input++;if((unsigned char)c<32||c=='"'||n+1>=capacity)return 0;out[n++]=c;}
    out[n]=0;return 1;
}
static int owned_command(const char *candidate)
{
    char a[1024],b[1024];
    if(!normalized_command(candidate,a,sizeof(a)))return 0;
    if(normalized_command(command,b,sizeof(b))&&!lstrcmpiA(a,b))return 1;
    return *long_command&&normalized_command(long_command,b,sizeof(b))&&!lstrcmpiA(a,b);
}
/* NT3.1 lacks GetShortPathName. Resolve only space-containing components to
 * the file system's actual alternate names; never guess an 8.3 spelling. */
static int command_image(char *path)
{
    char original[MAX_PATH],search[MAX_PATH],shortpath[MAX_PATH],component[MAX_PATH];
    unsigned at=3,full=3,shortlen=3,i;int spaces=0;
    for(i=0;path[i];i++)if(path[i]==' ')spaces=1;
    if(!spaces)return 1;
    if(path[1]!=':'||path[2]!='\\')return fail("Error: invalid system path");
    lstrcpyA(original,path);search[0]=shortpath[0]=path[0];search[1]=shortpath[1]=':';search[2]=shortpath[2]='\\';
    while(original[at]) {
        unsigned n=0;WIN32_FIND_DATAA found;HANDLE handle;const char *name;
        while(original[at]&&original[at]!='\\'){if(n+1>=sizeof(component))return fail("Error: system path component too long");component[n++]=original[at++];}
        component[n]=0;if(!n||full+n>=sizeof(search))return fail("Error: invalid system path component");
        for(i=0;i<n;i++)search[full++]=component[i];search[full]=0;
        handle=FindFirstFileA(search,&found);if(handle==INVALID_HANDLE_VALUE)return fail("Error: cannot resolve system file path");FindClose(handle);
        spaces=0;for(i=0;i<n;i++)if(component[i]==' ')spaces=1;
        name=spaces?found.cAlternateFileName:component;
        if(!*name||!safe_text(name))return fail("Error: no usable short system path");
        for(i=0;name[i];i++){if(name[i]==' '||shortlen+1>=sizeof(shortpath))return fail("Error: invalid short system path");shortpath[shortlen++]=name[i];}
        if(original[at]){at++;if(full+1>=sizeof(search)||shortlen+1>=sizeof(shortpath))return fail("Error: system path too long");search[full++]='\\';shortpath[shortlen++]='\\';}
    }
    shortpath[shortlen]=0;lstrcpyA(path,shortpath);return 1;
}
static HDDEDATA CALLBACK callback(UINT t,UINT f,HCONV c,HSZ a,HSZ b,HDDEDATA d,ULONG_PTR x,ULONG_PTR y)
{(void)t;(void)f;(void)c;(void)a;(void)b;(void)d;(void)x;(void)y;return 0;}
static int request(const char *item_name)
{
    HSZ item=DdeCreateStringHandleA(instance,item_name,CP_WINANSI);HDDEDATA data;DWORD size,copied;
    if(!item)return fail("Error: cannot allocate DDE item");
    data=DdeClientTransaction(0,0,conversation,item,CF_TEXT,XTYP_REQUEST,5000,0);
    DdeFreeStringHandle(instance,item);
    if(!data)return fail("Error: cannot read Program Manager group");
    size=DdeGetData(data,0,0,0);
    if(!size||size>=sizeof(data_buffer)){DdeFreeDataHandle(data);return fail("Error: invalid Program Manager data size");}
    copied=DdeGetData(data,(BYTE*)data_buffer,size,0);DdeFreeDataHandle(data);
    if(copied!=size)return fail("Error: incomplete Program Manager data");
    /* NT3.1 may return the CF_TEXT byte count without its final NUL. Bound
       the copy by the advertised size and append our own terminator. */
    data_buffer[size]=0;
    /* CF_TEXT ends at its first NUL. The DDE allocation may contain unused
       nonzero bytes after it; none of those bytes belongs to the text. */
    return 1;
}
static int execute(const char *text)
{
    return DdeClientTransaction((BYTE*)text,lstrlenA(text)+1,conversation,0,0,XTYP_EXECUTE,5000,0)!=0||fail("Error: Program Manager command failed");
}
static int select_group(const char *group)
{
    /* NT3.1 defaults to COMMON for administrators. Explicit zero selects
       the current user's group and brings it first in the MDI Z order. */
    lstrcpyA(wire,"[CreateGroup(\"");lstrcatA(wire,group);lstrcatA(wire,"\",0)]");return execute(wire);
}
static int personal_group(const char *group)
{
    HKEY root,key;LONG r;unsigned i;
    for(i=0;group[i];i++)if(group[i]=='\\')return -1;
    r=RegOpenKeyExA(HKEY_CURRENT_USER,"Program Groups",0,KEY_READ,&root);
    if(r==ERROR_FILE_NOT_FOUND)return 0;
    if(r)return -1;
    r=RegOpenKeyExA(root,group,0,KEY_READ,&key);RegCloseKey(root);
    if(!r){RegCloseKey(key);return 1;}
    return r==ERROR_FILE_NOT_FOUND?0:-1;
}
static int group_names(unsigned *count)
{
    const char *p;unsigned n=0;
    if(!request("PROGMAN"))return 0;
    p=data_buffer;
    while(*p) {
        unsigned length=0;
        if(*p=='\r'||*p=='\n'){p++;continue;}
        if(n==128)return fail("Error: too many Program Manager groups");
        while(*p&&*p!='\r'&&*p!='\n') {
            if(length+1>=sizeof(groups[n]))return fail("Error: group name too long");
            groups[n][length++]=*p++;
        }
        groups[n][length]=0;if(!safe_text(groups[n]))return fail("Error: unsupported group name syntax");
        {
            unsigned j;int personal=personal_group(groups[n]);
            if(personal<0)return fail("Error: cannot identify personal group");
            if(!personal)continue;
            for(j=0;j<n;j++)if(!lstrcmpiA(groups[j],groups[n]))break;
            if(j==n)n++;
        }
    }
    *count=n;return 1;
}
static int item_count(const char *group,unsigned *owned,int *conflict)
{
    const char *p,*end;unsigned row=0;int delimiter;
    /* NT3.1 REQUEST matches the first same-name MDI child. Selecting the
       personal group first disambiguates it from an existing common group. */
    *owned=0;*conflict=0;if(!select_group(group)||!request(group))return 0;
    p=data_buffer;end=p+lstrlenA(p);
    while(p<end) {
        unsigned column=0;int match=0,owns=0;
        if(*p=='\r'||*p=='\n'){p++;continue;}
        do {
            if(!csv_field(&p,end,field,sizeof(field),&delimiter))return fail("Error: malformed Program Manager item");
            if(!column){match=!lstrcmpiA(field,TITLE);if((unsigned)lstrlenA(field)>=sizeof(title))return fail("Error: oversized item title");lstrcpyA(title,field);}
            if(column==1&&match)owns=owned_command(field);
            if(++column>32)return fail("Error: too many item fields");
        }while(delimiter==',');
        if(!row++){if(column<2||lstrcmpiA(title,group))return fail("Error: incorrect group header");continue;}
        if(column<2)return fail("Error: incomplete Program Manager item");
        if(match){if(owns)(*owned)++;else *conflict=1;}
        if(row>1024)return fail("Error: too many Program Manager items");
    }
    return row!=0||fail("Error: empty Program Manager group response");
}
static int remove_items(const char *group,unsigned count)
{
    unsigned after;int conflict;
    if(!count)return 1;
    if(!select_group(group))return 0;
    while(count) {
        if(!execute("[DeleteItem(\"" TITLE "\")]")||!item_count(group,&after,&conflict))return 0;
        if(conflict||after>=count)return fail("Error: DHCP item removal did not converge");
        count=after;
    }
    return 1;
}
static int sync_startup(void)
{
    HKEY key;HSZ service;LONG r;unsigned n,i,counts[128];int conflicts[128],enabled,found=0;
    DWORD length=GetSystemDirectoryA(image,sizeof(image));
    if(!length||length>=sizeof(image)-14||!safe_text(image))return fail("Error: invalid system directory");
    lstrcatA(image,"\\DHCPNT31.EXE");
    lstrcpyA(long_command,"\"");lstrcatA(long_command,image);lstrcatA(long_command,"\" /auto /apply");
    if(!command_image(image))return 0;
    lstrcpyA(command,image);lstrcatA(command," /auto /apply");
    enabled=enabled_adapters();if(enabled<0)return fail("Error: cannot read adapter DHCP preferences");
    lstrcpyA(target,"Startup");r=RegOpenKeyExA(HKEY_CURRENT_USER,SETTINGS,0,KEY_READ,&key);
    if(!r){r=text_value(key,"Startup",target,sizeof(target));RegCloseKey(key);if(r==ERROR_FILE_NOT_FOUND){r=0;lstrcpyA(target,"Startup");}}
    else if(r==ERROR_FILE_NOT_FOUND)r=0;
    if(r||!safe_text(target))return fail("Error: invalid Startup group setting");
    /* An exported entry may be called again without unloading this DLL. */
    instance=0;conversation=0;
    if(DdeInitializeA(&instance,callback,APPCMD_CLIENTONLY,0))return fail("Error: cannot initialize Program Manager client");
    service=DdeCreateStringHandleA(instance,"PROGMAN",CP_WINANSI);
    conversation=service?DdeConnect(instance,service,service,0):0;
    if(service)DdeFreeStringHandle(instance,service);
    if(!conversation){DdeUninitialize(instance);return fail("Error: Program Manager is unavailable");}
    if(!group_names(&n))goto error;
    /* Read every relevant item before the first mutation. Same-title user
       commands cannot be addressed independently by the DDE DeleteItem API. */
    for(i=0;i<n;i++) {
        if(!item_count(groups[i],&counts[i],&conflicts[i]))goto error;
        if(!lstrcmpiA(groups[i],target)){found=1;if(enabled&&conflicts[i]){fail("Error: Startup contains a different RLTKLANT DHCP command");goto error;}}
        if(counts[i]&&conflicts[i]){fail("Error: mixed owned and user DHCP items require manual resolution");goto error;}
    }
    if(enabled) {
        unsigned count=0;int conflict=0;
        if(found&&!item_count(target,&count,&conflict))goto error;
        if(count!=1) {
            if(!remove_items(target,count)||!select_group(target))goto error;
            lstrcpyA(wire,"[AddItem(\"");lstrcatA(wire,command);lstrcatA(wire,"\",\"" TITLE "\",\"");lstrcatA(wire,image);lstrcatA(wire,"\",0)]");
            if(!execute(wire)||!item_count(target,&count,&conflict))goto error;
            if(count!=1||conflict){fail("Error: DHCP startup creation did not verify");goto error;}
        }
        r=RegCreateKeyExA(HKEY_CURRENT_USER,SETTINGS,0,0,0,KEY_SET_VALUE,0,&key,0);
        if(!r){r=RegSetValueExA(key,"Startup",0,REG_SZ,(BYTE*)target,lstrlenA(target)+1);RegCloseKey(key);}
        if(r){fail("Error: cannot save Startup group setting");goto error;}
    }
    for(i=0;i<n;i++)if(!enabled||lstrcmpiA(groups[i],target))if(!remove_items(groups[i],counts[i]))goto error;
    DdeDisconnect(conversation);DdeUninitialize(instance);return 1;
error:
    DdeDisconnect(conversation);DdeUninitialize(instance);return 0;
}
/* NCPA Update does not automatically replace the INF used by Configure.
 * Register the private, copied INF for all owned instances and the shared
 * software entry. Open/read every target first; roll back a partial write. */
#ifndef SETUP_INF
#define SETUP_INF "RLTKNT31.INF"
#endif
#define PRODUCT "SOFTWARE\\Realtek\\RLTKLANT\\CurrentVersion"
typedef struct { HKEY key; char previous[MAX_PATH]; int existed; } INF_TARGET;
static INF_TARGET inf_targets[257];
static int prepare_inf_target(HKEY parent,unsigned *count)
{
    INF_TARGET *t;LONG r;
    if(*count>=257)return fail("Error: too many installer registrations");
    t=&inf_targets[*count];t->key=0;
    r=RegOpenKeyExA(parent,"NetRules",0,KEY_QUERY_VALUE|KEY_SET_VALUE,&t->key);
    if(r)return fail("Error: cannot open installer registration");
    (*count)++;
    r=text_value(t->key,"InfName",t->previous,sizeof(t->previous));
    t->existed=r!=ERROR_FILE_NOT_FOUND;
    if(r&&r!=ERROR_FILE_NOT_FOUND)return fail("Error: invalid prior installer registration");
    return 1;
}
static int refresh_registration(void)
{
    HKEY root=0,key=0;LONG r;DWORD i;unsigned count=0,j;int ok=0,rollback=1;
    char path[MAX_PATH],child[128],product[128];DWORD length,attributes;
    length=GetSystemDirectoryA(path,sizeof(path));
    if(!length||length>=sizeof(path)-sizeof(SETUP_INF)-1)return fail("Error: invalid installer directory");
    lstrcatA(path,"\\" SETUP_INF);attributes=GetFileAttributesA(path);
    if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_DIRECTORY))return fail("Error: persistent installer INF is missing");
    r=RegOpenKeyExA(HKEY_LOCAL_MACHINE,CARDS,0,KEY_READ,&root);
    if(r)return fail("Error: cannot enumerate installer registrations");
    for(i=0;i<256;i++) {
        DWORD size=sizeof(child);FILETIME time;
        r=RegEnumKeyExA(root,i,child,&size,0,0,0,&time);
        if(r==ERROR_NO_MORE_ITEMS){r=0;break;}
        if(r)break;
        r=RegOpenKeyExA(root,child,0,KEY_READ,&key);if(r)break;
        r=text_value(key,"ProductName",product,sizeof(product));
        if(r==ERROR_FILE_NOT_FOUND||(!r&&lstrcmpiA(product,"RLTKLANT"))){RegCloseKey(key);key=0;r=0;continue;}
        if(!r&&!prepare_inf_target(key,&count))r=ERROR_INVALID_DATA;
        RegCloseKey(key);key=0;if(r)break;
    }
    if(r||i==256){fail("Error: cannot prepare adapter installer registrations");goto done;}
    r=RegOpenKeyExA(HKEY_LOCAL_MACHINE,PRODUCT,0,KEY_READ,&key);
    if(r){fail("Error: shared installer registration is missing");goto done;}
    if(!prepare_inf_target(key,&count))goto done;
    RegCloseKey(key);key=0;
    for(j=0;j<count;j++) {
        r=RegSetValueExA(inf_targets[j].key,"InfName",0,REG_SZ,(const BYTE*)SETUP_INF,sizeof(SETUP_INF));
        if(r) {
            while(j) {
                INF_TARGET *t=&inf_targets[--j];
                r=t->existed?RegSetValueExA(t->key,"InfName",0,REG_SZ,(BYTE*)t->previous,lstrlenA(t->previous)+1):RegDeleteValueA(t->key,"InfName");
                if(r)rollback=0;
            }
            fail(rollback?"Error: installer registration failed; prior values restored":"Error: installer registration failed; rollback incomplete");goto done;
        }
    }
    ok=1;
done:
    if(key)RegCloseKey(key);
    if(root)RegCloseKey(root);
    while(count)RegCloseKey(inf_targets[--count].key);
    return ok;
}
static BOOL run_setup(DWORD argc,LPSTR *argv,LPSTR *result,int (*action)(void))
{
    HANDLE mutex;DWORD wait;int ok;(void)argv;
    if(!result)return FALSE;
    *result="Error: unexpected arguments";if(argc)return TRUE;
    mutex=CreateMutexA(0,FALSE,"RLTKLANT.DhcpStartup");
    if(!mutex){*result="Error: cannot lock Startup maintenance";return TRUE;}
    wait=WaitForSingleObject(mutex,5000);
    if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED){CloseHandle(mutex);*result="Error: Startup maintenance is busy";return TRUE;}
    failure="Error: startup synchronization failed";ok=action();
    ReleaseMutex(mutex);CloseHandle(mutex);*result=(LPSTR)(ok?"Success":failure);return TRUE;
}
__declspec(dllexport) BOOL WINAPI SyncStartup(DWORD argc,LPSTR *argv,LPSTR *result)
{return run_setup(argc,argv,result,sync_startup);}
__declspec(dllexport) BOOL WINAPI RefreshRegistration(DWORD argc,LPSTR *argv,LPSTR *result)
{return run_setup(argc,argv,result,refresh_registration);}
BOOL WINAPI DllEntry(HINSTANCE module,DWORD reason,LPVOID reserved)
{(void)module;(void)reason;(void)reserved;return TRUE;}
