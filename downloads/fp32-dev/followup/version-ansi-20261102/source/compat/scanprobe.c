#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* Native NT GDI probe: establish SetDIBits partial scanline semantics before
   using them in the DIB compatibility bridge. This does not load FPG.DLL. */
static HANDLE logh;
static void value(const char*s,DWORD v){DWORD n=0,w;char b[16],out[16];int i=0,k=0;while(s[n])n++;WriteFile(logh,s,n,&w,0);do{b[i++]='0'+v%10;v/=10;}while(v);while(i)out[k++]=b[--i];WriteFile(logh,out,k,&w,0);WriteFile(logh,"\r\n",2,&w,0);}
void WINAPI entry(void){BITMAPINFO bi;DWORD px[16],part[4];HDC screen=GetDC(0),dc=CreateCompatibleDC(screen);HBITMAP bm=CreateCompatibleBitmap(screen,4,4);HGDIOBJ old;int i,n;BOOL ok;
 logh=CreateFileA("C:\\FP10\\SCAN.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 for(i=0;i<sizeof(bi);i++)((BYTE*)&bi)[i]=0;bi.bmiHeader.biSize=40;bi.bmiHeader.biWidth=4;bi.bmiHeader.biHeight=4;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
 for(i=0;i<16;i++)px[i]=0xff0000;for(i=0;i<4;i++)part[i]=0x0000ff;
 n=SetDIBits(dc,bm,0,4,px,&bi,DIB_RGB_COLORS);value("Initial lines=",n);
 n=SetDIBits(dc,bm,1,1,part,&bi,DIB_RGB_COLORS);value("Partial lines=",n);old=SelectObject(dc,bm);GdiFlush();
 for(i=0;i<4;i++){value("Top-down row=",i);value("Color=",GetPixel(dc,0,i));}
 ok=GetPixel(dc,0,0)==RGB(255,0,0)&&GetPixel(dc,0,1)==RGB(255,0,0)&&GetPixel(dc,0,2)==RGB(0,0,255)&&GetPixel(dc,0,3)==RGB(255,0,0);value("Bottom-up scan 1 from compact buffer correct=",ok);
 SelectObject(dc,old);DeleteObject(bm);DeleteDC(dc);ReleaseDC(0,screen);CloseHandle(logh);ExitProcess(ok?0:1);
}
