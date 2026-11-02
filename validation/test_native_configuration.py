"""Exercise the production USB request builder without an optional USBD import."""
from pathlib import Path
import re, subprocess, tempfile
source=(Path(__file__).resolve().parents[1]/'RLTKLANT/src/native_urb.c').read_text()
start=source.index('static PURB SelectConfiguration(')
end=source.index('\nvoid *RtlUrbOpen(',start)
prefix=r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef uint16_t USHORT;typedef uint8_t UCHAR;
typedef struct {UCHAR bNumEndpoints,bInterfaceNumber,bAlternateSetting,bInterfaceClass,bInterfaceSubClass,bInterfaceProtocol;} USB_INTERFACE_DESCRIPTOR,*PUSB_INTERFACE_DESCRIPTOR;
typedef struct {unsigned unused;} USB_CONFIGURATION_DESCRIPTOR,*PUSB_CONFIGURATION_DESCRIPTOR;
typedef struct {USHORT MaximumPacketSize;UCHAR EndpointAddress,Interval;uint32_t PipeType;void*PipeHandle;ULONG MaximumTransferSize,PipeFlags;} USBD_PIPE_INFORMATION;
typedef struct {USHORT Length;UCHAR InterfaceNumber,AlternateSetting,Class,SubClass,Protocol,Reserved;void*InterfaceHandle;ULONG NumberOfPipes;USBD_PIPE_INFORMATION Pipes[1];} USBD_INTERFACE_INFORMATION,*PUSBD_INTERFACE_INFORMATION;
typedef struct {USHORT Length,Function;ULONG Status;void*UsbdDeviceHandle;ULONG UsbdFlags;} HDR;
struct _URB_SELECT_CONFIGURATION {HDR Hdr;PUSB_CONFIGURATION_DESCRIPTOR ConfigurationDescriptor;void*ConfigurationHandle;USBD_INTERFACE_INFORMATION Interface;};
typedef union {HDR UrbHeader;struct _URB_SELECT_CONFIGURATION UrbSelectConfiguration;} URB,*PURB;
#define FIELD_OFFSET(t,f) offsetof(t,f)
#define RTL_URB_TAG 0x75544c52UL
#define NonPagedPool 0
#define URB_FUNCTION_SELECT_CONFIGURATION 0
#define RtlZeroMemory(p,n) memset(p,0,n)
static int fail;static unsigned calls;static size_t allocated;
static void*ExAllocatePoolWithTag(int pool,size_t n,unsigned tag){assert(!pool&&tag==RTL_URB_TAG);calls++;allocated=n;return fail?NULL:malloc(n);}
'''
main=r'''
int main(void){USB_CONFIGURATION_DESCRIPTOR c={0};USB_INTERFACE_DESCRIPTOR d={0};unsigned n,i;
 assert(!SelectConfiguration(&c,NULL)&&!calls);
 for(n=0;n<256;n++){
  PURB u;PUSBD_INTERFACE_INFORMATION p;d.bNumEndpoints=n;d.bInterfaceNumber=3;d.bAlternateSetting=0;
  d.bInterfaceClass=255;d.bInterfaceSubClass=42;d.bInterfaceProtocol=7;calls=0;
  u=SelectConfiguration(&c,&d);
  if(!n||n>16){assert(!u&&!calls);continue;}
  assert(u&&calls==1&&allocated==u->UrbHeader.Length&&u->UrbHeader.Function==0);
  assert(u->UrbSelectConfiguration.ConfigurationDescriptor==&c&&!u->UrbSelectConfiguration.ConfigurationHandle);
  p=&u->UrbSelectConfiguration.Interface;
  assert(p->Length+offsetof(struct _URB_SELECT_CONFIGURATION,Interface)==allocated);
  assert(p->NumberOfPipes==n&&p->InterfaceNumber==3&&!p->AlternateSetting);
  assert(p->Class==255&&p->SubClass==42&&p->Protocol==7&&!p->InterfaceHandle&&!p->Reserved);
  for(i=0;i<n;i++)assert(p->Pipes[i].MaximumTransferSize==4096&&!p->Pipes[i].PipeHandle&&!p->Pipes[i].PipeFlags);
  free(u);fail=1;assert(!SelectConfiguration(&c,&d));fail=0;
 }
 puts("PASS: production SELECT_CONFIGURATION variable pipe bounds, zero initialization, descriptor metadata and allocation failures");
}
'''
with tempfile.TemporaryDirectory() as td:
    p=Path(td);(p/'test.c').write_text(prefix+source[start:end]+main)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
