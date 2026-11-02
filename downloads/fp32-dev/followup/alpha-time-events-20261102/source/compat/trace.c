typedef unsigned long U32;
typedef void *H;
#define API __declspec(dllimport)
#define W __stdcall
API H W CreateFileA(const char*,U32,U32,void*,U32,U32,H);
API U32 W SetFilePointer(H,long,long*,U32);
API int W WriteFile(H,const void*,U32,U32*,void*);
API int W CloseHandle(H);
API H W CreateMutexA(void*,int,const char*);
API U32 W WaitForSingleObject(H,U32);
API int W ReleaseMutex(H);
API void W ExitProcess(U32);
API U32 W GetLastError(void);
API void W SetLastError(U32);
#ifdef FP_OPTIONAL_TRACE
API U32 W GetEnvironmentVariableA(const char*,char*,U32);
#endif
#ifdef FP_GDI
extern int W fp_gdi_main(U32);
#endif
#ifdef FP_CRYPTO
extern int W fp_crypto_main(U32);
#endif
int W DllMain(H module,U32 reason,void *reserved){
#ifdef FP_GDI
return fp_gdi_main(reason);
#elif defined(FP_CRYPTO)
return fp_crypto_main(reason);
#else
return 1;
#endif
}
static void write_record(const char *name,U32 ret){
 char out[256];U32 n=0,w,i;H h,lock;const char *hex="0123456789ABCDEF";
 while(*name && n<200)out[n++]=*name++;
 out[n++]=' ';out[n++]='@';out[n++]=' ';
 for(i=0;i<8;i++)out[n++]=hex[(ret>>(28-4*i))&15];
 out[n++]='\r';out[n++]='\n';
 lock=CreateMutexA(0,0,"FP10PortDiagnosticLog");
 if(!lock)return;
 if(WaitForSingleObject(lock,5000)>0x80){CloseHandle(lock);return;}
 h=CreateFileA("C:\\FP10\\TRACE.LOG",0x40000000,3,0,4,0x80,0);
 if(h!=(H)-1){SetFilePointer(h,0,0,2);WriteFile(h,out,n,&w,0);CloseHandle(h);}
 ReleaseMutex(lock);CloseHandle(lock);

}

/* Fatal diagnostics remain visible even when routine tracing is disabled. */
void W FpReport(const char *name,U32 ret){U32 error=GetLastError();write_record(name,ret);SetLastError(error);}

void W FpObserve(const char *name,U32 ret){
 U32 error=GetLastError();
#ifdef FP_OPTIONAL_TRACE
 /* Opt in per process. No registry/global setting and no per-frame disk I/O
    in ordinary playback. Missing required APIs always retain their record. */
 static int initialized,enabled;
 if(!initialized){char value[2];enabled=GetEnvironmentVariableA("FP32_TRACE",value,2)==1&&value[0]=='1';initialized=1;}
 if(enabled)write_record(name,ret);
#else
 write_record(name,ret);
#endif
 SetLastError(error);
}
void W FpMissing(const char *name,U32 ret){write_record(name,ret);ExitProcess(120);}
