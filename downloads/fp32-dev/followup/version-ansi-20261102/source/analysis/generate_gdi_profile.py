"""Generate the optional GDI timing build; run from the project root."""
from pathlib import Path
p=Path('compat/gdi.c');s=p.read_text();names=['fp_BitBlt','fp_StretchBlt','fp_GdiFlush','fp_ExtTextOutW','fp_ExtTextOutA','fp_StretchDIBits','transfer','cpu_copy']
import re
wrappers=[]
for idx,name in enumerate(names):
 match=re.search(r'(static int|BOOL WINAPI|int WINAPI) '+name+r'\(([^)]*)\)',s)
 assert match,name
 typ,args=match.groups();callargs=[]
 for a in args.split(','):
  if a.strip()!='void':callargs.append(re.search(r'(\w+)\s*$',a)[1])
 wrappers.append(f'#undef {name}\n{typ} {name}({args})'+'{DWORD start=GetTickCount(),error;int result=profile_inner_'+name+'('+','.join(callargs)+');error=GetLastError();profile_counts['+str(idx)+']++;profile_ticks['+str(idx)+']+=GetTickCount()-start;profile_emit();SetLastError(error);return result;}')
 # inner helper calls should resolve through profiled wrapper for nested timings
 if name in ['transfer','cpu_copy']:
  s=s[:match.start()]+f'{typ} {name}({args});\n'+s[match.start():]
  s=s.replace(f'{typ} {name}({args})'+'{',f'{typ} profile_inner_{name}({args})'+'{',1)
 else:s=s.replace(f'{typ} {name}({args})'+'{',f'{typ} profile_inner_{name}({args})'+'{',1)
header='''\n/* Cumulative wall-time probes. Categories are inclusive and may overlap. */
static DWORD profile_counts[8],profile_ticks[8],profile_last;
static void profile_number(HANDLE h,DWORD n){char b[16],v[16];int i=0,j=0;DWORD w;do{b[i++]='0'+n%10;n/=10;}while(n);while(i)v[j++]=b[--i];WriteFile(h,v,j,&w,0);}
static void profile_emit(void){DWORD now=GetTickCount(),i,w;HANDLE h;const char*names[8]={"BitBlt","StretchBlt","GdiFlush","ExtTextOutW","ExtTextOutA","StretchDIBits","transfer","cpu_copy"};if(now-profile_last<5000)return;profile_last=now;h=CreateFileA("C:\\\\FP10\\\\PROFILE.LOG",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,0,CREATE_ALWAYS,0,0);if(h==INVALID_HANDLE_VALUE)return;WriteFile(h,"tick=",5,&w,0);profile_number(h,now);WriteFile(h,"\\r\\n",2,&w,0);for(i=0;i<8;i++){DWORD len=0;while(names[i][len])len++;WriteFile(h,names[i],len,&w,0);WriteFile(h," count=",7,&w,0);profile_number(h,profile_counts[i]);WriteFile(h," ms=",4,&w,0);profile_number(h,profile_ticks[i]);WriteFile(h,"\\r\\n",2,&w,0);}CloseHandle(h);}
'''
# Keep instrumentation out of ordinary builds with a separate generated TU.
s=s.replace('#include <windows.h>','#include <windows.h>'+header)
Path('compat/gdi-profile.c').write_text(s+'\n'+'\n'.join(wrappers)+'\n')
