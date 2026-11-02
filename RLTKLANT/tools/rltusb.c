/* Persistent USB adapter configuration for NT 3.1 through NT 4.0.
 * Uses existing NCPA instances; never creates bindings or edits TCP/IP.
 * Freestanding Win32 executable: no CRT or optional setup DLL imports.
 */
#define _WIN32_WINNT 0x0400
#include <windows.h>

#define CARDS "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\NetworkCards"
#define SERVICES "SYSTEM\\CurrentControlSet\\Services\\"
typedef struct { char service[64]; DWORD usb; char mac[32]; } CARD;
typedef struct { DWORD type, size; BYTE data[256]; int present; } VALUE;
static CARD cards[64];
static unsigned count;
static void clear(void *p,unsigned n){volatile BYTE*b=p;while(n--)*b++=0;}
static void out(const char*s){DWORD w;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,lstrlenA(s),&w,NULL);}
static void error(const char*s,LONG code){char h[11];unsigned i;h[0]='0';h[1]='x';h[10]=0;for(i=0;i<8;i++)h[i+2]="0123456789ABCDEF"[((DWORD)code>>(28-4*i))&15];out(s);out(": ");out(h);out("\r\n");}
static int service_name(const char*s){unsigned i;const char*p="RLTKLANT";for(i=0;p[i];i++)if((s[i]|32)!=(p[i]|32))return 0;if(!s[i])return 0;for(;s[i];i++)if(s[i]<'0'||s[i]>'9')return 0;return i<64;}
static int hexchar(char c){if(c>='0'&&c<='9')return c-'0';c=(char)(c|32);return c>='a'&&c<='f'?c-'a'+10:-1;}
static int mac_address(const char*s){unsigned i,any=0;int a,b;if(lstrlenA(s)!=12)return 0;for(i=0;i<6;i++){a=hexchar(s[2*i]);b=hexchar(s[2*i+1]);if(a<0||b<0||(!i&&(b&1)))return 0;any|=(unsigned)((a<<4)|b);}return any!=0;}
static LONG string_value(HKEY key,const char*name,char*outbuf,DWORD capacity)
{
 DWORD type=0,size=capacity;LONG r;clear(outbuf,capacity);
 r=RegQueryValueExA(key,name,NULL,&type,(BYTE*)outbuf,&size);
 if(r)return r;
 if(type!=REG_SZ||!size||size>capacity||outbuf[size-1])return ERROR_INVALID_DATA;
 return ERROR_SUCCESS;
}
static LONG dword_value(HKEY key,const char*name,DWORD*value)
{
 DWORD type=0,size=sizeof(*value);LONG r=RegQueryValueExA(key,name,NULL,&type,(BYTE*)value,&size);
 return r?r:(type==REG_DWORD&&size==sizeof(*value)?ERROR_SUCCESS:ERROR_INVALID_DATA);
}
static LONG parameters(const char*service,REGSAM access,HKEY*key)
{
 char path[160];if(!service_name(service))return ERROR_INVALID_PARAMETER;
 lstrcpyA(path,SERVICES);lstrcatA(path,service);lstrcatA(path,"\\Parameters");
 return RegOpenKeyExA(HKEY_LOCAL_MACHINE,path,0,access,key);
}
static LONG enumerate(void)
{
 HKEY root,key,params;DWORD i;LONG r;char child[128],product[128],service[64];
 r=RegOpenKeyExA(HKEY_LOCAL_MACHINE,CARDS,0,KEY_READ,&root);if(r)return r;
 for(i=0;;i++){
  DWORD length=sizeof(child);FILETIME ft;
  r=RegEnumKeyExA(root,i,child,&length,NULL,NULL,NULL,&ft);
  if(r==ERROR_NO_MORE_ITEMS){r=ERROR_SUCCESS;break;}if(r)break;
  r=RegOpenKeyExA(root,child,0,KEY_READ,&key);if(r)break;
  r=string_value(key,"ProductName",product,sizeof(product));
  if(r==ERROR_FILE_NOT_FOUND||(!r&&lstrcmpiA(product,"RLTKLANT"))){RegCloseKey(key);continue;}
  if(!r)r=string_value(key,"ServiceName",service,sizeof(service));RegCloseKey(key);
  if(r)break;
  if(!service_name(service)||count==64){r=ERROR_INVALID_DATA;break;}
  {unsigned j;for(j=0;j<count;j++)if(!lstrcmpiA(cards[j].service,service))break;
   if(j<count){r=ERROR_INVALID_DATA;break;}}
  r=parameters(service,KEY_READ,&params);if(r)break;
  lstrcpyA(cards[count].service,service);
  r=dword_value(params,"UsbNtAdapter",&cards[count].usb);
  if(r==ERROR_FILE_NOT_FOUND){r=0;cards[count].usb=0;}
  if(!r&&cards[count].usb){r=string_value(params,"UsbPermanentAddress",cards[count].mac,sizeof(cards[count].mac));if(!r&&!mac_address(cards[count].mac))r=ERROR_INVALID_DATA;}
  RegCloseKey(params);if(r)break;count++;
 }
 RegCloseKey(root);return r;
}
static LONG usbnt_ready(void)
{
 HKEY key;DWORD start,type;char group[128];LONG r;
 r=RegOpenKeyExA(HKEY_LOCAL_MACHINE,SERVICES "usbnt",0,KEY_READ,&key);if(r)return r;
 r=dword_value(key,"Start",&start);if(!r)r=dword_value(key,"Type",&type);
 if(!r)r=string_value(key,"Group",group,sizeof(group));RegCloseKey(key);
 if(r)return r;
 return start==SERVICE_BOOT_START&&type==SERVICE_KERNEL_DRIVER&&
  (!lstrcmpiA(group,"SCSI miniport")||!lstrcmpiA(group,"System Bus Extender"))?0:ERROR_INVALID_DATA;
}
static LONG snapshot(HKEY key,const char*name,VALUE*v)
{
 LONG r;clear(v,sizeof(*v));v->size=sizeof(v->data);
 r=RegQueryValueExA(key,name,NULL,&v->type,v->data,&v->size);
 if(r==ERROR_FILE_NOT_FOUND)return 0;
 if(!r)v->present=1;return r;
}
static LONG restore(HKEY key,const char*name,const VALUE*v)
{
 LONG r;if(v->present)return RegSetValueExA(key,name,0,v->type,v->data,v->size);
 r=RegDeleteValueA(key,name);return r==ERROR_FILE_NOT_FOUND?0:r;
}
static LONG configure(const CARD*card,const char*mac)
{
 static const char*names[]={"UsbPermanentAddress","BusType","UsbNtAdapter"};
 VALUE saved[3];HKEY key;LONG r,rollback=0;unsigned i;DWORD usb=mac?1:0,bus=mac?0:5;
 r=parameters(card->service,KEY_QUERY_VALUE|KEY_SET_VALUE,&key);if(r)return r;
 for(i=0;i<3;i++){r=snapshot(key,names[i],&saved[i]);if(r){RegCloseKey(key);return r;}}
 /* Publish the selector last. A failed write restores every original value,
    including its type and absence; bindings and TCP/IP are never touched. */
 if(mac)r=RegSetValueExA(key,names[0],0,REG_SZ,(const BYTE*)mac,13);
 if(!r)r=RegSetValueExA(key,names[1],0,REG_DWORD,(const BYTE*)&bus,sizeof(bus));
 if(!r)r=RegSetValueExA(key,names[2],0,REG_DWORD,(const BYTE*)&usb,sizeof(usb));
 if(r){for(i=3;i>0;i--){LONG e=restore(key,names[i-1],&saved[i-1]);if(e)rollback=e;}
  if(rollback)error("ROLLBACK FAILED: inspect the adapter Parameters before reboot",rollback);}
 RegCloseKey(key);return r;
}
void Entry(void)
{
 char command[512],*args[6],*p;unsigned n=0,i,selected=64;LONG r;DWORD version=GetVersion();
 /* GetVersionEx is absent from the original NT 3.1 KERNEL32. */
 if((version&0x80000000UL)||(version&255)<3||(version&255)>4){out("RLTUSB supports Windows NT 3.1 through NT 4.0 only.\r\n");ExitProcess(2);}
 if(lstrlenA(GetCommandLineA())>=(int)sizeof(command)){out("Command too long.\r\n");ExitProcess(2);}
 lstrcpyA(command,GetCommandLineA());p=command;
 while(*p&&n<6){while(*p==' ')p++;if(!*p)break;if(*p=='"'){args[n++]=++p;while(*p&&*p!='"')p++;}else{args[n++]=p;while(*p&&*p!=' ')p++;}if(*p)*p++=0;}
 if(n<2||!((n==2&&!lstrcmpiA(args[1],"list"))||(n==4&&!lstrcmpiA(args[1],"usb"))||(n==3&&!lstrcmpiA(args[1],"pci")))){
  out("RLTUSB list\r\nRLTUSB usb RLTKLANT1 525400abcdef\r\nRLTUSB pci RLTKLANT1\r\nInstall each adapter through Network Setup first. USBNT must be installed\r\nand configured for boot start before selecting USB. Reboot after changes.\r\n");ExitProcess(2);}
 if(n==4&&!mac_address(args[3])){out("Invalid MAC: use twelve hexadecimal digits, nonzero and unicast.\r\n");ExitProcess(2);}
 r=enumerate();if(r){error("Cannot read registered RLTKLANT adapters",r);ExitProcess(1);}
 for(i=0;i<count;i++){
  if(n==2){out(cards[i].service);out(cards[i].usb?" USB ":" PCI/PCIe");if(cards[i].usb)out(cards[i].mac);out("\r\n");}
  else if(!lstrcmpiA(cards[i].service,args[2]))selected=i;
 }
 if(n==2){if(!count)out("No RLTKLANT adapters registered. Use Network Setup first.\r\n");ExitProcess(0);}
 if(selected==64){out("Adapter is not registered by RLTKLANT Network Setup.\r\n");ExitProcess(2);}
 if(n==4){
  for(i=0;i<count;i++)if(i!=selected&&cards[i].usb&&!lstrcmpiA(cards[i].mac,args[3])){out("This permanent MAC is already assigned to another adapter.\r\n");ExitProcess(2);}
  r=usbnt_ready();if(r){error("USBNT must use boot start and an early bus load-order group",r);ExitProcess(1);}
 }
 r=configure(&cards[selected],n==4?args[3]:NULL);
 if(r){error("Configuration failed",r);ExitProcess(1);}
 out("Configuration saved for ");out(cards[selected].service);out(". Reboot to apply.\r\n");
 out("Existing protocol bindings and TCP/IP settings were preserved.\r\n");ExitProcess(0);
}
