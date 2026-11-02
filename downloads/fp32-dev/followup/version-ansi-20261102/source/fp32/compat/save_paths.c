/* Private saved-game namespace for FAT/Win32s-era 8.3 limits. The mapping is
   confined to FP32_USERDATA; all other paths retain native Win32 behavior. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
extern void WINAPI FpObserve(const char*,DWORD);
extern LPCWSTR fp_LegacyFilePath(LPCWSTR);
extern HANDLE WINAPI fp_BaseCreateFileW(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
extern BOOL WINAPI fp_BaseGetFileAttributesExW(LPCWSTR,GET_FILEEX_INFO_LEVELS,LPVOID);
#define MAP_MAGIC 0x314d5046u
#define MAP_FILE L"\\FP32MAP.DAT"
#define LOCK_FILE L"\\FP32LOCK.LCK"
typedef struct {DWORD magic;WCHAR alias[13];WCHAR name[256];DWORD checksum;} Record;
typedef struct Search {struct Search*next;HANDLE handle;WCHAR parent[MAX_PATH],pattern[256];} Search;
static Search*searches;static volatile LONG search_lock;
static void lock_search(void){while(InterlockedExchange(&search_lock,1))Sleep(0);}
static void unlock_search(void){InterlockedExchange(&search_lock,0);}
static void zero(void*p,DWORD n){DWORD i;for(i=0;i<n;i++)((BYTE*)p)[i]=0;}
static DWORD hash(const void*p,DWORD n){DWORD i,h=2166136261u;for(i=0;i<n;i++){h^=((BYTE*)p)[i];h*=16777619u;}return h;}
static BOOL join(WCHAR*out,LPCWSTR a,LPCWSTR b){DWORD n=lstrlenW(a),k=lstrlenW(b);if(n+k>=MAX_PATH){SetLastError(ERROR_FILENAME_EXCED_RANGE);return FALSE;}lstrcpyW(out,a);lstrcpyW(out+n,b);return TRUE;}
static BOOL valid_record(Record*r){DWORD i;if(r->magic!=MAP_MAGIC||r->checksum!=hash(r,FIELD_OFFSET(Record,checksum)))return FALSE;for(i=0;i<13&&r->alias[i];i++){}if(i!=8)return FALSE;for(i=0;i<8;i++)if(!((r->alias[i]>='0'&&r->alias[i]<='9')||(r->alias[i]>='A'&&r->alias[i]<='F')))return FALSE;for(i=0;i<256&&r->name[i];i++){}return i>0&&i<256;}
static HANDLE map_lock(LPCWSTR parent){WCHAR path[MAX_PATH];HANDLE h;DWORD start=GetTickCount(),e;if(!join(path,parent,LOCK_FILE))return INVALID_HANDLE_VALUE;for(;;){h=CreateFileW(path,GENERIC_READ,0,0,OPEN_ALWAYS,FILE_ATTRIBUTE_HIDDEN,0);if(h!=INVALID_HANDLE_VALUE)return h;e=GetLastError();if(e!=ERROR_SHARING_VIOLATION||GetTickCount()-start>5000){SetLastError(e);return INVALID_HANDLE_VALUE;}Sleep(1);}}
static int record_read(HANDLE h,Record*r){DWORD n;if(!ReadFile(h,r,sizeof(*r),&n,0))return -1;if(!n)return 0;if(n!=sizeof(*r)||!valid_record(r)){SetLastError(ERROR_INVALID_DATA);return -1;}return 1;}
/* The complete logical name is stored alongside each alias. A hash collision
   therefore selects another alias instead of silently sharing another save. */
static BOOL map_name(LPCWSTR parent,LPCWSTR name,BOOL create,BOOL reverse,LPWSTR out){WCHAR file[MAX_PATH],candidate[13],physical[MAX_PATH];HANDLE guard,h;Record row,newrow;DWORD e=0,seed,attempt,i,used,end;int status;BOOL found=FALSE,collision;
 if(!name[0]||lstrlenW(name)>255){SetLastError(ERROR_INVALID_NAME);return FALSE;}
 guard=map_lock(parent);if(guard==INVALID_HANDLE_VALUE)return FALSE;
 if(!join(file,parent,MAP_FILE)){e=GetLastError();goto done;}
 h=CreateFileW(file,GENERIC_READ|(create?GENERIC_WRITE:0),FILE_SHARE_READ,0,create?OPEN_ALWAYS:OPEN_EXISTING,FILE_ATTRIBUTE_HIDDEN,0);
 if(h==INVALID_HANDLE_VALUE){e=GetLastError();goto done;}
 while((status=record_read(h,&row))>0){
  if(!lstrcmpiW(reverse?row.alias:row.name,name)){lstrcpyW(out,reverse?row.name:row.alias);found=TRUE;goto close;}}
 if(status<0){e=GetLastError();goto close;}
 if(!create||reverse){e=ERROR_FILE_NOT_FOUND;goto close;}
 seed=hash(name,lstrlenW(name)*sizeof(WCHAR));
 for(attempt=0;attempt<65536;attempt++,seed+=0x9e3779b9u){
  for(i=0;i<8;i++)candidate[i]=L"0123456789ABCDEF"[(seed>>(28-4*i))&15];candidate[8]=0;collision=FALSE;
  SetFilePointer(h,0,0,FILE_BEGIN);
  while((status=record_read(h,&row))>0){if(!lstrcmpiW(row.alias,candidate)){collision=TRUE;break;}}
  if(status<0){e=GetLastError();goto close;}
  if(collision)continue;
  if(!join(physical,parent,L"\\")||!join(physical,physical,candidate)){e=GetLastError();goto close;}
  if(GetFileAttributesW(physical)!=INVALID_FILE_ATTRIBUTES)continue;
  if(GetLastError()!=ERROR_FILE_NOT_FOUND&&GetLastError()!=ERROR_PATH_NOT_FOUND){e=GetLastError();goto close;}
  zero(&newrow,sizeof(newrow));newrow.magic=MAP_MAGIC;lstrcpyW(newrow.alias,candidate);lstrcpyW(newrow.name,name);newrow.checksum=hash(&newrow,FIELD_OFFSET(Record,checksum));
  end=SetFilePointer(h,0,0,FILE_END);if(end==INVALID_SET_FILE_POINTER){e=GetLastError();goto close;}if(!WriteFile(h,&newrow,sizeof(newrow),&used,0)||used!=sizeof(newrow)||!FlushFileBuffers(h)){e=GetLastError();if(!e)e=ERROR_WRITE_FAULT;SetFilePointer(h,end,0,FILE_BEGIN);SetEndOfFile(h);goto close;}
  lstrcpyW(out,candidate);found=TRUE;goto close;
 }
 e=ERROR_TOO_MANY_NAMES;
close:CloseHandle(h);
done:CloseHandle(guard);if(!found)SetLastError(e?e:ERROR_INVALID_DATA);return found;
}
/* Return 0 outside the namespace, 1 inside, -1 on invalid input. */
static int logical_path(LPCWSTR input,LPWSTR full,LPWSTR base){WCHAR prefix[MAX_PATH];DWORD n,k;input=fp_LegacyFilePath(input);if(!input){SetLastError(ERROR_INVALID_PARAMETER);return -1;}n=GetEnvironmentVariableW(L"FP32_USERDATA",base,MAX_PATH);if(!n)return 0;if(n>=MAX_PATH){SetLastError(ERROR_FILENAME_EXCED_RANGE);return -1;}k=GetFullPathNameW(input,MAX_PATH,full,0);if(!k||k>=MAX_PATH){if(k>=MAX_PATH)SetLastError(ERROR_FILENAME_EXCED_RANGE);return -1;}while(k>3&&full[k-1]=='\\')full[--k]=0;if(k<n)return 0;for(k=0;k<n;k++)prefix[k]=full[k];prefix[n]=0;if(lstrcmpiW(prefix,base)|| (full[n]&&full[n]!='\\'))return 0;return 1;}
static BOOL mapped(LPCWSTR input,LPWSTR out,BOOL create){WCHAR full[MAX_PATH],base[MAX_PATH],name[256],alias[13];DWORD i,k;int scope;input=fp_LegacyFilePath(input);scope=logical_path(input,full,base);if(scope<0)return FALSE;if(!scope){if(!input||lstrlenW(input)>=MAX_PATH){SetLastError(ERROR_FILENAME_EXCED_RANGE);return FALSE;}lstrcpyW(out,input);return TRUE;}lstrcpyW(out,base);i=lstrlenW(base);while(full[i]){if(full[i]=='\\')i++;k=0;while(full[i]&&full[i]!='\\'){if(k>=255){SetLastError(ERROR_FILENAME_EXCED_RANGE);return FALSE;}name[k++]=full[i++];}name[k]=0;if(!k)break;if(!map_name(out,name,create&&!full[i],FALSE,alias)){if(full[i]&&GetLastError()==ERROR_FILE_NOT_FOUND)SetLastError(ERROR_PATH_NOT_FOUND);return FALSE;}if(!join(out,out,L"\\")||!join(out,out,alias))return FALSE;}return TRUE;}
BOOL WINAPI fp_CreateDirectoryW(LPCWSTR p,LPSECURITY_ATTRIBUTES s){WCHAR path[MAX_PATH];return mapped(p,path,TRUE)&&CreateDirectoryW(path,s);}
BOOL WINAPI fp_RemoveDirectoryW(LPCWSTR p){WCHAR path[MAX_PATH],full[MAX_PATH],base[MAX_PATH],item[MAX_PATH];WIN32_FIND_DATAW data;HANDLE guard,h;DWORD error;int scope=logical_path(p,full,base);BOOL empty=TRUE;
 if(scope<0)return FALSE;if(!scope)return RemoveDirectoryW(p);if(!mapped(p,path,FALSE))return FALSE;
 guard=map_lock(path);if(guard==INVALID_HANDLE_VALUE)return FALSE;if(!join(item,path,L"\\*")){CloseHandle(guard);return FALSE;}
 h=FindFirstFileW(item,&data);if(h!=INVALID_HANDLE_VALUE){do{if(lstrcmpW(data.cFileName,L".")&&lstrcmpW(data.cFileName,L"..")&&lstrcmpiW(data.cFileName,L"FP32MAP.DAT")&&lstrcmpiW(data.cFileName,L"FP32LOCK.LCK")){empty=FALSE;break;}}while(FindNextFileW(h,&data));FindClose(h);}else if(GetLastError()!=ERROR_FILE_NOT_FOUND){error=GetLastError();CloseHandle(guard);SetLastError(error);return FALSE;}
 if(!empty){CloseHandle(guard);SetLastError(ERROR_DIR_NOT_EMPTY);return FALSE;}
 if(!join(item,path,MAP_FILE)){CloseHandle(guard);return FALSE;}if(!DeleteFileW(item)&&GetLastError()!=ERROR_FILE_NOT_FOUND){error=GetLastError();CloseHandle(guard);SetLastError(error);return FALSE;}
 CloseHandle(guard);if(!join(item,path,LOCK_FILE)||!DeleteFileW(item))return FALSE;return RemoveDirectoryW(path);
}
HANDLE WINAPI fp_CreateFileW(LPCWSTR p,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES sa,DWORD how,DWORD flags,HANDLE template_file){WCHAR path[MAX_PATH];if(!mapped(p,path,how==CREATE_ALWAYS||how==CREATE_NEW||how==OPEN_ALWAYS))return INVALID_HANDLE_VALUE;{HANDLE h=fp_BaseCreateFileW(path,access,share,sa,how,flags,template_file);DWORD e=GetLastError();static DWORD observed;if(h==INVALID_HANDLE_VALUE&&observed++<32){char logical[MAX_PATH];DWORD j;for(j=0;j<MAX_PATH-1&&p[j];j++)logical[j]=p[j]<128?(char)p[j]:'?';logical[j]=0;FpObserve(logical,e);FpObserve("Save CreateFile access",access);FpObserve("Save CreateFile share",share);FpObserve("Save CreateFile how",how);FpObserve("Save CreateFile flags",flags);FpObserve("Save CreateFile caller",(DWORD)__builtin_return_address(0));}SetLastError(e);return h;}}
DWORD WINAPI fp_GetFileAttributesW(LPCWSTR p){WCHAR path[MAX_PATH];return mapped(p,path,FALSE)?GetFileAttributesW(path):INVALID_FILE_ATTRIBUTES;}
BOOL WINAPI fp_DeleteFileW(LPCWSTR p){WCHAR path[MAX_PATH];return mapped(p,path,FALSE)&&DeleteFileW(path);}
BOOL WINAPI fp_CopyFileW(LPCWSTR a,LPCWSTR b,BOOL fail_exists){WCHAR src[MAX_PATH],dst[MAX_PATH];return mapped(a,src,FALSE)&&mapped(b,dst,TRUE)&&CopyFileW(src,dst,fail_exists);}
BOOL WINAPI fp_MoveFileExW(LPCWSTR a,LPCWSTR b,DWORD flags){WCHAR src[MAX_PATH],dst[MAX_PATH];if(!mapped(a,src,FALSE))return FALSE;if(b&&!mapped(b,dst,TRUE))return FALSE;return MoveFileExW(src,b?dst:0,flags);}
static BOOL ansi(LPCSTR in,LPWSTR out){if(!in){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}return MultiByteToWideChar(CP_ACP,0,in,-1,out,MAX_PATH)!=0;}
HANDLE WINAPI fp_CreateFileA(LPCSTR p,DWORD a,DWORD s,LPSECURITY_ATTRIBUTES sa,DWORD c,DWORD f,HANDLE t){WCHAR w[MAX_PATH];if(!ansi(p,w))return INVALID_HANDLE_VALUE;return fp_CreateFileW(w,a,s,sa,c,f,t);}
DWORD WINAPI fp_GetFileAttributesA(LPCSTR p){WCHAR w[MAX_PATH];return ansi(p,w)?fp_GetFileAttributesW(w):INVALID_FILE_ATTRIBUTES;}
BOOL WINAPI fp_DeleteFileA(LPCSTR p){WCHAR w[MAX_PATH];return ansi(p,w)&&fp_DeleteFileW(w);}
BOOL WINAPI fp_CreateDirectoryA(LPCSTR p,LPSECURITY_ATTRIBUTES sa){WCHAR w[MAX_PATH];return ansi(p,w)&&fp_CreateDirectoryW(w,sa);}
static WCHAR upper(WCHAR c){return (WCHAR)(ULONG_PTR)CharUpperW((LPWSTR)(ULONG_PTR)c);}
static BOOL matches(LPCWSTR pattern,LPCWSTR name){LPCWSTR star=0,retry=0;if(!lstrcmpW(pattern,L"*.*"))return TRUE;while(*name){if(*pattern=='*'){star=++pattern;retry=name;}else if(*pattern=='?'||upper(*pattern)==upper(*name)){pattern++;name++;}else if(star){pattern=star;name=++retry;}else return FALSE;}while(*pattern=='*')pattern++;return !*pattern;}
/* 1 = match, 0 = skip, -1 = metadata error. Never hide a corrupt map. */
static int translate_found(Search*s,LPWIN32_FIND_DATAW data){WCHAR name[256];if(!lstrcmpW(data->cFileName,L".")||!lstrcmpW(data->cFileName,L".."))return 0;if(!lstrcmpiW(data->cFileName,L"FP32MAP.DAT")||!lstrcmpiW(data->cFileName,L"FP32LOCK.LCK"))return 0;if(!map_name(s->parent,data->cFileName,FALSE,TRUE,name)){if(GetLastError()==ERROR_FILE_NOT_FOUND)SetLastError(ERROR_INVALID_DATA);return -1;}if(!matches(s->pattern,name))return 0;lstrcpyW(data->cAlternateFileName,data->cFileName);lstrcpyW(data->cFileName,name);return 1;}
static LPCWSTR search_input(LPCWSTR input,LPWSTR scratch){DWORD i,n;LPCWSTR parent;if(!input)return input;n=lstrlenW(input);if(n>=MAX_PATH)return input;i=n;while(i&&input[i]!='\\')i--;if(!i)return fp_LegacyFilePath(input);lstrcpyW(scratch,input);scratch[i]=0;parent=fp_LegacyFilePath(scratch);if(parent!=scratch){DWORD j,k=lstrlenW(parent);for(j=0;j<=k;j++)scratch[j]=parent[j];if(k+n-i>=MAX_PATH)return input;lstrcpyW(scratch+k,input+i);return scratch;}return fp_LegacyFilePath(input);}
HANDLE WINAPI fp_FindFirstFileW(LPCWSTR input,LPWIN32_FIND_DATAW data){WCHAR full[MAX_PATH],base[MAX_PATH],physical[MAX_PATH],search[MAX_PATH],scratch[MAX_PATH];DWORD i,n,error;Search*s;HANDLE h;int scope,status;input=search_input(input,scratch);scope=logical_path(input,full,base);if(scope<0)return INVALID_HANDLE_VALUE;if(!scope||!full[lstrlenW(base)])return FindFirstFileW(input,data);if(!data){SetLastError(ERROR_INVALID_PARAMETER);return INVALID_HANDLE_VALUE;}n=lstrlenW(full);i=n;while(i&&full[i]!='\\')i--;if(!i||n-i>255){SetLastError(ERROR_INVALID_NAME);return INVALID_HANDLE_VALUE;}s=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*s));if(!s){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return INVALID_HANDLE_VALUE;}lstrcpyW(s->pattern,full+i+1);full[i]=0;if(!mapped(full,physical,FALSE)||!join(search,physical,L"\\*")){error=GetLastError();HeapFree(GetProcessHeap(),0,s);SetLastError(error);return INVALID_HANDLE_VALUE;}lstrcpyW(s->parent,physical);h=FindFirstFileW(search,data);if(h==INVALID_HANDLE_VALUE){error=GetLastError();HeapFree(GetProcessHeap(),0,s);SetLastError(error);return h;}s->handle=h;for(;;){status=translate_found(s,data);if(status>0){lock_search();s->next=searches;searches=s;unlock_search();return h;}if(status<0){error=GetLastError();break;}if(!FindNextFileW(h,data)){error=GetLastError();if(error==ERROR_NO_MORE_FILES)error=ERROR_FILE_NOT_FOUND;break;}}FindClose(h);HeapFree(GetProcessHeap(),0,s);SetLastError(error);return INVALID_HANDLE_VALUE;}
BOOL WINAPI fp_FindNextFileW(HANDLE h,LPWIN32_FIND_DATAW data){Search*s;BOOL result;int status;DWORD error;lock_search();for(s=searches;s&&s->handle!=h;s=s->next){}if(!s){unlock_search();return FindNextFileW(h,data);}if(!data){unlock_search();SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}for(;;){result=FindNextFileW(h,data);if(!result)break;status=translate_found(s,data);if(status>0)break;if(status<0){result=FALSE;break;}}error=GetLastError();unlock_search();SetLastError(error);return result;}
BOOL WINAPI fp_FindClose(HANDLE h){Search**p,*s=0;BOOL result;lock_search();for(p=&searches;*p;p=&(*p)->next)if((*p)->handle==h){s=*p;*p=s->next;break;}result=FindClose(h);unlock_search();if(s)HeapFree(GetProcessHeap(),0,s);return result;}
BOOL WINAPI fp_GetFileAttributesExW(LPCWSTR p,GET_FILEEX_INFO_LEVELS level,LPVOID out){WCHAR path[MAX_PATH];if(!mapped(p,path,FALSE))return FALSE;return fp_BaseGetFileAttributesExW(path,level,out);}

/* Resolve component spelling by real directory queries on systems predating
   GetLongPathNameW. Preserve relative/drive prefixes and the buffer contract. */
DWORD WINAPI fp_GetLongPathNameW(LPCWSTR input,LPWSTR output,DWORD capacity){
 typedef DWORD(WINAPI *Native)(LPCWSTR,LPWSTR,DWORD);
 Native native=(Native)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),"GetLongPathNameW");
 WCHAR result[MAX_PATH],query[MAX_PATH],full[MAX_PATH],base[MAX_PATH];DWORD i=0,n=0,start,k,j,error;WIN32_FIND_DATAW data;HANDLE h;int scope;
 if(!input||!input[0]||(!output&&capacity)){SetLastError(ERROR_INVALID_PARAMETER);return 0;}
 scope=logical_path(input,full,base);if(scope<0)return 0;if(!scope&&native)return native(input,output,capacity);
 input=fp_LegacyFilePath(input);if(lstrlenW(input)>=MAX_PATH){SetLastError(ERROR_FILENAME_EXCED_RANGE);return 0;}
 if(input[0]=='\\'&&input[1]=='\\'){i=2;while(input[i]&&input[i]!='\\')i++;if(input[i])i++;while(input[i]&&input[i]!='\\')i++;}
 else if(input[1]==':')i=2;
 for(j=0;j<i;j++)result[n++]=input[j];
 while(input[i]){
  if(input[i]=='\\'||input[i]=='/'){if(n+1>=MAX_PATH){SetLastError(ERROR_FILENAME_EXCED_RANGE);return 0;}result[n++]=input[i++];continue;}
  start=i;while(input[i]&&input[i]!='\\'&&input[i]!='/')i++;k=i-start;
  if(n+k>=MAX_PATH){SetLastError(ERROR_FILENAME_EXCED_RANGE);return 0;}
  if((k==1&&input[start]=='.')||(k==2&&input[start]=='.'&&input[start+1]=='.')){for(j=0;j<k;j++)result[n++]=input[start+j];continue;}
  result[n]=0;lstrcpyW(query,result);for(j=0;j<k;j++)query[n+j]=input[start+j];query[n+k]=0;
  h=fp_FindFirstFileW(query,&data);if(h==INVALID_HANDLE_VALUE)return 0;error=GetLastError();fp_FindClose(h);SetLastError(error);k=lstrlenW(data.cFileName);if(n+k>=MAX_PATH){SetLastError(ERROR_FILENAME_EXCED_RANGE);return 0;}lstrcpyW(result+n,data.cFileName);n+=k;
 }
 result[n]=0;if(!n){SetLastError(ERROR_BAD_PATHNAME);return 0;}
 if(n>=capacity)return n+1;lstrcpyW(output,result);return n;
}
