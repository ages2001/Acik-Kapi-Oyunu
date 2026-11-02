#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* The projector rewrites five Adobe shell commands at startup, even when setup
   owns the .swf association. Keep those writes in a volatile private namespace
   so existing Adobe installations and the user's previous association survive.
   Actual registry calls execute and their errors are preserved. No general
   registry access or application preference writes are intercepted. */
static const char*commands[]={
 "ShockwaveFlash.ShockwaveFlash\\shell\\open\\command",
 "FlashPlayer.FlashVideo\\shell\\open\\command",
 "FlashPlayer.VideoForFlashPlayer\\shell\\open\\command",
 "FlashPlayer.AudioForFlashPlayer\\shell\\open\\command",
 "FlashPlayer.ProtectedMediaForFlashPlayer\\shell\\open\\command"};
static const char prefix[]="FP10PortPrivateClasses\\";
static unsigned fold(unsigned x){return x>='A'&&x<='Z'?x+32:x;}
static int known_a(LPCSTR s){unsigned i,j;if(!s)return 0;for(i=0;i<5;i++){for(j=0;s[j]&&commands[i][j]&&fold((BYTE)s[j])==fold((BYTE)commands[i][j]);j++){}if(!s[j]&&!commands[i][j])return 1;}return 0;}
static int known_w(LPCWSTR s){unsigned i,j;if(!s)return 0;for(i=0;i<5;i++){for(j=0;s[j]&&commands[i][j]&&fold(s[j])==fold((BYTE)commands[i][j]);j++){}if(!s[j]&&!commands[i][j])return 1;}return 0;}
LSTATUS WINAPI fp_RegCreateKeyExA(HKEY key,LPCSTR name,DWORD reserved,LPSTR cls,DWORD options,REGSAM access,LPSECURITY_ATTRIBUTES security,PHKEY result,LPDWORD disposition){char path[256];unsigned i=0,j=0;if(key==HKEY_CLASSES_ROOT&&known_a(name)){while(prefix[i]){path[i]=prefix[i];i++;}while((path[i++]=name[j++])){}key=HKEY_CURRENT_USER;name=path;options|=REG_OPTION_VOLATILE;}return RegCreateKeyExA(key,name,reserved,cls,options,access,security,result,disposition);}
LSTATUS WINAPI fp_RegCreateKeyExW(HKEY key,LPCWSTR name,DWORD reserved,LPWSTR cls,DWORD options,REGSAM access,LPSECURITY_ATTRIBUTES security,PHKEY result,LPDWORD disposition){WCHAR path[256];unsigned i=0,j=0;if(key==HKEY_CLASSES_ROOT&&known_w(name)){while(prefix[i]){path[i]=prefix[i];i++;}while((path[i++]=name[j++])){}key=HKEY_CURRENT_USER;name=path;options|=REG_OPTION_VOLATILE;}return RegCreateKeyExW(key,name,reserved,cls,options,access,security,result,disposition);}
