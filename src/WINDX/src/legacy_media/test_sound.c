/* Exercise the public DirectSound playback API and an actual WinMM endpoint. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include "../test_output.h"

typedef HRESULT (WINAPI *create_fn)(const GUID *,IDirectSound **,IUnknown *);
typedef HRESULT (WINAPI *enum_fn)(LPDSENUMCALLBACKA,void *);

struct endpoints {
    UINT count;
    GUID first;
};

static BOOL CALLBACK endpoint(LPGUID guid, LPCSTR name, LPCSTR driver, LPVOID parameter)
{
    struct endpoints *found = parameter;
    if (guid && !found->count++) found->first = *guid;
    report(guid ? "WinMM playback endpoint " : "Default playback endpoint ");
    report(name ? name : ""); report("\r\n");
    return TRUE;
}

static void status(const char *operation, HRESULT hr)
{
    report(operation); report(" "); report_hex((DWORD)hr); report("\r\n");
}

static HRESULT tone(IDirectSound *sound, unsigned period, IDirectSoundBuffer **out)
{
    WAVEFORMATEX format = {WAVE_FORMAT_PCM,1,11025,11025,1,8,0};
    DSBUFFERDESC desc = {0};
    IDirectSoundBuffer *buffer;
    BYTE *first, *second;
    DWORD n1,n2;
    const DWORD bytes = 16000;
    *out = NULL;
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLVOLUME | DSBCAPS_STATIC;
    desc.dwBufferBytes = bytes;
    desc.lpwfxFormat = &format;
    HRESULT hr = IDirectSound_CreateSoundBuffer(sound,&desc,&buffer,NULL);
    if (FAILED(hr)) return hr;
    hr = IDirectSoundBuffer_Lock(buffer,bytes-32,64,(void **)&first,&n1,(void **)&second,&n2,0);
    if (SUCCEEDED(hr)) {
        BOOL wrapped = first && second && n1==32 && n2==32;
        hr = IDirectSoundBuffer_Unlock(buffer,first,n1,second,n2);
        if (SUCCEEDED(hr) && !wrapped) hr = E_FAIL;
    }
    if (SUCCEEDED(hr)) hr = IDirectSoundBuffer_Lock(buffer,0,0,(void **)&first,&n1,(void **)&second,&n2,DSBLOCK_ENTIREBUFFER);
    if (SUCCEEDED(hr)) {
        for (DWORD i=0;i<n1;++i) first[i]=(BYTE)(i % period < period/2 ? 164 : 92);
        for (DWORD i=0;i<n2;++i) second[i]=(BYTE)((i+n1) % period < period/2 ? 164 : 92);
        hr = IDirectSoundBuffer_Unlock(buffer,first,n1,second,n2);
        if (SUCCEEDED(hr) && n1+n2 != bytes) hr = E_FAIL;
    }
    if (FAILED(hr)) IDirectSoundBuffer_Release(buffer); else *out = buffer;
    return hr;
}

void WINAPI sound_entry(void)
{
    char path[MAX_PATH],*base;
    struct endpoints devices={0};
    IDirectSound *sound=NULL;
    IDirectSoundBuffer *first=NULL,*second=NULL;
    DSCAPS caps={0};
    DWORD length, initial=0, cursor=0, state=0;
    initialize_test();
    length=GetModuleFileNameA(NULL,path,MAX_PATH);
    if (!length || length>=MAX_PATH) ExitProcess(1);
    base=path+length;
    while (base>path && base[-1]!='\\' && base[-1]!='/') --base;
    if ((base-path)+sizeof("dsound.dll")>MAX_PATH) ExitProcess(1);
    lstrcpyA(base,"dsound.dll");
    HMODULE dll=LoadLibraryA(path);
    if (!dll) { status("LoadLibrary",HRESULT_FROM_WIN32(GetLastError())); ExitProcess(1); }
    report("DirectSound module ");report(path);report("\r\n");
    create_fn create=(create_fn)GetProcAddress(dll,"DirectSoundCreate");
    enum_fn enumerate=(enum_fn)GetProcAddress(dll,"DirectSoundEnumerateA");
    HRESULT hr=enumerate && create ? enumerate(endpoint,&devices) : E_NOINTERFACE;
    if (FAILED(hr)) {status("Enumeration",hr);ExitProcess(1);}
    if (!waveOutGetNumDevs()) {
        hr=create(NULL,&sound,NULL);
        status("No WinMM output, DirectSoundCreate returned",hr);
        if (sound) IDirectSound_Release(sound);
        if (hr==DSERR_NODRIVER && !devices.count) {report("Empty output-device list and DSERR_NODRIVER PASS\r\n");ExitProcess(2);}
        ExitProcess(1);
    }
    if (!devices.count) {report("No DirectSound endpoint for the installed WinMM device\r\n");ExitProcess(1);}
    hr=create(&devices.first,&sound,NULL);
    if (FAILED(hr)) {status("DirectSoundCreate",hr);ExitProcess(1);}
    caps.dwSize=sizeof(caps);
    hr=IDirectSound_GetCaps(sound,&caps);
    if (SUCCEEDED(hr)) { report("DirectSound device capabilities ");report_hex(caps.dwFlags);report("\r\n"); }
    if (SUCCEEDED(hr)) hr=IDirectSound_SetCooperativeLevel(sound,GetDesktopWindow(),DSSCL_NORMAL);
    if (SUCCEEDED(hr)) hr=tone(sound,32,&first);
    if (SUCCEEDED(hr)) hr=tone(sound,50,&second);
    if (SUCCEEDED(hr)) hr=IDirectSoundBuffer_SetVolume(second,-200);
    if (SUCCEEDED(hr)) hr=IDirectSoundBuffer_Play(first,0,0,DSBPLAY_LOOPING);
    if (SUCCEEDED(hr)) hr=IDirectSoundBuffer_Play(second,0,0,DSBPLAY_LOOPING);
    if (SUCCEEDED(hr)) hr=IDirectSoundBuffer_GetCurrentPosition(first,&initial,NULL);
    if (SUCCEEDED(hr)) { Sleep(700);hr=IDirectSoundBuffer_GetCurrentPosition(first,&cursor,NULL); }
    if (SUCCEEDED(hr)) hr=IDirectSoundBuffer_GetStatus(second,&state);
    if (SUCCEEDED(hr) && (initial==cursor || !(state&DSBSTATUS_PLAYING) || !(state&DSBSTATUS_LOOPING))) hr=E_FAIL;
    if (SUCCEEDED(hr)) {report("Two simultaneous PCM buffers, wrapped locks and moving playback cursor PASS\r\n");}
    if (second) IDirectSoundBuffer_Stop(second);
    if (first) IDirectSoundBuffer_Stop(first);
    if (SUCCEEDED(hr)) hr=IDirectSoundBuffer_SetCurrentPosition(first,0);
    if (SUCCEEDED(hr)) hr=IDirectSoundBuffer_Play(first,0,0,0);
    if (SUCCEEDED(hr)) {
        for (unsigned i=0;i<160;++i) {
            Sleep(25);hr=IDirectSoundBuffer_GetStatus(first,&state);
            if (FAILED(hr) || !(state&DSBSTATUS_PLAYING)) break;
        }
        if (SUCCEEDED(hr) && (state&DSBSTATUS_PLAYING)) hr=E_FAIL;
    }
    if (SUCCEEDED(hr)) report("Nonlooping playback finished PASS\r\n");else status("DirectSound playback",hr);
    if (second) IDirectSoundBuffer_Release(second);
    if (first) IDirectSoundBuffer_Release(first);
    IDirectSound_Release(sound);
    FreeLibrary(dll);
    ExitProcess(FAILED(hr));
}
