/* Native palette/ExtractIcon comparison, independent of the Flash GDI adapter. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
static HICON icon;
static void line(HANDLE f,const char *name,DWORD value){char b[160];DWORD n,w;
 n=wsprintfA(b,"%s %08lX\r\n",name,value);WriteFile(f,b,n,&w,0);}
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){
 if(m==WM_PAINT){PAINTSTRUCT p;HDC dc=BeginPaint(w,&p);RECT r;HBRUSH brush;HANDLE f;int i;
  COLORREF colors[4]={RGB(128,0,0),RGB(69,2,2),RGB(255,0,0),RGB(128,0,128)};
  const char *names[4]={"Dark red","Original red","Bright red","Purple"};
  f=CreateFileA("C:\\FP32\\ICONCOL.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
  line(f,"Bits per pixel",GetDeviceCaps(dc,BITSPIXEL));line(f,"Planes",GetDeviceCaps(dc,PLANES));line(f,"Palette entries",GetDeviceCaps(dc,SIZEPALETTE));
  for(i=0;i<4;i++){r.left=10+i*100;r.top=20;r.right=r.left+70;r.bottom=70;
   brush=CreateSolidBrush(colors[i]);FillRect(dc,&r,brush);DeleteObject(brush);
   TextOutA(dc,r.left,80,names[i],lstrlenA(names[i]));line(f,names[i],GetPixel(dc,r.left+30,40));}
  DrawIcon(dc,20,120,icon);TextOutA(dc,70,130,"Installed launcher icon",23);
  line(f,"Icon red sample",GetPixel(dc,27,127));CloseHandle(f);EndPaint(w,&p);return 0;}
 if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
 return DefWindowProcA(w,m,a,b);
}
void WINAPI entry(void){WNDCLASSA c;MSG m;DWORD i;
 for(i=0;i<sizeof(c);i++)((BYTE*)&c)[i]=0;
 c.hInstance=GetModuleHandleA(0);c.lpszClassName="FP32ColorProbe";c.lpfnWndProc=proc;
 c.hbrBackground=(HBRUSH)GetStockObject(WHITE_BRUSH);c.hCursor=LoadCursorA(0,IDC_ARROW);
 if(!RegisterClassA(&c))ExitProcess(1);
 icon=ExtractIconA(c.hInstance,"C:\\F32TEST\\FP32.EXE",0);if((UINT_PTR)icon<=1)ExitProcess(2);
 if(!CreateWindowA(c.lpszClassName,"Native icon color comparison",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,100,430,220,0,0,c.hInstance,0))ExitProcess(3);
 while(GetMessageA(&m,0,0,0)>0){TranslateMessage(&m);DispatchMessageA(&m);}
 DestroyIcon(icon);ExitProcess(0);
}
