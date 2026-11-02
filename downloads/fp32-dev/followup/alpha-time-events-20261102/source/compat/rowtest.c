#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* Exercise partial uploads against native GetPixel, including padded 24-bit
   rows, both row orientations, and unchanged rows between dirty endpoints. */
static HANDLE logh;static DWORD failures,checks;
static void value(const char*s,DWORD v){DWORD n=0,w;char b[16],out[16];int i=0,k=0;while(s[n])n++;WriteFile(logh,s,n,&w,0);do{b[i++]='0'+v%10;v/=10;}while(v);while(i)out[k++]=b[--i];WriteFile(logh,out,k,&w,0);WriteFile(logh,"\r\n",2,&w,0);}
static void check(BOOL ok,const char*s){checks++;if(!ok)failures++;value(s,ok);}
static void pixel(BYTE*p,int stride,int bytes,int x,int row,DWORD rgb){p+=row*stride+x*bytes;p[0]=rgb;p[1]=rgb>>8;p[2]=rgb>>16;if(bytes==4)p[3]=0;}
void WINAPI entry(void){HMODULE lib=LoadLibraryA("C:\\FP10\\TRACE\\FPG.DLL");
 HBITMAP(WINAPI*create)(HDC,const BITMAPINFO*,UINT,VOID**,HANDLE,DWORD)=(void*)GetProcAddress(lib,"CreateDIBSection");
 HGDIOBJ(WINAPI*select)(HDC,HGDIOBJ)=(void*)GetProcAddress(lib,"SelectObject");
 BOOL(WINAPI*blit)(HDC,int,int,int,int,HDC,int,int,DWORD)=(void*)GetProcAddress(lib,"BitBlt");
 BOOL(WINAPI*del)(HGDIOBJ)=(void*)GetProcAddress(lib,"DeleteObject");
 BOOL(WINAPI*deldc)(HDC)=(void*)GetProcAddress(lib,"DeleteDC");
 int mode,i,x,y,bytes,stride;BOOL ok;BITMAPINFO bi;BYTE*bits;HDC screen=GetDC(0),src,dst;HBITMAP soft,native;HGDIOBJ os,od;DWORD expected[12];
 if(!create||!select||!blit||!del||!deldc)ExitProcess(2);
 logh=CreateFileA("C:\\FP10\\ROWS.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 for(mode=0;mode<4;mode++){
  bytes=mode<2?3:4;stride=(3*bytes+3)&~3;
  for(i=0;i<sizeof(bi);i++)((BYTE*)&bi)[i]=0;
  bi.bmiHeader.biSize=40;bi.bmiHeader.biWidth=3;bi.bmiHeader.biHeight=(mode&1)?4:-4;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=bytes*8;
  value("Bits per pixel=",bytes*8);value("Bottom-up=",mode&1);
  src=CreateCompatibleDC(screen);dst=CreateCompatibleDC(screen);native=CreateCompatibleBitmap(screen,3,4);soft=create(screen,&bi,0,(void**)&bits,0,0);if(!native||!soft)ExitProcess(3);
  os=select(src,soft);od=select(dst,native);
  for(i=0;i<stride*4;i++)bits[i]=0;
  for(y=0;y<4;y++)for(x=0;x<3;x++){pixel(bits,stride,bytes,x,y,0xff0000);expected[y*3+x]=RGB(255,0,0);}
  check(blit(dst,0,0,3,4,src,0,0,SRCCOPY),"Initial full upload=");
  /* A single changed middle row must not change any other native row. */
  pixel(bits,stride,bytes,1,(mode&1)?2:1,0x00ff00);expected[4]=RGB(0,255,0);
  ok=blit(dst,0,0,3,4,src,0,0,SRCCOPY);for(y=0;y<4;y++)for(x=0;x<3;x++)if(GetPixel(dst,x,y)!=expected[y*3+x])ok=FALSE;
  check(ok,"Middle-row update and unchanged neighbors=");
  /* The changed span includes an unchanged row; it must remain intact. */
  pixel(bits,stride,bytes,0,(mode&1)?3:0,0x0000ff);expected[0]=RGB(0,0,255);
  pixel(bits,stride,bytes,2,(mode&1)?0:3,0xffffff);expected[11]=RGB(255,255,255);
  ok=blit(dst,0,0,3,4,src,0,0,SRCCOPY);for(y=0;y<4;y++)for(x=0;x<3;x++)if(GetPixel(dst,x,y)!=expected[y*3+x])ok=FALSE;
  check(ok,"Separated first/last-row edits=");
  ok=blit(dst,0,0,3,4,src,0,0,SRCCOPY);for(y=0;y<4;y++)for(x=0;x<3;x++)if(GetPixel(dst,x,y)!=expected[y*3+x])ok=FALSE;
  check(ok,"Repeated unchanged presentation=");
  select(src,os);select(dst,od);del(soft);DeleteObject(native);deldc(src);deldc(dst);
 }
 value("Checks=",checks);value("Failures=",failures);ReleaseDC(0,screen);CloseHandle(logh);ExitProcess(failures?1:0);
}
