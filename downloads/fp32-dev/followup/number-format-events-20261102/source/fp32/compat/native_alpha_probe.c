#define WIN32_LEAN_AND_MEAN
#include <windows.h>
void WINAPI entry(void){DWORD pixels[4]={0x80800000,0x80800000,0x80800000,0x80800000},output[4]={0,0,0,0},n,w;BITMAP bm;BITMAPINFO bi;HDC dc=CreateCompatibleDC(0);HBITMAP b=CreateBitmap(2,2,1,32,pixels);HANDLE logh;char text[256];UINT i;
logh=CreateFileA("C:\\FP32\\NATALPHA.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
for(i=0;i<sizeof(bi);i++)((BYTE*)&bi)[i]=0;for(i=0;i<sizeof(bm);i++)((BYTE*)&bm)[i]=0;
n=GetObjectW(b,sizeof(bm),&bm);wsprintfA(text,"CreateBitmap %08lx GetObject %lu bpp %u planes %u\r\n",b,n,bm.bmBitsPixel,bm.bmPlanes);WriteFile(logh,text,lstrlenA(text),&w,0);
n=GetBitmapBits(b,sizeof(output),output);wsprintfA(text,"GetBitmapBits %lu pixel %08lx\r\n",n,output[0]);WriteFile(logh,text,lstrlenA(text),&w,0);
bi.bmiHeader.biSize=40;bi.bmiHeader.biWidth=2;bi.bmiHeader.biHeight=2;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
n=GetDIBits(dc,b,0,2,output,&bi,DIB_RGB_COLORS);wsprintfA(text,"GetDIBits %lu pixel %08lx error %lu\r\n",n,output[0],GetLastError());WriteFile(logh,text,lstrlenA(text),&w,0);
DeleteObject(b);DeleteDC(dc);CloseHandle(logh);ExitProcess(0);}
