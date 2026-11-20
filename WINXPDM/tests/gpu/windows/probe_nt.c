/* Native system-start client. Exercises the same IOCTL as a Win32 program.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <ntddk.h>
#include "scene.h"
#include "engines.h"
static WxpFrame frame;
static void emit(const char *s)
{ while (*s) __asm__ volatile("outb %0,$0xe9" : : "a"((unsigned char)*s++)); }
static void hex(uint32_t value)
{
    const char table[] = "0123456789abcdef";
    char buf[9]; unsigned i;
    buf[8] = 0;
    for (i = 8; i; --i) { buf[i-1] = table[value & 15]; value >>= 4; }
    emit(buf);
}
static NTSTATUS check(void)
{
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK iosb;
    UNICODE_STRING name;
    HANDLE device;
    NTSTATUS status;
    unsigned attempt;
    LARGE_INTEGER delay; delay.QuadPart = -1000000;
    emit("WINXPDM native NT client\n");
    RtlInitUnicodeString(&name,L"\\Device\\WxpGpu");
    InitializeObjectAttributes(&attr,&name,OBJ_CASE_INSENSITIVE,NULL,NULL);
    for (attempt = 0; attempt < 600; ++attempt) {
        status = ZwOpenFile(&device,GENERIC_READ|GENERIC_WRITE|SYNCHRONIZE,&attr,&iosb,
                        FILE_SHARE_READ|FILE_SHARE_WRITE,FILE_SYNCHRONOUS_IO_NONALERT);
        if (NT_SUCCESS(status)) break;
        KeDelayExecutionThread(KernelMode,FALSE,&delay);
    }
    if (!NT_SUCCESS(status)) { emit("open="); hex(status); emit("\n"); return status; }
    check_engines(emit);
    status = ZwDeviceIoControlFile(device,NULL,NULL,NULL,&iosb,WXP_GPU_IOCTL,
                                   (PVOID)&scene,sizeof(scene),&frame,sizeof(frame));
    emit("ioctl="); hex(status); emit(" bytes="); hex((uint32_t)iosb.Information);
    emit(" draw="); hex(frame.status); emit(" command="); hex(frame.request);
    emit(" response="); hex(frame.response); emit(" commands="); hex(frame.completed); emit(" fence="); hex(frame.fence);
    emit(" outputs="); hex(frame.outputs); emit(" enabled="); hex(frame.enabled_outputs);
    emit(" edid="); hex(frame.edid_outputs); emit(" scanouts="); hex(frame.tested_outputs); emit(" center="); hex(frame.pixels[16*WXP_SIDE+16]); emit("\n");
    if (!NT_SUCCESS(status) || iosb.Information != sizeof(frame) || !check_scene(&frame)) {
        emit("pixel check FAIL\n"); ZwClose(device); return STATUS_UNSUCCESSFUL;
    }
    emit("interpolation and depth PASS 1024 pixels\n");
    status = ZwDeviceIoControlFile(device,NULL,NULL,NULL,&iosb,WXP_GPU_IOCTL,
                                   (PVOID)&textured,sizeof(textured),&frame,sizeof(frame));
    ZwClose(device);
    if (!NT_SUCCESS(status) || iosb.Information != sizeof(frame) || !check_texture(&frame)) {
        emit("texture FAIL\n"); return STATUS_UNSUCCESSFUL;
    }
    emit("texture PASS 1024 pixels\n");
    return STATUS_SUCCESS;
}

static void NTAPI worker(PVOID arg)
{
    (void)arg;
    PsTerminateSystemThread(check());
}
NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    HANDLE thread;
    NTSTATUS status;
    (void)driver; (void)path;
    status = PsCreateSystemThread(&thread,0,NULL,NULL,NULL,worker,NULL);
    if (NT_SUCCESS(status)) ZwClose(thread);
    return status;
}
