#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "mmsystem.h"
#include "dsound.h"
#include "dsdriver.h"
#include "native_wave.h"

MMRESULT wx_wave_description(UINT device, BOOL capture, DSDRIVERDESC *desc)
{
    MMRESULT err;
    ZeroMemory(desc, sizeof(*desc));
    if (capture) {
        WAVEINCAPSA caps;
        err = waveInGetDevCapsA(device, &caps, sizeof(caps));
        if (!err) lstrcpynA(desc->szDesc, caps.szPname, sizeof(desc->szDesc));
    } else {
        WAVEOUTCAPSA caps;
        err = waveOutGetDevCapsA(device, &caps, sizeof(caps));
        if (!err) lstrcpynA(desc->szDesc, caps.szPname, sizeof(desc->szDesc));
    }
    if (!err) {
        desc->dnDevNode = device;
        desc->dwFlags = DSDDESC_DOMMSYSTEMOPEN | DSDDESC_DOMMSYSTEMSETFORMAT;
        /* Native WinMM supplies a device description but no driver filename
         * through GetDevCaps. An empty filename is permitted by DirectSound. */
    }
    return err;
}

void wx_choose_primary_format(UINT device, WAVEFORMATEX *format)
{
    static const DWORD rates[] = {48000, 44100, 22050, 11025, 8000};
    static const WORD channels[] = {2, 1};
    static const WORD bits[] = {16, 8};
    if (waveOutOpen(NULL, device, format, 0, 0, WAVE_FORMAT_QUERY) == MMSYSERR_NOERROR) return;
    WAVEFORMATEX trial = *format;
    for (unsigned r = 0; r < sizeof(rates) / sizeof(rates[0]); ++r) {
        for (unsigned c = 0; c < 2; ++c) {
            for (unsigned b = 0; b < 2; ++b) {
                trial.nSamplesPerSec = rates[r];
                trial.nChannels = channels[c];
                trial.wBitsPerSample = bits[b];
                trial.nBlockAlign = trial.nChannels * trial.wBitsPerSample / 8;
                trial.nAvgBytesPerSec = trial.nSamplesPerSec * trial.nBlockAlign;
                if (waveOutOpen(NULL, device, &trial, 0, 0, WAVE_FORMAT_QUERY) == MMSYSERR_NOERROR) {
                    *format = trial;
                    return;
                }
            }
        }
    }
}

