#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* Guest-only bounded debugger: observes actual startup faults without changing
   target memory or reporting an unsupported API as successful. */
static HANDLE logh;
static HANDLE worker_handles[64];
static DWORD worker_ids[64],worker_count;
static void value(const char*s,DWORD v){char b[160];DWORD n=0,i,w;const char*x="0123456789ABCDEF";while(*s)b[n++]=*s++;b[n++]=' ';for(i=0;i<8;i++)b[n++]=x[(v>>(28-4*i))&15];b[n++]='\r';b[n++]='\n';WriteFile(logh,b,n,&w,0);FlushFileBuffers(logh);}
void WINAPI entry(void){STARTUPINFOA si;PROCESS_INFORMATION pi;DEBUG_EVENT e;DWORD i,start,status,limit=30000;BOOL done=FALSE;static char fallback[]="C:\\FP32\\FP32TEST.EXE C:\\FP32\\SMOKE.SWF";char*command=GetCommandLineA();
 if(*command=='"'){command++;while(*command&&*command!='"')command++;if(*command)command++;}else while(*command&&*command!=' ')command++;while(*command==' ')command++;if(!*command)command=fallback;
 {const char *prefix="/timeout=";DWORD k=0,seconds=0;while(prefix[k]&&command[k]==prefix[k])k++;if(!prefix[k]){command+=k;while(*command>='0'&&*command<='9'){seconds=seconds*10+*command++-'0';if(seconds>600)ExitProcess(ERROR_INVALID_PARAMETER);}if(!seconds||*command!=' ')ExitProcess(ERROR_INVALID_PARAMETER);limit=seconds*1000;while(*command==' ')command++;}}
 for(i=0;i<sizeof(si);i++)((BYTE*)&si)[i]=0;si.cb=sizeof(si);
 logh=CreateFileA("C:\\FP32\\DEBUG.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 value("Start tick",GetTickCount());value("Watchdog milliseconds",limit);
 if(!DeleteFileA("C:\\FP32\\TRACE.LOG")&&GetLastError()!=ERROR_FILE_NOT_FOUND){value("Trace cleanup error",GetLastError());CloseHandle(logh);ExitProcess(2);}
 if(!CreateProcessA(0,command,0,0,FALSE,DEBUG_ONLY_THIS_PROCESS,0,"C:\\FP32",&si,&pi)){value("CreateProcess error",GetLastError());CloseHandle(logh);ExitProcess(1);}
 start=GetTickCount();while(!done){
  if(!WaitForDebugEvent(&e,1000)){if(GetTickCount()-start>limit){CONTEXT context;DWORD stack[24],bytes;SuspendThread(pi.hThread);context.ContextFlags=CONTEXT_CONTROL|CONTEXT_INTEGER;if(GetThreadContext(pi.hThread,&context)){value("Watchdog EIP",context.Eip);value("Watchdog ESP",context.Esp);value("Watchdog EBP",context.Ebp);if(ReadProcessMemory(pi.hProcess,(void*)context.Esp,stack,sizeof(stack),&bytes))for(i=0;i<bytes/4;i++)value("Watchdog stack",stack[i]);}ResumeThread(pi.hThread);{DWORD t,j;for(t=0;t<worker_count;t++){HANDLE h=worker_handles[t];if(SuspendThread(h)==(DWORD)-1)continue;context.ContextFlags=CONTEXT_CONTROL|CONTEXT_INTEGER;if(GetThreadContext(h,&context)){value("Watchdog worker ID",worker_ids[t]);value("Worker EIP",context.Eip);value("Worker ESP",context.Esp);value("Worker EBP",context.Ebp);value("Worker EAX",context.Eax);value("Worker EBX",context.Ebx);value("Worker ESI",context.Esi);value("Worker EDI",context.Edi);if(ReadProcessMemory(pi.hProcess,(void*)context.Esp,stack,sizeof(stack),&bytes))for(j=0;j<bytes/4;j++)value("Worker stack",stack[j]);}ResumeThread(h);}}value("Watchdog termination",99);TerminateProcess(pi.hProcess,99);}continue;}
  status=DBG_CONTINUE;
  switch(e.dwDebugEventCode){
   case CREATE_PROCESS_DEBUG_EVENT:value("Image base",(DWORD)e.u.CreateProcessInfo.lpBaseOfImage);value("Entry",(DWORD)e.u.CreateProcessInfo.lpStartAddress);if(e.u.CreateProcessInfo.hFile)CloseHandle(e.u.CreateProcessInfo.hFile);break;
   case LOAD_DLL_DEBUG_EVENT:value("DLL base",(DWORD)e.u.LoadDll.lpBaseOfDll);if(e.u.LoadDll.hFile)CloseHandle(e.u.LoadDll.hFile);break;
   case EXCEPTION_DEBUG_EVENT:{BYTE code[16];DWORD n;value("Exception",e.u.Exception.ExceptionRecord.ExceptionCode);value("Address",(DWORD)e.u.Exception.ExceptionRecord.ExceptionAddress);value("First chance",e.u.Exception.dwFirstChance);if(e.u.Exception.ExceptionRecord.ExceptionCode==0xc06d007f&&e.u.Exception.ExceptionRecord.NumberParameters){DWORD info[9],bytes;char name[128];if(ReadProcessMemory(pi.hProcess,(void*)e.u.Exception.ExceptionRecord.ExceptionInformation[0],info,sizeof(info),&bytes)){for(i=0;i<9;i++)value("Delay info",info[i]);if(ReadProcessMemory(pi.hProcess,(void*)info[3],name,sizeof(name)-1,&bytes)){name[bytes]=0;value(name,info[3]);}if(info[4]&&ReadProcessMemory(pi.hProcess,(void*)info[5],name,sizeof(name)-1,&bytes)){name[bytes]=0;value(name,info[5]);}}}if(ReadProcessMemory(pi.hProcess,e.u.Exception.ExceptionRecord.ExceptionAddress,code,sizeof(code),&n)){for(i=0;i<n;i+=4)value("Instruction DWORD",*(DWORD*)(code+i));}if((e.u.Exception.ExceptionRecord.ExceptionCode==EXCEPTION_ILLEGAL_INSTRUCTION||e.u.Exception.ExceptionRecord.ExceptionCode==EXCEPTION_ACCESS_VIOLATION)&&e.dwThreadId==pi.dwThreadId){CONTEXT context;DWORD exception_stack[24];context.ContextFlags=CONTEXT_CONTROL|CONTEXT_INTEGER;if(GetThreadContext(pi.hThread,&context)){value("Main ESP",context.Esp);value("Main EAX",context.Eax);value("Main ESI",context.Esi);value("Main EDI",context.Edi);value("Main EBX",context.Ebx);value("Main EDX",context.Edx);value("Main EBP",context.Ebp);if(ReadProcessMemory(pi.hProcess,(void*)context.Esp,exception_stack,sizeof(exception_stack),&n))for(i=0;i<n/4;i++)value("Stack DWORD",exception_stack[i]);}}if(e.u.Exception.ExceptionRecord.ExceptionCode!=EXCEPTION_BREAKPOINT)status=DBG_EXCEPTION_NOT_HANDLED;break;}
   case CREATE_THREAD_DEBUG_EVENT:value("Thread start",(DWORD)e.u.CreateThread.lpStartAddress);if(worker_count<64){worker_ids[worker_count]=e.dwThreadId;worker_handles[worker_count++]=e.u.CreateThread.hThread;}else CloseHandle(e.u.CreateThread.hThread);break;
   case EXIT_PROCESS_DEBUG_EVENT:value("Exit",e.u.ExitProcess.dwExitCode);done=TRUE;break;
  }
  ContinueDebugEvent(e.dwProcessId,e.dwThreadId,status);
 }
 for(i=0;i<worker_count;i++)CloseHandle(worker_handles[i]);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);CloseHandle(logh);ExitProcess(0);
}
