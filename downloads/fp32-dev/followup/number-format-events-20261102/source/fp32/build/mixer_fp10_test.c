#define MIXER_TEST_DLL "C:\\FP32\\M10M.DLL"
#define MIXER_TEST_LOG "C:\\FP32\\MIXER10.LOG"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#ifndef MIXER_TEST_DLL
#define MIXER_TEST_DLL "C:\\FP32\\M32M.DLL"
#endif
#ifndef MIXER_TEST_LOG
#define MIXER_TEST_LOG "C:\\FP32\\MIXER.LOG"
#endif
static HANDLE logh;static DWORD checks,failures;
static void value(const char*s,DWORD n){char b[180];DWORD w;wsprintfA(b,"%s %lu\r\n",s,n);WriteFile(logh,b,lstrlenA(b),&w,0);FlushFileBuffers(logh);}
static void check(BOOL ok,const char*s){checks++;if(!ok)failures++;value(s,ok);}
static void zero(void*p,UINT n){BYTE*b=p;while(n--)*b++=0;}
static UINT(WINAPI*stress_num)(void);
static MMRESULT(WINAPI*stress_caps)(UINT_PTR,LPMIXERCAPSA,UINT);
static MMRESULT(WINAPI*stress_open)(LPHMIXER,UINT,DWORD_PTR,DWORD_PTR,DWORD);
static MMRESULT(WINAPI*stress_close)(HMIXER);
static HANDLE stress_start;static volatile LONG stress_errors;
static DWORD WINAPI stress_worker(void*unused){UINT i;MIXERCAPSA caps;HMIXER h;
 WaitForSingleObject(stress_start,INFINITE);
 for(i=0;i<32;i++){h=0;if(stress_num()<1||stress_caps(0,&caps,sizeof(caps))||!caps.cDestinations)InterlockedIncrement(&stress_errors);
  if(stress_open(&h,0,0,0,CALLBACK_NULL)||!h)InterlockedIncrement(&stress_errors);else if(stress_close(h))InterlockedIncrement(&stress_errors);}
 return 0;
}
#define LOAD(ret,name,args) ret(WINAPI*name)args=(void*)GetProcAddress(dll,#name);check(name!=0,#name " resolves");if(!name)goto done;
void WINAPI entry(void){HMODULE dll;HMIXER mixer=0;HWND window=0;MIXERCAPSA caps;MIXERLINEA line;MIXERLINECONTROLSA controls;static MIXERCONTROLA all[64];MIXERCONTROLA volume,one;MIXERCONTROLDETAILS details;MIXERCONTROLDETAILS_UNSIGNED before[16],changed[16],after[16];UINT id=999;DWORD result,channels,i,tick,notifications=0;BOOL same;MSG msg;
 logh=CreateFileA(MIXER_TEST_LOG,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 dll=LoadLibraryA(MIXER_TEST_DLL);check(dll!=0,"Production mixer proxy loads");if(!dll){value("Load error",GetLastError());goto done;}
 {LOAD(UINT,mixerGetNumDevs,(void));LOAD(MMRESULT,mixerGetDevCapsA,(UINT_PTR,LPMIXERCAPSA,UINT));LOAD(MMRESULT,mixerGetID,(HMIXEROBJ,UINT*,DWORD));LOAD(MMRESULT,mixerOpen,(LPHMIXER,UINT,DWORD_PTR,DWORD_PTR,DWORD));LOAD(MMRESULT,mixerClose,(HMIXER));LOAD(MMRESULT,mixerGetLineInfoA,(HMIXEROBJ,LPMIXERLINEA,DWORD));LOAD(MMRESULT,mixerGetLineControlsA,(HMIXEROBJ,LPMIXERLINECONTROLSA,DWORD));LOAD(MMRESULT,mixerGetControlDetailsA,(HMIXEROBJ,LPMIXERCONTROLDETAILS,DWORD));LOAD(MMRESULT,mixerSetControlDetails,(HMIXEROBJ,LPMIXERCONTROLDETAILS,DWORD));
 {HANDLE workers[4]={0,0,0,0};DWORD ids[4];UINT n;BOOL created=TRUE,finished=TRUE;
 stress_num=mixerGetNumDevs;stress_caps=mixerGetDevCapsA;stress_open=mixerOpen;stress_close=mixerClose;
 stress_start=CreateEventA(0,TRUE,FALSE,0);check(stress_start!=0,"Concurrent first-use start event created");
 if(stress_start){for(n=0;n<4;n++){workers[n]=CreateThread(0,0,stress_worker,0,0,&ids[n]);if(!workers[n])created=FALSE;}
 check(created,"Four concurrent first-use workers created");SetEvent(stress_start);
 for(n=0;n<4;n++)if(workers[n]){if(WaitForSingleObject(workers[n],20000)!=WAIT_OBJECT_0)finished=FALSE;CloseHandle(workers[n]);}
 check(finished&&!stress_errors,"128 concurrent device queries and mixer lifetimes complete");CloseHandle(stress_start);}
 }
 value("Mixer devices",mixerGetNumDevs());check(mixerGetNumDevs()>0,"Real mixer device enumerated");if(!mixerGetNumDevs())goto done;
 zero(&caps,sizeof(caps));result=mixerGetDevCapsA(0,&caps,sizeof(caps));value("Caps result",result);check(result==MMSYSERR_NOERROR&&caps.cDestinations>0&&caps.szPname[0],"ANSI device capabilities and name returned");
 check(mixerGetID(0,&id,MIXER_OBJECTF_MIXER)==MMSYSERR_NOERROR&&id==0,"Numeric mixer identity resolved");
 window=CreateWindowExA(0,"STATIC","Mixer callback probe",WS_OVERLAPPEDWINDOW,100,100,400,160,0,0,GetModuleHandleA(0),0);
 result=mixerOpen(&mixer,0,(DWORD_PTR)window,0,CALLBACK_WINDOW);value("Open result",result);check(result==MMSYSERR_NOERROR&&mixer,"Mixer opens with window callback");if(!mixer)goto done;
 check(mixerGetID((HMIXEROBJ)mixer,&id,MIXER_OBJECTF_HMIXER)==MMSYSERR_NOERROR&&id==0,"Open handle identity resolved");
 zero(&line,sizeof(line));line.cbStruct=sizeof(line);line.dwDestination=0;result=mixerGetLineInfoA((HMIXEROBJ)mixer,&line,MIXER_OBJECTF_HMIXER|MIXER_GETLINEINFOF_DESTINATION);value("Line result",result);check(result==MMSYSERR_NOERROR&&line.cChannels&&line.cControls&&line.szName[0],"Destination line topology returned");if(result||line.cControls>64)goto close;
 value("Destination controls",line.cControls);value("Destination channels",line.cChannels);
 zero(&controls,sizeof(controls));controls.cbStruct=sizeof(controls);controls.dwLineID=line.dwLineID;controls.cControls=line.cControls;controls.cbmxctrl=sizeof(MIXERCONTROLA);controls.pamxctrl=all;result=mixerGetLineControlsA((HMIXEROBJ)mixer,&controls,MIXER_OBJECTF_HMIXER|MIXER_GETLINECONTROLSF_ALL);check(result==0,"All destination controls returned");if(result)goto close;
 zero(&volume,sizeof(volume));controls.dwControlType=MIXERCONTROL_CONTROLTYPE_VOLUME;controls.cControls=1;controls.pamxctrl=&volume;result=mixerGetLineControlsA((HMIXEROBJ)mixer,&controls,MIXER_OBJECTF_HMIXER|MIXER_GETLINECONTROLSF_ONEBYTYPE);check(result==0&&volume.dwControlType==MIXERCONTROL_CONTROLTYPE_VOLUME,"Volume control found by type");if(result)goto close;
 same=FALSE;for(i=0;i<line.cControls;i++)if(all[i].dwControlID==volume.dwControlID)same=TRUE;check(same,"Volume identity matches enumerated controls");
 controls.dwControlID=volume.dwControlID;controls.pamxctrl=&one;check(mixerGetLineControlsA((HMIXEROBJ)mixer,&controls,MIXER_OBJECTF_HMIXER|MIXER_GETLINECONTROLSF_ONEBYID)==0&&one.dwControlID==volume.dwControlID,"Control lookup by ID matches");
 channels=volume.fdwControl&MIXERCONTROL_CONTROLF_UNIFORM?1:line.cChannels;if(channels>16)goto close;
 zero(&details,sizeof(details));details.cbStruct=sizeof(details);details.dwControlID=volume.dwControlID;details.cChannels=channels;details.cbDetails=sizeof(before[0]);details.paDetails=before;
 result=mixerGetControlDetailsA((HMIXEROBJ)mixer,&details,MIXER_OBJECTF_HMIXER|MIXER_GETCONTROLDETAILSF_VALUE);check(result==0,"Current volume values read");if(result)goto close;
 for(i=0;i<channels;i++)changed[i].dwValue=volume.Bounds.dwMinimum+(volume.Bounds.dwMaximum-volume.Bounds.dwMinimum)/2;
 details.paDetails=changed;result=mixerSetControlDetails((HMIXEROBJ)mixer,&details,MIXER_OBJECTF_HMIXER|MIXER_SETCONTROLDETAILSF_VALUE);check(result==0,"Real mixer volume updated");
 details.paDetails=after;result=mixerGetControlDetailsA((HMIXEROBJ)mixer,&details,MIXER_OBJECTF_HMIXER|MIXER_GETCONTROLDETAILSF_VALUE);same=result==0;for(i=0;i<channels;i++)if(after[i].dwValue!=changed[i].dwValue)same=FALSE;check(same,"Changed mixer values read back");
 tick=GetTickCount();while(GetTickCount()-tick<600){while(PeekMessageA(&msg,0,0,0,PM_REMOVE)){if(msg.message==MM_MIXM_CONTROL_CHANGE&&msg.wParam==(WPARAM)mixer&&msg.lParam==volume.dwControlID)notifications++;TranslateMessage(&msg);DispatchMessageA(&msg);}Sleep(10);}
 check(notifications>0,"Native driver control-change callback delivered");
 details.paDetails=before;check(mixerSetControlDetails((HMIXEROBJ)mixer,&details,MIXER_OBJECTF_HMIXER|MIXER_SETCONTROLDETAILSF_VALUE)==0,"Original volume restored");
 details.paDetails=after;result=mixerGetControlDetailsA((HMIXEROBJ)mixer,&details,MIXER_OBJECTF_HMIXER|MIXER_GETCONTROLDETAILSF_VALUE);same=result==0;for(i=0;i<channels;i++)if(after[i].dwValue!=before[i].dwValue)same=FALSE;check(same,"Restored volume verified");
 /* The player still opens wave devices through the native WINMM. */
 {HMODULE native=LoadLibraryA("WINMM.DLL");MMRESULT(WINAPI*open_wave)(LPHWAVEOUT,UINT,LPCWAVEFORMATEX,DWORD_PTR,DWORD_PTR,DWORD)=(void*)GetProcAddress(native,"waveOutOpen");MMRESULT(WINAPI*close_wave)(HWAVEOUT)=(void*)GetProcAddress(native,"waveOutClose");WAVEFORMATEX format;HWAVEOUT wave=0;HMIXER other=0;
  zero(&format,sizeof(format));format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=22050;format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=88200;
  result=open_wave(&wave,0,&format,0,0,CALLBACK_NULL);check(result==0&&wave,"Native wave handle opens outside mixer provider");
  if(!result&&wave){
   check(mixerGetID((HMIXEROBJ)wave,&id,MIXER_OBJECTF_HWAVEOUT)==0&&id==0,"Native wave handle maps to mixer without crossing provider heaps");
   {MIXERLINEA wave_line;MIXERLINECONTROLSA wave_controls;MIXERCONTROLA wave_volume;
    zero(&wave_line,sizeof(wave_line));wave_line.cbStruct=sizeof(wave_line);wave_line.dwDestination=0;
    result=mixerGetLineInfoA((HMIXEROBJ)wave,&wave_line,MIXER_OBJECTF_HWAVEOUT|MIXER_GETLINEINFOF_DESTINATION);
    check(result==0&&wave_line.dwLineID==line.dwLineID,"Line query accepts native wave handle");
    zero(&wave_controls,sizeof(wave_controls));wave_controls.cbStruct=sizeof(wave_controls);wave_controls.dwLineID=wave_line.dwLineID;wave_controls.dwControlType=MIXERCONTROL_CONTROLTYPE_VOLUME;wave_controls.cControls=1;wave_controls.cbmxctrl=sizeof(wave_volume);wave_controls.pamxctrl=&wave_volume;
    result=mixerGetLineControlsA((HMIXEROBJ)wave,&wave_controls,MIXER_OBJECTF_HWAVEOUT|MIXER_GETLINECONTROLSF_ONEBYTYPE);
    check(result==0&&wave_volume.dwControlID==volume.dwControlID,"Control query accepts native wave handle");
    details.paDetails=after;result=mixerGetControlDetailsA((HMIXEROBJ)wave,&details,MIXER_OBJECTF_HWAVEOUT|MIXER_GETCONTROLDETAILSF_VALUE);same=result==0;for(i=0;i<channels;i++)if(after[i].dwValue!=before[i].dwValue)same=FALSE;
    check(same,"Value query accepts native wave handle");details.paDetails=before;
    check(mixerSetControlDetails((HMIXEROBJ)wave,&details,MIXER_OBJECTF_HWAVEOUT|MIXER_SETCONTROLDETAILSF_VALUE)==0,"Value assignment accepts native wave handle");
   }
   result=mixerOpen(&other,(UINT)(UINT_PTR)wave,0,0,MIXER_OBJECTF_HWAVEOUT|CALLBACK_NULL);check(result==0&&other,"Mixer opens from native wave handle");if(other)check(mixerClose(other)==0,"Wave-selected mixer closes");
   check(close_wave(wave)==0,"Native wave handle closes");
  }
  FreeLibrary(native);
 }
 close:if(mixer){HMIXER old=mixer;check(mixerClose(mixer)==0,"Mixer handle closes");mixer=0;check(mixerGetID((HMIXEROBJ)old,&id,MIXER_OBJECTF_HMIXER)==MMSYSERR_INVALHANDLE,"Closed handle rejected");}
 }
 done:if(window)DestroyWindow(window);if(dll)FreeLibrary(dll);value("Checks",checks);value("Failures",failures);CloseHandle(logh);ExitProcess(failures?2:0);}
