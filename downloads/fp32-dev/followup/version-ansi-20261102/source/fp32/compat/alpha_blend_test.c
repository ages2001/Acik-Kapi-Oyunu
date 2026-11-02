#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HBITMAP(WINAPI*CreateDib)(HDC,const BITMAPINFO*,UINT,void**,HANDLE,DWORD);
typedef HGDIOBJ(WINAPI*Select)(HDC,HGDIOBJ);
typedef BOOL(WINAPI*Delete)(HGDIOBJ);
typedef BOOL(WINAPI*DeleteDc)(HDC);
typedef BOOL(WINAPI*Blend)(HDC,int,int,int,int,HDC,int,int,int,int,BLENDFUNCTION);
static CreateDib create_dib;static Select select_object;static Delete delete_object;static DeleteDc delete_dc;static Blend blend;
static HANDLE logh;static DWORD checks,failures;
static void number(const char*s,DWORD n){char b[180];DWORD w;wsprintfA(b,"%s %lu\r\n",s,n);WriteFile(logh,b,lstrlenA(b),&w,0);FlushFileBuffers(logh);}
static void check(BOOL ok,const char*s){checks++;if(!ok)failures++;number(s,ok);}
static void zero(void*p,UINT n){BYTE*b=p;while(n--)*b++=0;}
typedef struct {HDC dc;HBITMAP bm;HGDIOBJ old;BYTE*bits;UINT stride,w,h,bpp;BOOL top;}Surface;
static BOOL surface(Surface*s,UINT w,UINT h,UINT bpp,BOOL top){BITMAPINFO bi;zero(s,sizeof(*s));zero(&bi,sizeof(bi));s->w=w;s->h=h;s->bpp=bpp;s->top=top;s->stride=((w*bpp+31)/32)*4;s->dc=CreateCompatibleDC(0);bi.bmiHeader.biSize=40;bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=top?-(LONG)h:h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=bpp;s->bm=create_dib(s->dc,&bi,DIB_RGB_COLORS,(void**)&s->bits,0,0);if(s->bm)s->old=select_object(s->dc,s->bm);return s->dc&&s->bm&&s->old&&s->old!=HGDI_ERROR;}
static void release(Surface*s){if(s->old&&s->old!=HGDI_ERROR)select_object(s->dc,s->old);if(s->bm)delete_object(s->bm);if(s->dc)delete_dc(s->dc);}
static void pixel(Surface*s,UINT x,UINT y,DWORD v){BYTE*p=s->bits+(s->top?y:s->h-1-y)*s->stride+x*(s->bpp/8);p[0]=v;p[1]=v>>8;p[2]=v>>16;if(s->bpp==32)p[3]=v>>24;}
static DWORD read_pixel(Surface*s,UINT x,UINT y){BYTE*p=s->bits+(s->top?y:s->h-1-y)*s->stride+x*(s->bpp/8);return p[0]|((DWORD)p[1]<<8)|((DWORD)p[2]<<16)|(s->bpp==32?(DWORD)p[3]<<24:0xff000000);}
static void fill(Surface*s,DWORD v){UINT x,y;for(y=0;y<s->h;y++)for(x=0;x<s->w;x++)pixel(s,x,y,v);}
static BOOL scaled(Surface*d,const DWORD*expected,BOOL edges){UINT x,y;for(y=0;y<4;y++)for(x=0;x<4;x++)if(read_pixel(d,x,y)!=(edges&&x>0&&x<3?0xff000080:expected[(y/2)*2+x/2]))return FALSE;return TRUE;}
void WINAPI entry(void){HMODULE dll;Surface src,dst,temp;BLENDFUNCTION f={AC_SRC_OVER,0,255,AC_SRC_ALPHA};UINT i,j,k;BOOL ok;HRGN r1,r2;HDC screen,dc;HBITMAP bm;HGDIOBJ old;HBRUSH brush;RECT rect={0,0,4,4};DWORD start,elapsed;
 static const DWORD expected[]={0xff800040,0xff00ff00,0xff000080,0xff0000ff};
 logh=CreateFileA("C:\\FP32\\ALPHA.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 dll=LoadLibraryA("C:\\FP32\\B32G.DLL");check(dll!=0,"Production GDI proxy loads");if(!dll)goto done;
 create_dib=(void*)GetProcAddress(dll,"CreateDIBSection");select_object=(void*)GetProcAddress(dll,"SelectObject");delete_object=(void*)GetProcAddress(dll,"DeleteObject");delete_dc=(void*)GetProcAddress(dll,"DeleteDC");blend=(void*)GetProcAddress(dll,"GdiAlphaBlend");check(create_dib&&select_object&&delete_object&&delete_dc&&blend,"All tested exports resolve");if(!create_dib||!select_object||!delete_object||!delete_dc||!blend)goto done;
 for(i=0;i<2;i++)for(j=0;j<2;j++)for(k=0;k<2;k++){
  ok=surface(&src,2,2,32,i)&&surface(&dst,4,4,k?32:24,j);check(ok,"Top/bottom-up source and 24/32-bit destination created");if(!ok)goto done;
  pixel(&src,0,0,0x80800000);pixel(&src,1,0,0xff00ff00);pixel(&src,0,1,0);pixel(&src,1,1,0xff0000ff);fill(&dst,0xff000080);
  f.SourceConstantAlpha=255;f.AlphaFormat=AC_SRC_ALPHA;
  check(blend(dst.dc,0,0,4,4,src.dc,0,0,2,2,f)&&scaled(&dst,expected,FALSE),"Premultiplied pixels and nearest-neighbor scaling");
  fill(&dst,0xff000080);r1=CreateRectRgn(0,0,1,4);r2=CreateRectRgn(3,0,4,4);CombineRgn(r1,r1,r2,RGN_OR);SelectClipRgn(dst.dc,r1);
  check(blend(dst.dc,0,0,4,4,src.dc,0,0,2,2,f)&&scaled(&dst,expected,TRUE),"Complex destination clip preserves holes");SelectClipRgn(dst.dc,0);DeleteObject(r1);DeleteObject(r2);
  fill(&dst,0xff000080);f.SourceConstantAlpha=128;
  check(blend(dst.dc,0,0,2,2,src.dc,0,0,2,2,f)&&read_pixel(&dst,0,0)==0xff400060&&read_pixel(&dst,1,0)==0xff008040&&read_pixel(&dst,1,1)==0xff0000c0,"Per-pixel and constant alpha combine");
  fill(&dst,0xff000080);f.AlphaFormat=0;
  check(blend(dst.dc,0,0,2,2,src.dc,0,0,2,2,f)&&read_pixel(&dst,0,0)==0xff400040,"Constant-only alpha ignores source alpha byte");
  fill(&dst,0xff000080);f.SourceConstantAlpha=0;f.AlphaFormat=AC_SRC_ALPHA;
  check(blend(dst.dc,0,0,4,4,src.dc,0,0,2,2,f)&&read_pixel(&dst,0,0)==0xff000080&&read_pixel(&dst,3,3)==0xff000080,"Zero constant alpha leaves destination intact");
  fill(&dst,0xff000080);f.SourceConstantAlpha=255;SetViewportOrgEx(dst.dc,1,1,0);SetViewportOrgEx(src.dc,1,0,0);
  check(blend(dst.dc,0,0,2,2,src.dc,-1,0,2,2,f)&&read_pixel(&dst,1,1)==expected[0]&&read_pixel(&dst,2,2)==expected[3]&&read_pixel(&dst,0,0)==0xff000080,"Source and destination viewport origins honored");SetViewportOrgEx(dst.dc,0,0,0);SetViewportOrgEx(src.dc,0,0,0);
  fill(&dst,0xff000080);check(blend(dst.dc,-2,-2,4,4,src.dc,0,0,2,2,f)&&read_pixel(&dst,0,0)==expected[3]&&read_pixel(&dst,2,2)==0xff000080,"Off-surface destination clips without shifting sample");
  release(&src);release(&dst);
 }
 check(surface(&src,2,2,32,TRUE)&&surface(&dst,4,4,32,TRUE),"Validation surfaces created");fill(&src,0x80008000);fill(&dst,0x80402010);f.SourceConstantAlpha=255;f.AlphaFormat=AC_SRC_ALPHA;
 check(blend(dst.dc,0,0,2,2,src.dc,0,0,2,2,f)&&read_pixel(&dst,0,0)==0xc0209008,"Destination alpha uses source-over composition");
 /* Compare meta/application clipping against the native visibility oracle. */
 {BOOL visible[16],match=TRUE;int saved=SaveDC(dst.dc);HRGN left=CreateRectRgn(0,0,1,4),right=CreateRectRgn(3,0,4,4),top=CreateRectRgn(0,0,4,2);
  CombineRgn(left,left,right,RGN_OR);SelectClipRgn(dst.dc,left);SetMetaRgn(dst.dc);SelectClipRgn(dst.dc,top);SetViewportOrgEx(dst.dc,1,1,0);fill(&dst,0xff000080);
  for(i=0;i<16;i++)visible[i]=PtVisible(dst.dc,i%4-1,i/4-1);
  ok=blend(dst.dc,-1,-1,4,4,src.dc,0,0,2,2,f);
  for(i=0;i<16;i++)if(read_pixel(&dst,i%4,i/4)!=(visible[i]?0xff008040:0xff000080))match=FALSE;
  check(ok&&match,"Meta and application clips match native visibility with viewport offset");
  RestoreDC(dst.dc,saved);DeleteObject(left);DeleteObject(right);DeleteObject(top);
 }
 {int saved=SaveDC(dst.dc);BOOL match=TRUE;fill(&dst,0xff000080);SetMapMode(dst.dc,MM_ANISOTROPIC);SetWindowExtEx(dst.dc,2,2,0);SetViewportExtEx(dst.dc,4,4,0);
  ok=blend(dst.dc,0,0,2,2,src.dc,0,0,2,2,f);for(i=0;i<16;i++)if(read_pixel(&dst,i%4,i/4)!=0xff008040)match=FALSE;
  check(ok&&match,"Axis-aligned destination mapping scales to device pixels");RestoreDC(dst.dc,saved);
 }
 {HRGN empty=CreateRectRgn(0,0,0,0);fill(&dst,0xff000080);SelectClipRgn(dst.dc,empty);
  {RECT empty_box;number("Native empty clip kind",GetClipBox(dst.dc,&empty_box));number("Native empty clip width",empty_box.right-empty_box.left);number("Native empty clip height",empty_box.bottom-empty_box.top);
   ok=blend(dst.dc,0,0,4,4,src.dc,0,0,2,2,f);if(!ok)number("Empty clip blend error",GetLastError());
   check(ok&&read_pixel(&dst,0,0)==0xff000080,"Empty clip succeeds without changing pixels");}
  SelectClipRgn(dst.dc,0);DeleteObject(empty);
 }
 f.BlendOp=1;check(!blend(dst.dc,0,0,2,2,src.dc,0,0,2,2,f),"Unsupported blend operation rejected");f.BlendOp=0;
 f.BlendFlags=1;check(!blend(dst.dc,0,0,2,2,src.dc,0,0,2,2,f),"Reserved blend flags rejected");f.BlendFlags=0;
 f.AlphaFormat=2;check(!blend(dst.dc,0,0,2,2,src.dc,0,0,2,2,f),"Invalid alpha format rejected");f.AlphaFormat=AC_SRC_ALPHA;
 check(!blend(dst.dc,0,0,-2,2,src.dc,0,0,2,2,f),"Negative destination extent rejected");
 check(!blend(dst.dc,0,0,2,2,src.dc,0,0,0,2,f),"Zero source extent rejected");
 check(!blend(dst.dc,0,0,2,2,src.dc,1,0,2,2,f),"Out-of-bitmap source rejected");
 check(!blend(src.dc,0,0,2,2,src.dc,0,0,2,2,f),"Overlapping same-surface operation rejected");
 check(!blend(dst.dc,0x7fffffff,0,2,2,src.dc,0,0,2,2,f),"Coordinate overflow rejected");
 check(!blend(0,0,0,2,2,src.dc,0,0,2,2,f),"Invalid destination DC rejected");
 /* A native 32-bit bitmap retains alpha through GetBitmapBits; the old
    VGA GetDIBits path cannot provide this source format. */
 {DWORD native_pixels[4]={0x80800000,0xff00ff00,0,0xff0000ff};HDC ndc=CreateCompatibleDC(0);HBITMAP nb=CreateBitmap(2,2,1,32,native_pixels);HGDIOBJ no=SelectObject(ndc,nb);
  if(nb&&no&&no!=HGDI_ERROR){
   number("Native per-pixel DDB path remains unsupported",1);
   SetLastError(0);check(!blend(dst.dc,0,0,4,4,ndc,0,0,2,2,f)&&GetLastError()==ERROR_NOT_SUPPORTED,"Unsupported native per-pixel DDB fails explicitly");
   SelectObject(ndc,no);
  }else number("Native 32-bit DDB blend cases unavailable on this VGA driver",2);
  if(nb)DeleteObject(nb);DeleteDC(ndc);
 }
 /* Native VGA DDB presentation and native constant-alpha source capture. */
 screen=GetDC(0);dc=CreateCompatibleDC(screen);bm=CreateCompatibleBitmap(screen,4,4);ReleaseDC(0,screen);old=SelectObject(dc,bm);fill(&src,0xff00ff00);
 check(dc&&bm&&blend(dc,0,0,4,4,src.dc,0,0,2,2,f)&&GetPixel(dc,0,0)==RGB(0,255,0),"CPU alpha presents to native VGA bitmap");
 f.AlphaFormat=0;fill(&dst,0xff000000);check(blend(dst.dc,0,0,4,4,dc,0,0,4,4,f)&&read_pixel(&dst,0,0)==0xff00ff00,"Native VGA bitmap supplies constant-alpha source");
 brush=CreateSolidBrush(RGB(255,255,255));FillRect(dc,&rect,brush);DeleteObject(brush);r1=CreateRectRgn(1,1,3,3);SelectClipRgn(dc,r1);f.AlphaFormat=AC_SRC_ALPHA;
 check(blend(dc,0,0,4,4,src.dc,0,0,2,2,f)&&GetPixel(dc,1,1)==RGB(0,255,0),"Native presentation honors clip");SelectClipRgn(dc,0);check(GetPixel(dc,0,0)==RGB(255,255,255),"Native presentation preserves pixels outside clip");DeleteObject(r1);
 SelectObject(dc,old);DeleteObject(bm);DeleteDC(dc);release(&src);release(&dst);
 check(surface(&src,640,480,32,TRUE)&&surface(&dst,640,480,32,TRUE),"Large render surfaces created");fill(&src,0x80800000);fill(&dst,0xff000080);f.SourceConstantAlpha=255;f.AlphaFormat=AC_SRC_ALPHA;
 start=GetTickCount();ok=TRUE;for(i=0;i<8;i++)if(!blend(dst.dc,0,0,640,480,src.dc,0,0,640,480,f))ok=FALSE;elapsed=GetTickCount()-start;
 check(ok,"Eight full-frame software blends complete");number("Eight 640x480 blends milliseconds",elapsed);release(&src);release(&dst);
 done:number("Checks",checks);number("Failures",failures);if(dll)FreeLibrary(dll);CloseHandle(logh);ExitProcess(failures?2:0);
}
