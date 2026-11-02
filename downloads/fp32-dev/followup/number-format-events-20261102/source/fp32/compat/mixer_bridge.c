/* Use native mixer APIs when available, otherwise the original NT3.51
   provider. Native NT3.1 wave/MIDI handles must not cross provider heaps. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
static HMODULE provider;
static volatile LONG provider_guard;
static HMODULE load_provider(void)
{
    HMODULE result;MEMORY_BASIC_INFORMATION memory;char path[MAX_PATH];DWORD n,i;
    while(InterlockedExchange(&provider_guard,1))Sleep(0);
    result=provider;InterlockedExchange(&provider_guard,0);
    if(!result){
        if(!VirtualQuery((LPCVOID)&provider,&memory,sizeof(memory)))goto done;
        n=GetModuleFileNameA((HMODULE)memory.AllocationBase,path,sizeof(path));
        if(!n||n>=sizeof(path))goto done;
        for(i=n;i&&path[i-1]!='\\';i--);
        if(!i||i+9>=sizeof(path))goto done;lstrcpyA(path+i,"FPMM.DLL");
        result=LoadLibraryA(path);
        if(result){HMODULE existing;
            while(InterlockedExchange(&provider_guard,1))Sleep(0);
            existing=provider;if(!existing)provider=result;
            InterlockedExchange(&provider_guard,0);
            if(existing){FreeLibrary(result);result=existing;}
        }
    }
 done:return result;
}
static FARPROC resolve(const char *name,BOOL *legacy)
{
    HMODULE native=GetModuleHandleA("WINMM.DLL"),module;FARPROC function;
    /* A mixer-only process may not have resolved any forwarded wave API. */
    if(!native)native=LoadLibraryA("WINMM.DLL");
    function=native?GetProcAddress(native,name):0;*legacy=!function;
    if(function)return function;module=load_provider();return module?GetProcAddress(module,name):0;
}
static MMRESULT object_id(HMIXEROBJ *object,DWORD *flags)
{
    typedef MMRESULT(WINAPI*GetID)(HANDLE,UINT*);const char *name=0;GetID get;UINT id;MMRESULT result;HMODULE module;
    if(!(*flags&MIXER_OBJECTF_HANDLE))return MMSYSERR_NOERROR;
    switch(*flags&0x70000000u){
    case MIXER_OBJECTF_MIXER:return MMSYSERR_NOERROR;
    case MIXER_OBJECTF_WAVEOUT:name="waveOutGetID";break;
    case MIXER_OBJECTF_WAVEIN:name="waveInGetID";break;
    case MIXER_OBJECTF_MIDIOUT:name="midiOutGetID";break;
    case MIXER_OBJECTF_MIDIIN:name="midiInGetID";break;
    default:return MMSYSERR_INVALFLAG;
    }
    module=GetModuleHandleA("WINMM.DLL");get=module?(void*)GetProcAddress(module,name):0;
    if(!get)return MMSYSERR_NOTSUPPORTED;
    result=get((HANDLE)*object,&id);if(result)return result;
    *object=(HMIXEROBJ)(UINT_PTR)id;*flags&=~MIXER_OBJECTF_HANDLE;return MMSYSERR_NOERROR;
}
UINT WINAPI fp_mixerGetNumDevs(void)
{typedef UINT(WINAPI*Fn)(void);BOOL legacy;Fn f=(void*)resolve("mixerGetNumDevs",&legacy);return f?f():0;}
MMRESULT WINAPI fp_mixerGetDevCapsA(UINT_PTR id,LPMIXERCAPSA caps,UINT size)
{typedef MMRESULT(WINAPI*Fn)(UINT_PTR,LPMIXERCAPSA,UINT);BOOL legacy;Fn f=(void*)resolve("mixerGetDevCapsA",&legacy);return f?f(id,caps,size):MMSYSERR_NODRIVER;}
MMRESULT WINAPI fp_mixerGetID(HMIXEROBJ object,UINT *id,DWORD flags)
{typedef MMRESULT(WINAPI*Fn)(HMIXEROBJ,UINT*,DWORD);BOOL legacy;MMRESULT r;Fn f=(void*)resolve("mixerGetID",&legacy);if(!f)return MMSYSERR_NODRIVER;if(legacy&&(r=object_id(&object,&flags)))return r;return f(object,id,flags);}
MMRESULT WINAPI fp_mixerOpen(LPHMIXER out,UINT id,DWORD_PTR callback,DWORD_PTR instance,DWORD flags)
{typedef MMRESULT(WINAPI*Fn)(LPHMIXER,UINT,DWORD_PTR,DWORD_PTR,DWORD);BOOL legacy;MMRESULT r;HMIXEROBJ object=(HMIXEROBJ)(UINT_PTR)id;Fn f=(void*)resolve("mixerOpen",&legacy);if(!f)return MMSYSERR_NODRIVER;if(legacy&&(r=object_id(&object,&flags)))return r;return f(out,(UINT)(UINT_PTR)object,callback,instance,flags);}
MMRESULT WINAPI fp_mixerClose(HMIXER mixer)
{typedef MMRESULT(WINAPI*Fn)(HMIXER);BOOL legacy;Fn f=(void*)resolve("mixerClose",&legacy);return f?f(mixer):MMSYSERR_NODRIVER;}
#define OBJECT_CALL(name,Type) \
MMRESULT WINAPI fp_##name(HMIXEROBJ object,Type *data,DWORD flags) \
{typedef MMRESULT(WINAPI*Fn)(HMIXEROBJ,Type*,DWORD);BOOL legacy;MMRESULT r;Fn f=(void*)resolve(#name,&legacy);if(!f)return MMSYSERR_NODRIVER;if(legacy&&(r=object_id(&object,&flags)))return r;return f(object,data,flags);}
OBJECT_CALL(mixerGetLineInfoA,MIXERLINEA)
OBJECT_CALL(mixerGetLineControlsA,MIXERLINECONTROLSA)
OBJECT_CALL(mixerGetControlDetailsA,MIXERCONTROLDETAILS)
OBJECT_CALL(mixerSetControlDetails,MIXERCONTROLDETAILS)
