#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE logh;static int fails,total;
static void say(const char*s){DWORD n=0,w;while(s[n])n++;WriteFile(logh,s,n,&w,0);}
static void check(int ok,const char*s){total++;if(!ok)fails++;say(ok?"PASS ":"FAIL ");say(s);say("\r\n");}
#define LOAD(ret,name,args) typedef ret(WINAPI*T_##name)args;T_##name name=(T_##name)GetProcAddress(m,#name);if(!name){say("missing " #name);ExitProcess(2);}
void WINAPI entry(void){HMODULE m=LoadLibraryA("C:\\FP32\\B32G.DLL");BITMAPINFO info;DWORD *a=0,*b=0;HBITMAP ba,bb;HDC ca,cb;HGDIOBJ olda,oldb;DIBSECTION ds;int i;char summary[80];const char*s;int k=0,n;char digits[12];
 LOAD(HBITMAP,CreateDIBSection,(HDC,const BITMAPINFO*,UINT,VOID**,HANDLE,DWORD));LOAD(HGDIOBJ,SelectObject,(HDC,HGDIOBJ));LOAD(BOOL,BitBlt,(HDC,int,int,int,int,HDC,int,int,DWORD));LOAD(BOOL,StretchBlt,(HDC,int,int,int,int,HDC,int,int,int,int,DWORD));LOAD(BOOL,DeleteObject,(HGDIOBJ));LOAD(BOOL,DeleteDC,(HDC));LOAD(int,GetObjectW,(HANDLE,int,LPVOID));LOAD(COLORREF,SetPixel,(HDC,int,int,COLORREF));LOAD(BOOL,GdiFlush,(void));LOAD(BOOL,LineTo,(HDC,int,int));
 logh=CreateFileA("C:\\FP32\\GDIREGR.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
 for(i=0;i<sizeof(info);i++)((BYTE*)&info)[i]=0;info.bmiHeader.biSize=40;info.bmiHeader.biWidth=4;info.bmiHeader.biHeight=-4;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 ba=CreateDIBSection(0,&info,DIB_RGB_COLORS,(void**)&a,0,0);bb=CreateDIBSection(0,&info,DIB_RGB_COLORS,(void**)&b,0,0);check(ba&&bb&&a&&b,"allocate two top-down 32-bit sections");if(!ba||!bb||!a||!b)goto done;
 check(GetObjectW(ba,sizeof(ds),&ds)==sizeof(ds)&&ds.dsBm.bmBits==a&&ds.dsBm.bmBitsPixel==32&&ds.dsBmih.biHeight==-4,"DIBSECTION metadata");
 for(i=0;i<16;i++){a[i]=i<4?0x00123456:0x0000ff00;b[i]=0;}
 ca=CreateCompatibleDC(0);cb=CreateCompatibleDC(0);olda=SelectObject(ca,ba);oldb=SelectObject(cb,bb);check(olda&&oldb,"select software sections into DCs");
 check(BitBlt(cb,0,0,4,4,ca,0,0,SRCCOPY),"copy DIB pixels through native GDI");check((b[0]&0xffffff)==0x123456&&(b[12]&0xffffff)==0x00ff00,"pixel values and top-down orientation preserved");
 check(SetPixel(cb,2,2,RGB(255,0,0))!=CLR_INVALID,"native GDI pixel operation");check((b[10]&0xffffff)==0xff0000,"GDI writes visible in CPU memory");
 for(i=0;i<16;i++)a[i]=0x000000ff;check(GdiFlush(),"flush software pixels");check(StretchBlt(cb,0,0,4,4,ca,0,0,2,2,SRCCOPY),"stretch bitmap through GDI");check((b[0]&0xffffff)==0x0000ff&&(b[15]&0xffffff)==0x0000ff,"stretched pixels visible in CPU memory");
 /* Real native transfers: the earlier CPU-only checks missed scanline reversal. */
 {HDC screen=GetDC(0),native_dc=CreateCompatibleDC(0);HBITMAP native=CreateCompatibleBitmap(screen,4,4);HGDIOBJ selected=SelectObject(native_dc,native),pen,old_pen;
 ReleaseDC(0,screen);
 for(i=0;i<16;i++)a[i]=i<4?0xff0000:(i>=12?0x0000ff:0x00ff00);
 check(BitBlt(native_dc,0,0,4,4,ca,0,0,SRCCOPY),"upload asymmetric top-down RGB bitmap");
 check(GetPixel(native_dc,0,0)==RGB(255,0,0)&&GetPixel(native_dc,0,3)==RGB(0,0,255),"native presentation preserves top and bottom rows");
 check(BitBlt(native_dc,0,0,4,4,ca,0,0,SRCCOPY)&&GetPixel(native_dc,0,0)==RGB(255,0,0),"repeated unchanged bitmap stays correct");
 a[0]=0x0000ff;
 check(BitBlt(native_dc,0,0,4,4,ca,0,0,SRCCOPY)&&GetPixel(native_dc,0,0)==RGB(0,0,255),"direct pixel edit after cached upload is presented");
 pen=CreatePen(PS_SOLID,1,RGB(255,255,255));old_pen=SelectObject(ca,pen);MoveToEx(ca,0,1,0);
 check(LineTo(ca,4,1),"native line drawing into emulated DIB");
 check((a[4]&0xffffff)==0xffffff&&(a[12]&0xffffff)==0x0000ff,"download preserves native drawing and row direction");
 check(GetObjectW(ba,sizeof(ds),&ds)==sizeof(ds)&&ds.dsBmih.biHeight==-4,"native transfer does not mutate DIB orientation metadata");
 check(BitBlt(native_dc,0,0,4,4,ca,0,0,SRCCOPY)&&GetPixel(native_dc,0,1)==RGB(255,255,255),"native drawing survives the next cached presentation");
 SelectObject(ca,old_pen);DeleteObject(pen);SelectObject(native_dc,selected);DeleteObject(native);DeleteDC(native_dc);}
 {HRGN left=CreateRectRgn(0,0,1,4),right=CreateRectRgn(3,0,4,4),both=CreateRectRgn(0,0,0,0);
 for(i=0;i<16;i++){a[i]=0xff0000;b[i]=0;}
 CombineRgn(both,left,right,RGN_OR);SelectClipRgn(cb,both);
 check(BitBlt(cb,0,0,4,4,ca,0,0,SRCCOPY),"copy through disjoint clipping rectangles");
 check((b[0]&0xffffff)==0xff0000&&(b[3]&0xffffff)==0xff0000&&b[1]==0&&b[2]==0,"pixels outside a complex clip remain untouched");
 SelectClipRgn(cb,0);DeleteObject(left);DeleteObject(right);DeleteObject(both);}
 {int saved=SaveDC(cb),expected[16],ok;HRGN left=CreateRectRgn(0,0,1,4),right=CreateRectRgn(3,0,4,4),both=CreateRectRgn(0,0,0,0),top=CreateRectRgn(0,0,4,2);POINT prior;
 CombineRgn(both,left,right,RGN_OR);SelectClipRgn(cb,both);SetMetaRgn(cb);SelectClipRgn(cb,top);SetViewportOrgEx(cb,1,1,&prior);
 for(i=0;i<16;i++){a[i]=0x12345678;b[i]=0;expected[i]=PtVisible(cb,i%4-1,i/4-1);}
 check(BitBlt(cb,-1,-1,4,4,ca,0,0,SRCCOPY),"copy with viewport offset and intersected meta clipping");
 ok=1;for(i=0;i<16;i++)if(b[i]!=(expected[i]?0x12345678:0))ok=0;
 check(ok,"cached clipping matches native PtVisible with meta region and viewport");
 RestoreDC(cb,saved);DeleteObject(left);DeleteObject(right);DeleteObject(both);DeleteObject(top);}
 {HRGN empty=CreateRectRgn(0,0,0,0);int ok=1;
 for(i=0;i<16;i++){a[i]=0xff0000;b[i]=0x87654321;}
 SelectClipRgn(cb,empty);check(BitBlt(cb,0,0,4,4,ca,0,0,SRCCOPY),"copy with empty clipping region succeeds");
 for(i=0;i<16;i++)if(b[i]!=0x87654321)ok=0;check(ok,"empty clipping leaves every pixel untouched");SelectClipRgn(cb,0);DeleteObject(empty);}
 SelectObject(ca,olda);SelectObject(cb,oldb);DeleteDC(ca);DeleteDC(cb);check(DeleteObject(ba)&&DeleteObject(bb),"release DIB resources");
 {struct {BITMAPINFOHEADER h;RGBQUAD colors[16];} pal;BYTE *pixels;HDC mem=CreateCompatibleDC(0),target=CreateCompatibleDC(0),screen=GetDC(0);HBITMAP native=CreateCompatibleBitmap(screen,4,4),soft;HGDIOBJ prev,pt;int j;ReleaseDC(0,screen);for(j=0;j<sizeof(pal);j++)((BYTE*)&pal)[j]=0;pal.h.biSize=40;pal.h.biWidth=4;pal.h.biHeight=-4;pal.h.biPlanes=1;pal.h.biBitCount=4;pal.colors[9].rgbRed=255;soft=CreateDIBSection(mem,(BITMAPINFO*)&pal,DIB_RGB_COLORS,(void**)&pixels,0,0);check(soft&&pixels,"DIB created using default monochrome memory DC");if(soft&&pixels){for(j=0;j<16;j++)pixels[j]=0x99;prev=SelectObject(mem,soft);pt=SelectObject(target,native);check(BitBlt(target,0,0,4,4,mem,0,0,SRCCOPY),"indexed DIB copied to native color bitmap");check(GetPixel(target,0,0)==RGB(255,0,0),"native bitmap receives red instead of monochrome black");SelectObject(mem,prev);SelectObject(target,pt);DeleteObject(soft);}DeleteObject(native);DeleteDC(mem);DeleteDC(target);}
 done:s="Checks: ";while(*s)summary[k++]=*s++;n=total;i=0;do{digits[i++]='0'+n%10;n/=10;}while(n);while(i)summary[k++]=digits[--i];s="; failures: ";while(*s)summary[k++]=*s++;n=fails;i=0;do{digits[i++]='0'+n%10;n/=10;}while(n);while(i)summary[k++]=digits[--i];summary[k]=0;say(summary);CloseHandle(logh);ExitProcess(fails?1:0);
}
