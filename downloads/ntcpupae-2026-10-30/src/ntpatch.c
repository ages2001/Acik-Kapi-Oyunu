/*
 * ntpatch.c  -  NT CPU and PAE Patcher (GUI)  -  self-contained, CRT-free
 * ============================================================================
 * Runs ON the running OS (NT 3.1 / 3.5x / 4.0 / 2000). Imports kernel32 +
 * user32 + advapi32 + gdi32 (all present since NT 3.1). GetVersion + registry
 * OS detection; SubsystemVersion 3.10 so it loads on NT 3.1.
 *
 * Scans EVERY .exe in system32 by PE structure to list the real kernels, lets
 * the user pick CPU / License / PAE patches (and the PAE max 8/16/32/64), writes
 * a separate 8.3 kernel, replaces NTLDR + NTDETECT.COM on the BOOT partition
 * (backing up the originals as *.OLD), adds a boot.ini entry and sets the
 * processor-license registry values live.
 */
#include "guiwin.h"
#include "ntldr_bin.h"
#include "ntdetect_bin.h"

/* ---- control IDs ---- */
#define IDC_TITLE  104
#define IDC_STATIC 103
#define IDC_LIST   100
#define IDC_SCAN   101
#define IDC_PATCH  102
#define IDC_GROUP  105
#define IDC_OPTS   106
#define IDC_CPU    110
#define IDC_LIC    111
#define IDC_PAE    112
#define IDC_PAEMAX 113
#define IDC_PMLBL  114
#define IDC_LDR    115

static HINSTANCE g_hInst;
static HWND g_hList, g_hStatic, g_hTitle;
static HFONT g_font, g_fontB, g_fontT;
static HICON g_icon;
static HBRUSH g_bg;      /* COLOR_BTNFACE brush (NT 3.1 has no GetSysColorBrush) */
static int  g_major, g_minor, g_build, g_sp;
static int  g_is2000, g_pae_ok;
static int  g_is9x, g_supported;
static char g_arch[12];          /* "x86"/"x64"/"ARM64"/"IA-64" for XP..10, else "" */
static char g_sysdir[260];
static char g_sys32[300];
static char g_osname[160];
static char g_bootdrv[8];       /* boot partition root, e.g. "C:\" */
static int  g_hal_status;       /* MPS HAL multi-CPU: 0 n/a, 1 can patch, 2 already patched */
static int  g_hal_present;      /* hal.dll exists and was read (NT 3.x/4.0 only)             */

typedef struct { int kind; char text[72]; char name[16]; int cpu,lic,pae; } ROW;
static ROW g_rows[48]; static int g_nrows;
static int g_sel = -1;      /* currently selected kernel row index, -1 = none */
static HWND g_hwnd;         /* main window (for GetDlgItem from helpers) */
static void update_opts(void);        /* fwd: match options to selected kernel */
static void sel_first_kernel(void);   /* fwd: auto-select first kernel row */

/* ======================= file I/O ======================= */
static BYTE* read_file(const char* path, DWORD* outsz){
    HANDLE h=CreateFileA(path,GENERIC_READ,1,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(h==INVALID_HANDLE_VALUE) return 0;
    DWORD sz=GetFileSize(h,0);
    if(sz==0xFFFFFFFF||sz==0){ CloseHandle(h); return 0; }
    BYTE* b=(BYTE*)VirtualAlloc(0,sz+0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!b){ CloseHandle(h); return 0; }
    DWORD got=0,total=0;
    while(total<sz && ReadFile(h,b+total,sz-total,&got,0) && got){ total+=got; }
    CloseHandle(h);
    *outsz=total;
    return b;
}
static void free_file(BYTE* b){ if(b) VirtualFree(b,0,MEM_RELEASE); }
static BOOL write_file(const char* path, const BYTE* b, DWORD sz){
    HANDLE h=CreateFileA(path,GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    if(h==INVALID_HANDLE_VALUE) return 0;
    DWORD put=0,total=0; BOOL ok=1;
    while(total<sz){ if(!WriteFile(h,(LPVOID)(b+total),sz-total,&put,0)||!put){ ok=0; break; } total+=put; }
    CloseHandle(h);
    return ok;
}

#include "ntengine.h"

/* ======================= OS detection ======================= */
static void detect_os(void){
    DWORD v=GetVersion();
    g_major=v&0xFF; g_minor=(v>>8)&0xFF;
    g_is9x=(v&0x80000000)!=0;                 /* high bit set => Windows 9x/Me */
    g_build=(v<0x80000000)?((v>>16)&0xFFFF):((v>>16)&0x3FFF);
    g_sp=0; g_osname[0]=0;
    HKEY hk;
    if(g_is9x){
        /* Windows 95/98/Me keep their version under ...\Windows\CurrentVersion. */
        if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SOFTWARE\\Microsoft\\Windows\\CurrentVersion",0,KEY_READ,&hk)==ERROR_SUCCESS){
            DWORD t,cb;
            { char pn[160]; cb=sizeof(pn); t=0;
              if(RegQueryValueExA(hk,"ProductName",0,&t,(BYTE*)pn,&cb)==ERROR_SUCCESS) m_strcpy(g_osname,pn); }
            { char vn[40]; cb=sizeof(vn); t=0;            /* "4.10.2222" -> build 2222 */
              if(RegQueryValueExA(hk,"VersionNumber",0,&t,(BYTE*)vn,&cb)==ERROR_SUCCESS){
                  int p=0,dots=0; while(vn[p]){ if(vn[p]=='.'){ if(++dots==2){ p++; int n=0; while(vn[p]>='0'&&vn[p]<='9'){n=n*10+vn[p]-'0';p++;} if(n)g_build=n; break; } } p++; } } }
            RegCloseKey(hk);
        }
    } else
    if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",0,KEY_READ,&hk)==ERROR_SUCCESS){
        DWORD t,cb;
        DWORD maj=0; cb=4; t=0;
        if(RegQueryValueExA(hk,"CurrentMajorVersionNumber",0,&t,(BYTE*)&maj,&cb)==ERROR_SUCCESS && t==REG_DWORD && maj){
            g_major=maj; DWORD mn=0; cb=4; RegQueryValueExA(hk,"CurrentMinorVersionNumber",0,0,(BYTE*)&mn,&cb); g_minor=mn;
        } else {
            char cv[32]; cb=sizeof(cv); t=0;
            if(RegQueryValueExA(hk,"CurrentVersion",0,&t,(BYTE*)cv,&cb)==ERROR_SUCCESS){
                int mj=0,mn=0,p=0; while(cv[p]>='0'&&cv[p]<='9'){mj=mj*10+cv[p]-'0';p++;}
                if(cv[p]=='.'){p++; while(cv[p]>='0'&&cv[p]<='9'){mn=mn*10+cv[p]-'0';p++;}}
                if(mj){ g_major=mj; g_minor=mn; }
            }
        }
        { char bs[32]; cb=sizeof(bs); t=0;
          if(RegQueryValueExA(hk,"CurrentBuildNumber",0,&t,(BYTE*)bs,&cb)==ERROR_SUCCESS){
              int bn=0,p=0; while(bs[p]>='0'&&bs[p]<='9'){bn=bn*10+bs[p]-'0';p++;} if(bn)g_build=bn; } }
        { char pn[160]; cb=sizeof(pn); t=0;
          if(RegQueryValueExA(hk,"ProductName",0,&t,(BYTE*)pn,&cb)==ERROR_SUCCESS) m_strcpy(g_osname,pn); }
        { char cs[64]; cb=sizeof(cs); t=0;
          if(RegQueryValueExA(hk,"CSDVersion",0,&t,(BYTE*)cs,&cb)==ERROR_SUCCESS && t==REG_SZ){
              int p=0; while(cs[p]&&!(cs[p]>='0'&&cs[p]<='9'))p++; int n=0; while(cs[p]>='0'&&cs[p]<='9'){n=n*10+cs[p]-'0';p++;} g_sp=n; } }
        RegCloseKey(hk);
    }
    if(!g_is9x && g_sp==0 && RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Control\\Windows",0,KEY_READ,&hk)==ERROR_SUCCESS){
        DWORD d=0,cb=4,t=0;
        if(RegQueryValueExA(hk,"CSDVersion",0,&t,(BYTE*)&d,&cb)==ERROR_SUCCESS && t==REG_DWORD) g_sp=(d>>8)&0xFF;
        RegCloseKey(hk);
    }
    g_is2000=(!g_is9x && g_major==5 && g_minor==0);
    g_pae_ok=g_is2000;                      /* PAE patch supported on Windows 2000 */
    /* This patcher only targets NT 3.1 / 3.5x / 4.0 / 2000 kernels. */
    g_supported=(!g_is9x && (g_major==3 || g_major==4 || (g_major==5 && g_minor==0)));
    /* OS architecture, only where more than one existed: XP/2003, Vista..8.1, Win10
       (Win11 is x64-only, NT<=2000 & 9x are x86 -> not shown). */
    g_arch[0]=0;
    if(!g_is9x && ((g_major==5 && g_minor>=1) || g_major==6 || (g_major==10 && g_build<22000))){
        HANDLE k=GetModuleHandleA("kernel32.dll");
        if(k){ PFN_GetNativeSystemInfo p=(PFN_GetNativeSystemInfo)GetProcAddress(k,"GetNativeSystemInfo");
            if(p){ SYSTEM_INFO si; memset(&si,0,sizeof(si)); p(&si);
                WORD a=si.wProcessorArchitecture;
                if(a==9)       m_strcpy(g_arch,"x64");
                else if(a==6)  m_strcpy(g_arch,"IA-64");
                else if(a==12) m_strcpy(g_arch,"ARM64");
                else           m_strcpy(g_arch,"x86"); } }
    }
    GetWindowsDirectoryA(g_sysdir,sizeof(g_sysdir));
    m_strcpy(g_sys32,g_sysdir); m_strcat(g_sys32,"\\system32");
}
static const char* os_name(void){
    int mj=g_major,mn=g_minor;
    if(g_is9x){
        if(mj==4&&mn==0){                    /* Windows 95 releases, by build */
            if(g_build>=1214) return "Windows 95 OSR2.5 (C)";
            if(g_build>=1212) return "Windows 95 OSR2.1";
            if(g_build>=1111) return "Windows 95 OSR2 (B)";
            return "Windows 95";
        }
        if(mj==4&&mn==10) return (g_build>=2222)?"Windows 98 SE":"Windows 98";
        if(mj==4&&mn==90) return "Windows Me";
        if(g_osname[0])   return g_osname;
        return "Windows 9x";
    }
    if(mj==3&&(mn==10||mn==1)) return "Windows NT 3.1";
    if(mj==3&&mn==50) return "Windows NT 3.5";
    if(mj==3&&mn==51) return "Windows NT 3.51";
    if(mj==4)         return "Windows NT 4.0";
    if(mj==5&&mn==0)  return "Windows 2000";
    if(mj==5&&mn==1)  return "Windows XP";
    if(mj==5&&mn==2)  return "Windows Server 2003 / XP x64";
    if(mj==6&&mn==0)  return "Windows Vista / Server 2008";
    if(mj==6&&mn==1)  return "Windows 7 / Server 2008 R2";
    if(mj==6&&mn==2)  return "Windows 8 / Server 2012";
    if(mj==6&&mn==3)  return "Windows 8.1 / Server 2012 R2";
    if(mj==10){ return (g_build>=22000)?"Windows 11":"Windows 10"; }
    if(g_osname[0]) return g_osname;
    return "Windows NT (unknown)";
}
static void show_osinfo(void){
    char b[260], arch[16]; arch[0]=0;
    const char* name=os_name();
    /* only NT 3.1 / 3.5x / 4.0 / 2000 kernels are patchable */
    const char* mark=g_supported ? "" : "     [UNSUPPORTED]";
    if(g_arch[0]){ m_strcpy(arch,"   "); m_strcat(arch,g_arch); }   /* "   x64" etc. */
    if(g_is9x || g_major>=10)
        wsprintfA(b,"%s%s   (build %d)%s", name, arch, g_build, mark);          /* 9x & Win10+: build only */
    else
        wsprintfA(b,"%s   Service Pack %d%s   (build %d)%s", name, g_sp, arch, g_build, mark);
    SetWindowTextA(g_hStatic,b);
}

/* ======================= scan (all exes, by PE structure) ======================= */
static BOOL file_exists(const char* p){ DWORD a=GetFileAttributesA(p); return a!=0xFFFFFFFF; }
static void join(char* dst,const char* a,const char* b){ m_strcpy(dst,a); m_strcat(dst,"\\"); m_strcat(dst,b); }

static int row_text(int kind,const char* s){
    int i=g_nrows++; g_rows[i].kind=kind; m_strcpy(g_rows[i].text,s);
    SendMessageA(g_hList,LB_ADDSTRING,0,(LPARAM)i); return i;
}
static void row_kernel(const char* name,int cpu,int lic,int pae){
    int i=g_nrows++; ROW* r=&g_rows[i]; r->kind=1; m_strcpy(r->name,name);
    r->cpu=cpu; r->lic=lic; r->pae=pae;
    SendMessageA(g_hList,LB_ADDSTRING,0,(LPARAM)i);
}
/* hal.dll as its own selectable row (kind=2). Its status (cpu field) uses the SAME
 * 0/1/2 scheme as a kernel: 0 n/a (not an MPS HAL), 1 can patch, 2 already patched. */
static void row_hal(const char* name,int status){
    int i=g_nrows++; ROW* r=&g_rows[i]; r->kind=2; m_strcpy(r->name,name);
    r->cpu=status; r->lic=0; r->pae=0;
    SendMessageA(g_hList,LB_ADDSTRING,0,(LPARAM)i);
}
/* fast reject: read only the PE header to get the subsystem */
static WORD quick_subsystem(const char* path){
    HANDLE h=CreateFileA(path,GENERIC_READ,1,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(h==INVALID_HANDLE_VALUE) return 0;
    BYTE hd[0x400]; DWORD got=0; ReadFile(h,hd,0x400,&got,0); CloseHandle(h);
    if(got<0x40||hd[0]!='M'||hd[1]!='Z') return 0;
    DWORD pe=hd[0x3c]|(hd[0x3d]<<8)|(hd[0x3e]<<16)|((DWORD)hd[0x3f]<<24);
    if(pe+0x5c>got) return 0;
    if(hd[pe]!='P'||hd[pe+1]!='E') return 0;
    return hd[pe+0x18+0x44]|(hd[pe+0x18+0x45]<<8);
}
/* MPS HAL multi-CPU detection: read %sys32%\hal.dll and classify it. Only meaningful on
 * NT 3.1 / 3.5x / 4.0 (2000 uses its own ACPI HAL and needs no CPU HAL patch here). */
static void hal_detect(void){
    g_hal_status=0; g_hal_present=0;
    if(g_is9x || !(g_major==3 || g_major==4)) return;   /* HAL CPU-fix targets NT 3.x / 4.0 */
    char path[320]; join(path,g_sys32,"hal.dll");
    DWORD sz=0; BYTE* b=read_file(path,&sz);
    if(!b) return;
    g_hal_present=1;
    gbuf=b; gsz=sz;
    g_hal_status=hal_status();
    free_file(b);
}
static void do_scan(void){
    hal_detect();                       /* refresh MPS HAL status (hal.dll) */
    SendMessageA(g_hList,LB_RESETCONTENT,0,0);
    g_nrows=0;
    if(!g_supported){
        row_text(0, g_is9x ? "Windows 9x/Me has no NT kernel - nothing to patch here."
                           : "This Windows version is not supported by this patcher.");
        row_text(0,"Only NT 3.1 / 3.5x / 4.0 / 2000 kernels can be patched.");
        sel_first_kernel();     /* no kernel rows -> options + Patch stay disabled */
        return;
    }
    /* Scan EVERY native-subsystem *.exe and *.dll in system32 and keep ONLY the multiprocessor images:
     *   pass 0 (*.exe): MP kernels   -> is_kernel_native() AND a CPU clamp is present (cpu_status != 0).
     *   pass 1 (*.dll): MPS HALs     -> hal_status() != 0 (a multi-CPU HAL).
     * Uniprocessor kernels/HALs and every other DLL are skipped. There is no selection - Patch patches
     * the MP kernel and the MP HAL it finds, together. */
    int nk=0, nh=0;
    for(int pass=0; pass<2; pass++){
        char pat[320]; m_strcpy(pat,g_sys32); m_strcat(pat, pass ? "\\*.dll" : "\\*.exe");
        WIN32_FIND_DATAA fd; HANDLE h=FindFirstFileA(pat,&fd);
        if(h==INVALID_HANDLE_VALUE) continue;
        do{
            if(fd.dwFileAttributes&0x10) continue;          /* directory */
            char path[320]; join(path,g_sys32,fd.cFileName);
            if(quick_subsystem(path)!=1) continue;          /* native-subsystem PEs only */
            DWORD sz=0; BYTE* b=read_file(path,&sz);
            if(!b) continue;
            gbuf=b; gsz=sz;
            if(pass==0){
                if(is_kernel_native()){
                    int cpu=cpu_status();                   /* 0 = UP kernel, 1 can patch, 2 patched */
                    if(cpu!=0 && g_nrows<40){
                        int lic=lic_status(), pae=g_is2000?pae_status():0;
                        row_kernel(fd.cFileName,cpu,lic,pae); nk++;
                    }
                }
            } else {
                int hs=hal_status();                        /* 0 = not MPS HAL, 1 can patch, 2 patched */
                if(hs!=0 && g_nrows<40){ row_hal(fd.cFileName,hs); nh++; }
            }
            free_file(b);
        } while(FindNextFileA(h,&fd));
        FindClose(h);
    }
    if(!nk && !nh){
        row_text(0,"No multiprocessor (MP) kernel or MPS HAL found in system32.");
        row_text(0,"This machine has only uniprocessor images - nothing to patch for >2 CPUs.");
    } else {
        row_text(0,"");
        row_text(0,"Patch patches the MP kernel and the MP HAL above together (no selection needed).");
        row_text(0,"Legend:  check = already patched   arrow = will be patched");
    }
    update_opts();
}

/* ======================= boot partition ======================= */
/* The boot files (NTLDR, NTDETECT.COM, boot.ini) live on the SYSTEM/boot partition,
 * which is not always the Windows drive. Find the drive that actually holds NTLDR. */
static void find_boot_partition(void){
    DWORD mask=GetLogicalDrives();
    for(int i=2;i<26;i++){                       /* skip A: and B: (floppies) */
        if(!(mask&(1u<<i))) continue;
        char root[8]; root[0]='A'+i; root[1]=':'; root[2]='\\'; root[3]=0;
        if(GetDriveTypeA(root)!=DRIVE_FIXED) continue;   /* fixed disks only */
        char p[20]; m_strcpy(p,root); m_strcat(p,"NTLDR");
        if(file_exists(p)){ m_strcpy(g_bootdrv,root); return; }
    }
    /* fallback: the Windows drive */
    g_bootdrv[0]=g_sysdir[0]; g_bootdrv[1]=':'; g_bootdrv[2]='\\'; g_bootdrv[3]=0;
}

/* ======================= NTLDR / NTDETECT.COM replace ======================= */
/* Back up <boot>\<fname> to <fname>.OLD (clearing hidden/system/readonly), then
 * write the embedded replacement. Returns 1 ok, 0 fail. */
static int replace_loader(const char* fname, const BYTE* data, DWORD len){
    char dst[24]; m_strcpy(dst,g_bootdrv); m_strcat(dst,fname);
    DWORD at=GetFileAttributesA(dst);
    if(at!=0xFFFFFFFF){
        char bak[32]; m_strcpy(bak,dst); m_strcat(bak,".OLD");
        SetFileAttributesA(dst,FILE_ATTRIBUTE_NORMAL);
        DWORD ba=GetFileAttributesA(bak);
        /* Keep the first rollback copy across repeated patch operations. */
        if(ba==0xFFFFFFFF && !CopyFileA(dst,bak,1)) return 0;
    }
    if(!write_file(dst,data,len)){ return 0; }
    SetFileAttributesA(dst,FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_READONLY);
    return 1;
}

/* ======================= boot.ini ======================= */
static void bootini_path(char* out){ m_strcpy(out,g_bootdrv); m_strcat(out,"boot.ini"); }
static int contains_ci(const char* hay,const char* needle){
    int nl=m_strlen(needle);
    for(int i=0;hay[i];i++){ int k=0;
        while(k<nl){ char a=hay[i+k],b=needle[k]; if(a>='A'&&a<='Z')a+=32; if(b>='A'&&b<='Z')b+=32; if(a!=b)break; k++; }
        if(k==nl) return 1;
    }
    return 0;
}
/* replace-or-add a "KEY=val" switch (KEY e.g. "/KERNEL=" or "/HAL=") inside a switch string */
static void set_sw(char* sw, const char* key, const char* val){
    int n=m_strlen(sw), kl=m_strlen(key), pos=-1;
    for(int i=0;i+kl<=n;i++){ if(m_stricmp_n(&sw[i],key,kl)==0){ pos=i; break; } }
    char tail[256]; tail[0]=0;
    if(pos>=0){ int e=pos+kl; while(sw[e]&&sw[e]!=' ') e++; m_strcpy(tail,&sw[e]); if(pos>0&&sw[pos-1]==' ') pos--; sw[pos]=0; }
    m_strcat(sw," "); m_strcat(sw,key); m_strcat(sw,val);
    if(tail[0]) m_strcat(sw,tail);
}
/* boot.ini: clone the CURRENTLY-BOOTED ARC line (matched by the booted %SystemRoot% leaf, not
 * default=) and insert the clone DIRECTLY ABOVE its source line; add/replace /KERNEL= and, when
 * a patched HAL was written, /HAL=; add /PAE when applied. Previous runs of ours are de-duped. */
static int bootini_add(const char* kernelname, const char* halname, int pae_applied, const char* label){
    char path[32]; bootini_path(path);
    DWORD sz=0; BYTE* raw=read_file(path,&sz);
    if(!raw) return 2;
    static char txt[8192]; if(sz>sizeof(txt)-1) sz=sizeof(txt)-1;
    memcpy(txt,raw,sz); txt[sz]=0; free_file(raw);
    /* ADD-ONLY: never delete or replace existing boot.ini lines (the user may keep their own
     * [CPU] / custom entries). We only append a new entry, and below we skip the append if an
     * identical entry already exists, so re-running does not accumulate duplicates. */
    /* booted system-root leaf, e.g. "\WINNT351" */
    char leaf[72]; { int p=m_strlen(g_sysdir); while(p>0&&g_sysdir[p-1]!='\\')p--; leaf[0]='\\'; m_strcpy(leaf+1,&g_sysdir[p]); }
    int ll=m_strlen(leaf);
    /* find the source ARC line: col-0 start, is an ARC path, references the booted leaf, and has
     * no custom /KERNEL yet (so we never clone one of our own). */
    int src=-1,se=-1;
    for(int i=0;txt[i];i++){
        if(!(i==0||txt[i-1]=='\n')) continue;
        int ls=i, e=i; while(txt[e]&&txt[e]!='\r'&&txt[e]!='\n') e++;
        /* ARC entry lines START with the ARC path (after optional leading space); this must NOT
         * match the "default=multi(..)" line in [boot loader], which contains multi( mid-line. */
        int ls2=ls; while(ls2<e&&(txt[ls2]==' '||txt[ls2]=='\t')) ls2++;
        int isarc = (m_stricmp_n(&txt[ls2],"multi(",6)==0||m_stricmp_n(&txt[ls2],"scsi(",5)==0);
        if(isarc){
            int hasleaf=0; for(int k=ls;k+ll<=e;k++){ if(m_stricmp_n(&txt[k],leaf,ll)==0){ hasleaf=1; break; } }
            int hascustom=0; for(int k=ls;k<e;k++){ if(m_stricmp_n(&txt[k],"/KERNEL=",8)==0){ hascustom=1; break; } }
            if(hasleaf && !hascustom){ src=ls; se=e; break; }
        }
        i=e;
    }
    if(src<0) return 3;
    int eq=-1; for(int i=src;i<se;i++){ if(txt[i]=='='){ eq=i; break; } }
    if(eq<0) return 3;
    char arc[256]; { int j=0; for(int i=src;i<eq&&j<255;i++) arc[j++]=txt[i]; arc[j]=0; }
    int q1=-1,q2=-1; for(int i=eq;i<se;i++){ if(txt[i]=='"'){ if(q1<0)q1=i; else {q2=i;break;} } }
    char desc[256]; desc[0]=0; if(q1>=0&&q2>q1){ int j=0; for(int i=q1+1;i<q2&&j<255;i++) desc[j++]=txt[i]; desc[j]=0; }
    char sw[256]; sw[0]=0; if(q2>=0){ int j=0; for(int i=q2+1;i<se&&j<255;i++) sw[j++]=txt[i]; sw[j]=0; }
    char ndesc[300]; m_strcpy(ndesc,desc); m_strcat(ndesc," "); m_strcat(ndesc,label);
    if(kernelname && kernelname[0]) set_sw(sw,"/KERNEL=",kernelname);
    if(halname && halname[0]) set_sw(sw,"/HAL=",halname);
    if(pae_applied && !contains_ci(sw,"/pae")) m_strcat(sw," /PAE");
    char nl[800];
    m_strcpy(nl,arc); m_strcat(nl,"=\""); m_strcat(nl,ndesc); m_strcat(nl,"\""); m_strcat(nl,sw); m_strcat(nl,"\r\n");
    /* skip if an identical entry already exists (avoid duplicate on re-run; never touch others) */
    { int nllen=m_strlen(nl); while(nllen>0&&(nl[nllen-1]=='\r'||nl[nllen-1]=='\n'||nl[nllen-1]==' ')) nllen--;
      int i=0; while(txt[i]){ int ls=i; while(txt[i]&&txt[i]!='\n') i++; int le=i;
        while(le>ls&&(txt[le-1]=='\r'||txt[le-1]=='\n'||txt[le-1]==' ')) le--;
        if(le-ls==nllen){ int same=1; for(int k=0;k<nllen;k++) if(txt[ls+k]!=nl[k]){ same=0; break; } if(same) return 0; }
        if(txt[i]=='\n') i++; }
    }
    static char outbuf[8192+900]; int o=0;
    for(int i=0;i<(int)sz;i++){ if(i==src){ for(int j=0;nl[j];j++) outbuf[o++]=nl[j]; } outbuf[o++]=txt[i]; }
    if(src>=(int)sz){ for(int j=0;nl[j];j++) outbuf[o++]=nl[j]; }
    outbuf[o]=0;
    DWORD at=GetFileAttributesA(path);
    SetFileAttributesA(path,FILE_ATTRIBUTE_NORMAL);
    BOOL ok=write_file(path,(const BYTE*)outbuf,o);
    /* leave boot.ini editable: keep hidden/system if it had them, but never READONLY */
    if(at!=0xFFFFFFFF) SetFileAttributesA(path,at&~FILE_ATTRIBUTE_READONLY);
    else SetFileAttributesA(path,FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM);
    return ok?0:4;
}

/* ======================= registry (live) ======================= */
static int reg_set_procs(DWORD n){
    HKEY hk; int ok=0;
    if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Control\\Session Manager",0,KEY_SET_VALUE,&hk)==ERROR_SUCCESS){
        if(RegSetValueExA(hk,"RegisteredProcessors",0,REG_DWORD,(const BYTE*)&n,4)==ERROR_SUCCESS) ok++;
        if(RegSetValueExA(hk,"LicensedProcessors", 0,REG_DWORD,(const BYTE*)&n,4)==ERROR_SUCCESS) ok++;
        RegCloseKey(hk);
    }
    return ok;
}

/* ======================= patch & install ======================= */
static int chk(HWND hwnd,int id){ return SendMessageA(GetDlgItem(hwnd,id),BM_GETCHECK,0,0)==BST_CHECKED; }
static int pae_gb(HWND hwnd){
    int i=(int)SendMessageA(GetDlgItem(hwnd,IDC_PAEMAX),CB_GETCURSEL,0,0);
    static const int tbl[4]={8,16,32,64};
    if(i<0||i>3) i=2; return tbl[i];
}
/* Patch hal.dll on its own (user picked the hal.dll row): ACPI CPU-detect + >2-CPU AP
 * bring-up -> HALMPCPU.DLL, plus a boot.ini entry that adds /HAL= to the booted line. */
static void do_patch_hal_only(HWND hwnd){
    if(g_hal_status!=1){
        MessageBoxA(hwnd,g_hal_status==2?"hal.dll is already patched.":"hal.dll is not a patchable MPS HAL.",
                    "NT CPU and PAE Patcher",MB_OK|MB_ICONINFORMATION); return;
    }
    char hpath[320]; join(hpath,g_sys32,"hal.dll");
    DWORD hsz=0; BYTE* hb=read_file(hpath,&hsz);
    if(!hb){ MessageBoxA(hwnd,"Cannot read hal.dll.","NT CPU and PAE Patcher",MB_OK|MB_ICONERROR); return; }
    gbuf=hb; gsz=hsz;
    int ok=hal_apply();
    if(!ok){ free_file(hb); MessageBoxA(hwnd,"Nothing applicable to patch in hal.dll.","NT CPU and PAE Patcher",MB_OK|MB_ICONINFORMATION); return; }
    pe_checksum_fix();
    char hop[320]; join(hop,g_sys32,"HALMPCPU.DLL");
    BOOL wok=write_file(hop,gbuf,gsz);
    free_file(hb);
    if(!wok){ MessageBoxA(hwnd,"Failed to write HALMPCPU.DLL.","NT CPU and PAE Patcher",MB_OK|MB_ICONERROR); return; }
    int reg=reg_set_procs(g_major<=4?8:32);  /* NT 3.x/4.0 MPS HAL max = 8; 2000+ keeps 32 */
    int bi=bootini_add(0, "HALMPCPU.DLL", 0, "[MP HAL]");   /* kernelname NULL -> /HAL= only */
    char msg[600],tmp[200];
    m_strcpy(msg,"MPS HAL patched (ACPI CPU-detect + >2-CPU fix):\n  ");
    m_strcat(msg,g_sys32); m_strcat(msg,"\\HALMPCPU.DLL\n(original hal.dll untouched)\n");
    wsprintfA(tmp,"\nRegistry proc=%d : %s",g_major<=4?8:32,reg>=2?"set":(reg==1?"partial":"not set")); m_strcat(msg,tmp);
    wsprintfA(tmp,"\nboot partition  : %s",g_bootdrv); m_strcat(msg,tmp);
    if(bi==0) m_strcpy(tmp,"\nboot.ini        : cloned booted entry above itself, /HAL=HALMPCPU.DLL");
    else if(bi==2) m_strcpy(tmp,"\nboot.ini        : NOT FOUND");
    else if(bi==4) m_strcpy(tmp,"\nboot.ini        : write failed");
    else           m_strcpy(tmp,"\nboot.ini        : parse failed");
    m_strcat(msg,tmp);
    m_strcat(msg,"\n\nNote: use this with a multi-CPU (MP) kernel. Reboot and pick the new entry.");
    MessageBoxA(hwnd,msg,"NT CPU and PAE Patcher - done",MB_OK|MB_ICONINFORMATION);
    do_scan();
}
static void do_patch(HWND hwnd){
    /* No manual selection: auto-pick the primary PATCHABLE MP kernel (first kind==1, status 1) and the
     * primary PATCHABLE MP HAL (first kind==2, status 1) from the scan, and patch both together. */
    int kr=-1, hr=-1, any_mp=0;
    for(int i=0;i<g_nrows;i++){
        if(g_rows[i].kind==1){ any_mp=1; if(kr<0 && g_rows[i].cpu==1) kr=i; }
        if(g_rows[i].kind==2){ any_mp=1; if(hr<0 && g_rows[i].cpu==1) hr=i; }
    }
    if(kr<0 && hr<0){
        MessageBoxA(hwnd, any_mp ? "The MP kernel and HAL found are already patched."
                                 : "No multiprocessor (MP) kernel or MPS HAL found to patch.",
                    "NT CPU and PAE Patcher",MB_OK|MB_ICONINFORMATION);
        return;
    }
    int want_cpu = IsWindowEnabled(GetDlgItem(hwnd,IDC_CPU)) && chk(hwnd,IDC_CPU);
    int want_lic = IsWindowEnabled(GetDlgItem(hwnd,IDC_LIC)) && chk(hwnd,IDC_LIC);
    int want_pae = IsWindowEnabled(GetDlgItem(hwnd,IDC_PAE)) && chk(hwnd,IDC_PAE);
    int want_ldr = IsWindowEnabled(GetDlgItem(hwnd,IDC_LDR)) && chk(hwnd,IDC_LDR);
    int gb=pae_gb(hwnd);

    /* ---- patch the MP kernel ---- */
    const char* outname=0; char label[16]=""; int cpu=0,lic=0,pae=0,pfn=0; char ksrc[64]=""; 
    if(kr>=0){
        m_strcpy(ksrc,g_rows[kr].name);
        char path[320]; join(path,g_sys32,ksrc);
        DWORD sz=0; BYTE* b=read_file(path,&sz);
        if(b&&sz>=0x200&&b[0]=='M'&&b[1]=='Z'){
            gbuf=b; gsz=sz; n_used=0;          /* fresh cave bookkeeping for this file */
            /* 0x9A entry-stub first so it reserves its cave, THEN the PFN guard (matches the CLI
             * carve order -> byte-identical output when both CPU and LIC are applied). */
            if(want_lic){ int hook=entry_hook_9a(); int n9a=neutralize_9a(); lic=(hook||n9a>0); }
            if(want_cpu){ cpu=cpu_apply(); pfn=kernel_pfn_guard(); if(pfn<0){ free_file(b); MessageBoxA(hwnd,"The NT 3.1 PFN guard could not be validated. No kernel or boot entry was written.","NT CPU and PAE Patcher",MB_OK|MB_ICONINFORMATION); return; } }  /* +MiBuildPfnDatabase bare-metal 0x50 guard */
            if(want_pae && pae_capable()) pae=(pae_apply(gb)>0);
            if(cpu||lic||pae){
                pe_checksum_fix();
                if(pae&&cpu){ outname="NTCPUPAE.EXE"; m_strcpy(label,"[PAE+CPU]"); }
                else if(pae){ outname="NTKRPAE.EXE";  m_strcpy(label,"[PAE]"); }
                else if(cpu){ outname="NTKRCPU.EXE";  m_strcpy(label,"[CPU]"); }
                else        { outname="NTKRLIC.EXE";  m_strcpy(label,"[LIC]"); }
                char outpath[320]; join(outpath,g_sys32,outname);
                if(!write_file(outpath,gbuf,gsz)) outname=0;
            }
            free_file(b);
        }
    }
    /* ---- patch the MP HAL (ACPI CPU-detect + >2-CPU AP bring-up) -> HALMPCPU.DLL ---- */
    char halout[16]=""; int hal_done=0; char hsrc[64]="";
    if(hr>=0){
        m_strcpy(hsrc,g_rows[hr].name);
        char hpath[320]; join(hpath,g_sys32,hsrc);
        DWORD hsz=0; BYTE* hb=read_file(hpath,&hsz);
        if(hb){
            gbuf=hb; gsz=hsz;
            if(hal_apply()){ pe_checksum_fix();
                m_strcpy(halout,"HALMPCPU.DLL");
                char hop[320]; join(hop,g_sys32,halout);
                if(write_file(hop,gbuf,gsz)) hal_done=1; else halout[0]=0;
            }
            free_file(hb);
        }
    }
    if(!outname && !hal_done){
        MessageBoxA(hwnd,"Nothing applicable to patch (already patched, or unsupported build).","NT CPU and PAE Patcher",MB_OK|MB_ICONINFORMATION);
        return;
    }

    int reg=reg_set_procs(g_major<=4?8:32);  /* NT 3.x/4.0 MPS HAL max = 8; 2000+ keeps 32 */
    int bi=bootini_add(outname?outname:0, halout[0]?halout:0, pae, label[0]?label:"[MP]");
    int ldr=0,det=0;
    if(want_ldr){
        ldr=replace_loader("NTLDR",g_ntldr,g_ntldr_LEN);
        det=replace_loader("NTDETECT.COM",g_ntdetect,g_ntdetect_LEN);
    }

    char msg[1024],tmp[200];
    m_strcpy(msg,"Patched for multiprocessor boot:\n");
    if(outname){ wsprintfA(tmp,"  kernel : %s  ->  %s\n",ksrc,outname); m_strcat(msg,tmp); }
    if(hal_done){ wsprintfA(tmp,"  MP HAL : %s  ->  %s\n",hsrc,halout); m_strcat(msg,tmp); }
    m_strcat(msg,"\nApplied:");
    wsprintfA(tmp,"\n  Sparse PFN    : %s",pfn?"guarded":"not applicable / skipped"); m_strcat(msg,tmp);
    wsprintfA(tmp,"\n  CPU clamp     : %s", cpu?"removed":(want_cpu?"n/a / already":"skipped")); m_strcat(msg,tmp);
    wsprintfA(tmp,"\n  License 0x9A  : %s", lic?"fixed":(want_lic?"n/a":"skipped")); m_strcat(msg,tmp);
    if(pae) wsprintfA(tmp,"\n  PAE >4GB      : enabled (max %dGB)",gb);
    else    wsprintfA(tmp,"\n  PAE >4GB      : %s", want_pae?"n/a / already":"skipped");
    m_strcat(msg,tmp);
    wsprintfA(tmp,"\n  MPS HAL       : %s", hal_done?"patched (ACPI CPU-detect + >2-CPU fix)":"n/a / already"); m_strcat(msg,tmp);
    wsprintfA(tmp,"\n\nRegistry proc=%d : %s", g_major<=4?8:32, reg>=2?"set":(reg==1?"partial":"not set")); m_strcat(msg,tmp);
    wsprintfA(tmp,"\nboot partition  : %s", g_bootdrv); m_strcat(msg,tmp);
    if(bi==0)      m_strcpy(tmp,"\nboot.ini        : new MP entry cloned above the booted line");
    else if(bi==2) m_strcpy(tmp,"\nboot.ini        : NOT FOUND");
    else if(bi==4) m_strcpy(tmp,"\nboot.ini        : write failed");
    else           m_strcpy(tmp,"\nboot.ini        : parse failed");
    m_strcat(msg,tmp);
    if(want_ldr){ wsprintfA(tmp,"\nNTLDR/NTDETECT  : %s (old -> *.OLD)",(ldr&&det)?"replaced":"FAILED"); m_strcat(msg,tmp); }
    m_strcat(msg,"\n\nReboot and pick the new entry.");
    MessageBoxA(hwnd,msg,"NT CPU and PAE Patcher - done",MB_OK|MB_ICONINFORMATION);
    do_scan();
}
/* ======================= owner-draw badge ======================= */
/* Palette-safe SOLID colors only (16-color VGA safe) so nothing dithers to a
 * noisy white box at low color depth. */
/* EXACT 16-color VGA palette entries only (idx2 green, idx9 blue, idx8 gray) so
 * nothing dithers to a noisy near-white box at 16-color depth. */
#define CLR_DONE RGB(0,128,0)      /* green  (VGA idx 2)   */
#define CLR_CAN  RGB(0,0,255)      /* blue   (VGA idx 9)   */
#define CLR_NA   RGB(128,128,128)  /* gray   (VGA idx 8)   */
static void draw_badge(HDC dc,int x,int y,int status,const char* label,int sel){
    COLORREF col = status==2?CLR_DONE: status==1?CLR_CAN: CLR_NA;
    RECT r; r.left=x; r.top=y; r.right=x+14; r.bottom=y+14;
    HBRUSH br=CreateSolidBrush(col); FillRect(dc,&r,br); DeleteObject((HGDIOBJ)br);
    HPEN pn=CreatePen(0,1,RGB(0,0,0)); HGDIOBJ op=SelectObject(dc,(HGDIOBJ)pn), ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
    Rectangle(dc,x,y,x+14,y+14);
    SelectObject(dc,ob); SelectObject(dc,op); DeleteObject((HGDIOBJ)pn);
    HPEN wp=CreatePen(0,1,RGB(255,255,255)); HGDIOBJ op2=SelectObject(dc,(HGDIOBJ)wp);
    if(status==2){ POINT p[3]={{x+3,y+7},{x+6,y+10},{x+11,y+4}}; Polyline(dc,p,3); }
    else if(status==1){ POINT p[3]={{x+4,y+9},{x+7,y+4},{x+10,y+9}}; Polyline(dc,p,3); }
    else { MoveToEx(dc,x+4,y+7,0); LineTo(dc,x+11,y+7); }
    SelectObject(dc,op2); DeleteObject((HGDIOBJ)wp);
    SetBkMode(dc,TRANSPARENT);
    SetTextColor(dc, sel?RGB(255,255,255):col);
    SelectObject(dc,(HGDIOBJ)g_font);
    char buf[32]; const char* s=status==2?"applied":status==1?"available":"n/a";
    wsprintfA(buf,"%s %s",label,s);
    TextOutA(dc,x+19,y+0,buf,m_strlen(buf));
}
static void set_font(HWND h,HFONT f){ SendMessageA(h,WM_SETFONT,(WPARAM)f,1); }

/* Enable/disable each patch option to match the SELECTED kernel's capabilities,
 * and tick the ones that can actually be applied. A kernel that cannot take a
 * given patch (UP kernel -> no CPU/License, non-PAE kernel -> no PAE) greys it. */
static void update_opts(void){
    HWND h=g_hwnd; if(!h) return;
    /* No manual selection: the options reflect the PRIMARY MP kernel found by the scan (first kind==1
     * row). All MP kernels in the list are patched with these same options; Patch is enabled whenever
     * any MP kernel OR MP HAL was found. */
    int kr=-1, has_hal=0;
    for(int i=0;i<g_nrows;i++){
        if(kr<0 && g_rows[i].kind==1) kr=i;
        if(g_rows[i].kind==2) has_hal=1;
    }
    int is_k = kr>=0;
    int kcpu = is_k?g_rows[kr].cpu:0, klic = is_k?g_rows[kr].lic:0, kpae = is_k?g_rows[kr].pae:0;
    int en_cpu = is_k && kcpu!=0;                 /* 0 = n/a (uniprocessor)      */
    int en_lic = is_k && klic!=0;
    int en_pae = is_k && g_pae_ok && kpae!=0;     /* 0 = non-PAE kernel          */
    int en_ldr = is_k && (g_is2000 || (g_major==3 && (g_minor==1 || g_minor==10)));                /* NT 3.1 stock loader ignores /KERNEL= and /HAL=. */
    EnableWindow(GetDlgItem(h,IDC_CPU),en_cpu);
    EnableWindow(GetDlgItem(h,IDC_LIC),en_lic);
    EnableWindow(GetDlgItem(h,IDC_PAE),en_pae);
    EnableWindow(GetDlgItem(h,IDC_PAEMAX),en_pae);
    EnableWindow(GetDlgItem(h,IDC_PMLBL),en_pae);
    EnableWindow(GetDlgItem(h,IDC_LDR),en_ldr);
    /* auto-tick patches that are applicable-but-not-yet-applied (status 1) */
    SendMessageA(GetDlgItem(h,IDC_CPU),BM_SETCHECK,(en_cpu&&kcpu==1)?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageA(GetDlgItem(h,IDC_LIC),BM_SETCHECK,(en_lic&&klic==1)?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageA(GetDlgItem(h,IDC_PAE),BM_SETCHECK,(en_pae&&kpae==1)?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageA(GetDlgItem(h,IDC_LDR),BM_SETCHECK,en_ldr?BST_CHECKED:BST_UNCHECKED,0);
    /* Patch is enabled whenever the scan found any MP kernel or MP HAL to patch. */
    EnableWindow(GetDlgItem(h,IDC_PATCH), is_k || has_hal);
}
/* Select the first kernel row (if any) and sync the options to it. */
static void sel_first_kernel(void){
    int k=-1;
    for(int i=0;i<g_nrows;i++){ if(g_rows[i].kind==1){ k=i; break; } }
    g_sel=k;
    if(k>=0) SendMessageA(g_hList,LB_SETCURSEL,(WPARAM)k,0);
    else     SendMessageA(g_hList,LB_SETCURSEL,(WPARAM)-1,0);
    update_opts();
}

/* ======================= window ======================= */
static LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:{
        g_hwnd=hwnd;
        g_bg=CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
        g_fontT=CreateFontA(-19,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,0,VARIABLE_PITCH|FF_SWISS,"MS Sans Serif");
        g_fontB=CreateFontA(-11,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,0,VARIABLE_PITCH|FF_SWISS,"MS Sans Serif");
        g_font =CreateFontA(-11,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,VARIABLE_PITCH|FF_SWISS,"MS Sans Serif");

        g_hTitle=CreateWindowExA(0,"STATIC","NT CPU and PAE Patcher",WS_CHILD|WS_VISIBLE|SS_LEFT,50,12,400,24,hwnd,(HMENU)IDC_TITLE,g_hInst,0);
        g_hStatic=CreateWindowExA(0,"STATIC","",WS_CHILD|WS_VISIBLE|SS_LEFT,14,42,592,16,hwnd,(HMENU)IDC_STATIC,g_hInst,0);

        CreateWindowExA(0,"BUTTON","MP kernels and MP HALs in system32",WS_CHILD|WS_VISIBLE|BS_GROUPBOX,8,64,604,152,hwnd,(HMENU)IDC_GROUP,g_hInst,0);
        g_hList=CreateWindowExA(WS_BORDER,"LISTBOX","",WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_TABSTOP|LBS_DISABLENOSCROLL|LBS_NOTIFY|LBS_OWNERDRAWFIXED,20,82,580,126,hwnd,(HMENU)IDC_LIST,g_hInst,0);
        SendMessageA(g_hList,LB_SETITEMHEIGHT,0,(LPARAM)22);

        CreateWindowExA(0,"BUTTON","Patch options",WS_CHILD|WS_VISIBLE|BS_GROUPBOX,8,222,604,148,hwnd,(HMENU)IDC_OPTS,g_hInst,0);
        CreateWindowExA(0,"BUTTON","CPU core limit  (boot > 2 CPUs)",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,22,242,290,18,hwnd,(HMENU)IDC_CPU,g_hInst,0);
        CreateWindowExA(0,"BUTTON","License fix  (STOP 0x9A)",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,22,266,290,18,hwnd,(HMENU)IDC_LIC,g_hInst,0);
        CreateWindowExA(0,"BUTTON","PAE  (use RAM above 4 GB)",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,22,290,240,18,hwnd,(HMENU)IDC_PAE,g_hInst,0);
        CreateWindowExA(0,"STATIC","PAE max:",WS_CHILD|WS_VISIBLE|SS_LEFT,286,292,50,16,hwnd,(HMENU)IDC_PMLBL,g_hInst,0);
        CreateWindowExA(0,"COMBOBOX","",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,338,289,88,140,hwnd,(HMENU)IDC_PAEMAX,g_hInst,0);
        CreateWindowExA(0,"BUTTON","Replace NTLDR + NTDETECT.COM  (needed for /KERNEL=)",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,22,316,470,18,hwnd,(HMENU)IDC_LDR,g_hInst,0);

        CreateWindowExA(0,"BUTTON","Re-scan",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,10,380,140,30,hwnd,(HMENU)IDC_SCAN,g_hInst,0);
        CreateWindowExA(0,"BUTTON","Patch && Install",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,160,380,190,30,hwnd,(HMENU)IDC_PATCH,g_hInst,0);

        set_font(g_hTitle,g_fontT); set_font(g_hStatic,g_fontB);
        set_font(GetDlgItem(hwnd,IDC_GROUP),g_fontB); set_font(GetDlgItem(hwnd,IDC_OPTS),g_fontB);
        set_font(g_hList,g_font);
        set_font(GetDlgItem(hwnd,IDC_CPU),g_font); set_font(GetDlgItem(hwnd,IDC_LIC),g_font);
        set_font(GetDlgItem(hwnd,IDC_PAE),g_font); set_font(GetDlgItem(hwnd,IDC_PMLBL),g_font);
        set_font(GetDlgItem(hwnd,IDC_PAEMAX),g_font); set_font(GetDlgItem(hwnd,IDC_LDR),g_font);
        set_font(GetDlgItem(hwnd,IDC_SCAN),g_font); set_font(GetDlgItem(hwnd,IDC_PATCH),g_fontB);

        detect_os(); find_boot_partition(); show_osinfo();

        /* PAE-max choices; per-kernel enable/check is driven by update_opts() */
        { HWND cb=GetDlgItem(hwnd,IDC_PAEMAX);
          SendMessageA(cb,CB_ADDSTRING,0,(LPARAM)"8 GB");
          SendMessageA(cb,CB_ADDSTRING,0,(LPARAM)"16 GB");
          SendMessageA(cb,CB_ADDSTRING,0,(LPARAM)"32 GB");
          SendMessageA(cb,CB_ADDSTRING,0,(LPARAM)"64 GB");
          SendMessageA(cb,CB_SETCURSEL,2,0); }                 /* default 32 GB */
        do_scan();      /* fills list + auto-selects first kernel + sets options */
        SetFocus(g_hList);   /* start on the list so a kernel can be picked at once */
        return 0; }
    case WM_DRAWITEM:{
        DRAWITEMSTRUCT* di=(DRAWITEMSTRUCT*)lp;
        if(di->CtlID!=IDC_LIST) break;
        int idx=(int)di->itemData; if(idx<0||idx>=g_nrows) return 1;
        ROW* r=&g_rows[idx];
        int sel=di->itemState&1;
        RECT rc=di->rcItem; HDC dc=di->hDC;
        HBRUSH bg=CreateSolidBrush(sel?GetSysColor(COLOR_HIGHLIGHT):RGB(255,255,255));
        FillRect(dc,&rc,bg); DeleteObject((HGDIOBJ)bg);
        SetBkMode(dc,TRANSPARENT);
        if(r->kind==1){
            SetTextColor(dc, sel?GetSysColor(COLOR_HIGHLIGHTTEXT):RGB(0,0,0));
            SelectObject(dc,(HGDIOBJ)g_fontB);
            TextOutA(dc,rc.left+10,rc.top+4,r->name,m_strlen(r->name));
            draw_badge(dc,rc.left+140,rc.top+4,r->cpu,"CPU",sel);
            draw_badge(dc,rc.left+290,rc.top+4,r->lic,"LIC",sel);
            draw_badge(dc,rc.left+440,rc.top+4,r->pae,"PAE",sel);
        } else if(r->kind==2){
            SetTextColor(dc, sel?GetSysColor(COLOR_HIGHLIGHTTEXT):RGB(0,0,0));
            SelectObject(dc,(HGDIOBJ)g_fontB);
            TextOutA(dc,rc.left+10,rc.top+4,r->name,m_strlen(r->name));
            draw_badge(dc,rc.left+140,rc.top+4,r->cpu,"MP CPU",sel);
        } else {
            SetTextColor(dc, sel?GetSysColor(COLOR_HIGHLIGHTTEXT):RGB(128,128,128));
            SelectObject(dc,(HGDIOBJ)g_font);
            TextOutA(dc,rc.left+12,rc.top+4,r->text,m_strlen(r->text));
        }
        return 1; }
    case WM_PAINT:{
        PAINTSTRUCT ps; HDC dc=BeginPaint(hwnd,&ps);
        if(g_icon) DrawIcon(dc,12,10,g_icon);     /* app icon next to the header title */
        EndPaint(hwnd,&ps);
        return 0; }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:{
        HDC dc=(HDC)wp; SetBkMode(dc,TRANSPARENT);
        if((HWND)lp==g_hTitle) SetTextColor(dc,RGB(0,0,128));   /* palette-safe navy */
        else                   SetTextColor(dc,RGB(0,0,0));
        return (LRESULT)g_bg;
    }
    case WM_COMMAND:{
        int id=wp&0xFFFF, code=(wp>>16)&0xFFFF;
        if(id==IDC_SCAN) do_scan();
        else if(id==IDC_PATCH) do_patch(hwnd);
        else if(id==IDC_LIST && code==LBN_SELCHANGE){
            int cur=(int)SendMessageA(g_hList,LB_GETCURSEL,0,0);
            g_sel = (cur<0)?-1:(int)SendMessageA(g_hList,LB_GETITEMDATA,cur,0);
            update_opts();
        }
        return 0; }
    case WM_CLOSE: PostQuitMessage(0); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd,msg,wp,lp);
}

void start(void){
    g_hInst=GetModuleHandleA(0);
    g_icon=LoadIconA(g_hInst,MAKEINTRESOURCEA(1));   /* app icon from .rsrc; works on NT 3.1+ */
    WNDCLASSA wc; memset(&wc,0,sizeof(wc));
    wc.style=CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc=WndProc;
    wc.hInstance=g_hInst;
    wc.hIcon=g_icon;                 /* 0 -> default icon; avoids NT3.1 LoadIconA crash */
    wc.hCursor=0;                    /* inherit arrow; avoids NT3.1 LoadCursorA crash */
    wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszClassName="NTPATCHCLS";
    RegisterClassA(&wc);
    DWORD osv=GetVersion(); DWORD exsty=((osv&0xFF)>=4)?0x10000:0;   /* WS_EX_CONTROLPARENT is NT4+ only */
    HWND hwnd=CreateWindowExA(exsty,"NTPATCHCLS","NT CPU and PAE Patcher - NT31 PFN fix 2026-10-30",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,626,454,0,0,g_hInst,0);
    if(g_icon){ SendMessageA(hwnd,WM_SETICON,ICON_BIG,(LPARAM)g_icon); SendMessageA(hwnd,WM_SETICON,ICON_SMALL,(LPARAM)g_icon); }
    ShowWindow(hwnd,SW_SHOWNORMAL);
    UpdateWindow(hwnd);
    MSG m;
    while(GetMessageA(&m,0,0,0)){ if(!IsDialogMessageA(hwnd,&m)){ TranslateMessage(&m); DispatchMessageA(&m); } }
    ExitProcess(0);
}
