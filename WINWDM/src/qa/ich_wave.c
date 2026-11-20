/* Play distinct channel tones on one named AC'97 or USB audio endpoint. */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>

#define SAMPLE_RATE 48000U
static short samples[SAMPLE_RATE * 2];

int PASCAL WinMain(HINSTANCE app, HINSTANCE previous, LPSTR args, int show)
{
    FILE *report;
    WAVEOUTCAPSA caps;
    WAVEFORMATEX format = {0};
    WAVEHDR header = {0};
    HWAVEOUT device;
    UINT i, endpoint = 0, matches = 0, count;
    MMRESULT result;
    DWORD start;
    BOOL done = FALSE, usb = !lstrcmpiA(args, "/usb");
    (void)app; (void)previous; (void)show;
    if (*args && !usb) return 2;
    CreateDirectoryA("C:\\WDMQA", NULL);
    report = fopen(usb ? "C:\\WDMQA\\USBWAVE.TXT" : "C:\\WDMQA\\ICHWAVE.TXT", "w");
    if (!report) return 2;
    count = waveOutGetNumDevs();
    fprintf(report, "GetVersion 0x%08lX\nwaveOut devices %u\n", GetVersion(), count);
    for (i = 0; i < count; ++i) {
        if (waveOutGetDevCapsA(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) continue;
        fprintf(report, "Device %u: %s, manufacturer %u, product %u\n",
                i, caps.szPname, caps.wMid, caps.wPid);
        if (usb ? (strstr(caps.szPname, "USB") && strstr(caps.szPname, "Audio")) :
            (!strcmp(caps.szPname, "AC'97 Sound Card") ||
             (strstr(caps.szPname, "Intel") && (strstr(caps.szPname, "AC'97") || strstr(caps.szPname, "82801"))))) {
            endpoint = i;
            ++matches;
        }
    }
    if (matches != 1) {
        fprintf(report, "Matching %s endpoints %u\n", usb ? "USB audio" : "AC'97", matches);
        fclose(report);
        return 1;
    }
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.wBitsPerSample = 16;
    format.nChannels = 2;
    format.nSamplesPerSec = SAMPLE_RATE;
    format.nBlockAlign = 4;
    format.nAvgBytesPerSec = SAMPLE_RATE * 4;
    result = waveOutOpen(&device, endpoint, &format, 0, 0, CALLBACK_NULL);
    fprintf(report, "waveOutOpen %u\n", result);
    if (result == MMSYSERR_NOERROR) {
        for (i = 0; i < SAMPLE_RATE; ++i) {
            samples[2*i] = (i * 997U % SAMPLE_RATE < SAMPLE_RATE / 2) ? 4000 : -4000;
            samples[2*i+1] = (i * 1511U % SAMPLE_RATE < SAMPLE_RATE / 2) ? 4000 : -4000;
        }
        header.lpData = (LPSTR)samples;
        header.dwBufferLength = sizeof(samples);
        result = waveOutPrepareHeader(device, &header, sizeof(header));
        fprintf(report, "waveOutPrepareHeader %u\n", result);
        if (result == MMSYSERR_NOERROR) {
            result = waveOutWrite(device, &header, sizeof(header));
            start = GetTickCount();
            while (!result && !(header.dwFlags & WHDR_DONE) && GetTickCount()-start < 7000UL)
                Sleep(20);
            done = !result && (header.dwFlags & WHDR_DONE);
            fprintf(report, "waveOutWrite %u, returned %u frames before reset\n",
                    result, done ? SAMPLE_RATE : 0);
            waveOutReset(device);
            waveOutUnprepareHeader(device, &header, sizeof(header));
        }
        waveOutClose(device);
    }
    return fclose(report) || !done;
}
