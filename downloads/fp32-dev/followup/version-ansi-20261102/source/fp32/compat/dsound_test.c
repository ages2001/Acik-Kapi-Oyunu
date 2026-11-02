#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
static HANDLE logh;static DWORD checks,failures;
static void value(const char*s,DWORD n){char b[180];DWORD w;wsprintfA(b,"%s %lu (0x%08lx)\r\n",s,n,n);WriteFile(logh,b,lstrlenA(b),&w,0);FlushFileBuffers(logh);}
static void check(BOOL ok,const char*s){checks++;if(!ok)failures++;value(s,ok);}
static void zero(void*p,UINT n){BYTE*b=p;while(n--)*b++=0;}
void WINAPI entry(void){HMODULE dll;HRESULT(WINAPI*create)(LPCGUID,LPDIRECTSOUND*,LPUNKNOWN);LPDIRECTSOUND sound=0;LPDIRECTSOUNDBUFFER buffer=0;DSCAPS caps;DSBUFFERDESC desc;WAVEFORMATEX format;HRESULT hr;void *first=0,*second=0;DWORD n1=0,n2=0,i,start,play=0,write=0,status=0;WAVEOUTCAPSA wavecaps;HWND window=0;MSG message;
 logh=CreateFileA("C:\\FP32\\DSOUND.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 value("Native wave output devices",waveOutGetNumDevs());zero(&wavecaps,sizeof(wavecaps));value("Native wave caps result",waveOutGetDevCapsA(0,&wavecaps,sizeof(wavecaps)));value("Native wave supported formats",wavecaps.dwFormats);value("Native wave support flags",wavecaps.dwSupport);
 dll=LoadLibraryA("C:\\FP32\\FPDSND.DLL");check(dll!=0,"Original adapted DirectSound loads");if(!dll){value("Load error",GetLastError());goto done;}
 create=(void*)GetProcAddress(dll,"DirectSoundCreate");check(create!=0,"Original DirectSoundCreate resolves");if(!create)goto done;
 hr=create(0,&sound,0);value("DirectSoundCreate HRESULT",hr);check(SUCCEEDED(hr)&&sound,"Original sound object created");if(FAILED(hr)||!sound)goto done;
 value("CreateSoundBuffer RVA",(DWORD)sound->lpVtbl->CreateSoundBuffer-(DWORD)dll);
 zero(&caps,sizeof(caps));caps.dwSize=sizeof(caps);hr=IDirectSound_GetCaps(sound,&caps);check(SUCCEEDED(hr),"Device capabilities queried");value("DirectSound caps flags",caps.dwFlags);
 window=CreateWindowExA(0,"STATIC","DirectSound PCM native probe",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,100,400,180,0,0,GetModuleHandleA(0),0);
 ShowWindow(window,SW_SHOW);SetForegroundWindow(window);UpdateWindow(window);while(PeekMessageA(&message,0,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageA(&message);}
 hr=IDirectSound_SetCooperativeLevel(sound,window,DSSCL_NORMAL);value("Cooperative level HRESULT",hr);check(SUCCEEDED(hr),"Normal cooperative level accepted");
 zero(&format,sizeof(format));format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=22050;format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=88200;
 zero(&desc,sizeof(desc));desc.dwSize=20;desc.dwFlags=DSBCAPS_GLOBALFOCUS|DSBCAPS_CTRLVOLUME|DSBCAPS_CTRLPAN|DSBCAPS_CTRLFREQUENCY;desc.dwBufferBytes=88200;desc.lpwfxFormat=&format;
 {DWORD sizes[]={20,36,24},flags[]={0,DSBCAPS_CTRLVOLUME|DSBCAPS_CTRLPAN|DSBCAPS_CTRLFREQUENCY,DSBCAPS_GLOBALFOCUS|DSBCAPS_CTRLVOLUME|DSBCAPS_CTRLPAN|DSBCAPS_CTRLFREQUENCY};UINT a,b;LPDIRECTSOUNDBUFFER trial;
 for(a=0;a<3;a++)for(b=0;b<3;b++){desc.dwSize=sizes[a];desc.dwFlags=flags[b];trial=0;hr=IDirectSound_CreateSoundBuffer(sound,&desc,&trial,0);value("Trial descriptor size",desc.dwSize);value("Trial flags",desc.dwFlags);value("Trial HRESULT",hr);if(trial)IDirectSoundBuffer_Release(trial);}
 desc.dwSize=20;desc.dwFlags=DSBCAPS_CTRLVOLUME|DSBCAPS_CTRLPAN|DSBCAPS_CTRLFREQUENCY;
 }
 hr=IDirectSound_CreateSoundBuffer(sound,&desc,&buffer,0);value("Buffer HRESULT",hr);check(SUCCEEDED(hr)&&buffer,"One-second PCM buffer created");if(FAILED(hr)||!buffer)goto done;
 hr=IDirectSoundBuffer_Lock(buffer,0,88200,&first,&n1,&second,&n2,0);check(SUCCEEDED(hr)&&n1+n2==88200,"Full PCM buffer locked");if(FAILED(hr))goto done;
 /* Distinct 441/630 Hz square waves in left/right channels; no floating point. */
 for(i=0;i<(n1+n2)/4;i++){SHORT left=(i%50)<25?6000:-6000,right=(i%35)<17?3000:-3000;BYTE*base=i*4<n1?(BYTE*)first+i*4:(BYTE*)second+i*4-n1;((SHORT*)base)[0]=left;((SHORT*)base)[1]=right;}
 check(SUCCEEDED(IDirectSoundBuffer_Unlock(buffer,first,n1,second,n2)),"PCM buffer unlocked");
 check(SUCCEEDED(IDirectSoundBuffer_SetVolume(buffer,-600)),"Volume control accepted");check(SUCCEEDED(IDirectSoundBuffer_SetPan(buffer,0)),"Pan control accepted");
 check(SUCCEEDED(IDirectSoundBuffer_SetFrequency(buffer,22050)),"Playback frequency accepted");
 check(SUCCEEDED(IDirectSoundBuffer_Play(buffer,0,0,DSBPLAY_LOOPING)),"Looping playback starts");
 Sleep(350);check(SUCCEEDED(IDirectSoundBuffer_GetStatus(buffer,&status))&&(status&DSBSTATUS_PLAYING),"Buffer reports playing");
 hr=IDirectSoundBuffer_GetCurrentPosition(buffer,&play,&write);value("Play cursor",play);value("Write cursor",write);check(SUCCEEDED(hr)&&play>0&&play<88200,"Playback cursor advances");
 start=GetTickCount();while(GetTickCount()-start<2200){while(PeekMessageA(&message,0,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageA(&message);}Sleep(20);}
 check(SUCCEEDED(IDirectSoundBuffer_Stop(buffer)),"Playback stops");check(SUCCEEDED(IDirectSoundBuffer_GetStatus(buffer,&status))&&!(status&DSBSTATUS_PLAYING),"Stopped status reported");
 done:if(window)DestroyWindow(window);if(buffer)IDirectSoundBuffer_Release(buffer);if(sound)IDirectSound_Release(sound);if(dll)FreeLibrary(dll);value("Checks",checks);value("Failures",failures);CloseHandle(logh);ExitProcess(failures?2:0);}
