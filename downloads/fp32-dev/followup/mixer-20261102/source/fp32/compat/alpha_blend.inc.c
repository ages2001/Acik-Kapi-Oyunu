/* Included after the existing DIB bridge. Alpha values are premultiplied,
   as required by GdiAlphaBlend. No SSE instructions are used. */
static BOOL alpha_bad(DWORD error){SetLastError(error);return FALSE;}
static BOOL alpha_rect(HDC dc,int x,int y,int w,int h,RECT *r)
{
    POINT p[3];LONGLONG right=(LONGLONG)x+w,bottom=(LONGLONG)y+h;
    if(w<=0||h<=0||right>0x7fffffffLL||right<(-2147483647LL-1)||bottom>0x7fffffffLL||bottom<(-2147483647LL-1))return alpha_bad(ERROR_INVALID_PARAMETER);
    p[0].x=x;p[0].y=y;p[1].x=(LONG)right;p[1].y=y;p[2].x=x;p[2].y=(LONG)bottom;
    if(!LPtoDP(dc,p,3))return FALSE;
    /* Mirroring, rotation and shear are not supported by AlphaBlend. */
    if(p[1].y!=p[0].y||p[2].x!=p[0].x||p[1].x<=p[0].x||p[2].y<=p[0].y||
       (LONGLONG)p[1].x-p[0].x>0x7fffffff||(LONGLONG)p[2].y-p[0].y>0x7fffffff)return alpha_bad(ERROR_INVALID_PARAMETER);
    r->left=p[0].x;r->top=p[0].y;r->right=p[1].x;r->bottom=p[2].y;return TRUE;
}
static BOOL alpha_identity(HDC dc)
{
    return SetGraphicsMode(dc,GM_ADVANCED)&&ModifyWorldTransform(dc,0,MWT_IDENTITY)&&
           SetMapMode(dc,MM_TEXT)&&SetWindowOrgEx(dc,0,0,0)&&SetViewportOrgEx(dc,0,0,0);
}
static void alpha_info(BITMAPINFO *bi,int w,int h)
{
    UINT i;for(i=0;i<sizeof(*bi);i++)((BYTE*)bi)[i]=0;
    bi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi->bmiHeader.biWidth=w;bi->bmiHeader.biHeight=h;
    bi->bmiHeader.biPlanes=1;bi->bmiHeader.biBitCount=32;bi->bmiHeader.biCompression=BI_RGB;
}
/* Capture a native DDB/display region in device coordinates, keeping caller
   transforms and clipping intact. The temporary bitmap is deselected before
   GetDIBits, as required on NT 3.1. */
static DWORD *alpha_capture(HDC dc,const RECT *r)
{
    int w=r->right-r->left,h=r->bottom-r->top,saved=0;DWORD *pixels=0;
    HDC memory=0;HBITMAP bitmap=0;HGDIOBJ old=0;BITMAPINFO bi;BOOL ok=FALSE;DWORD error;
    if((ULONGLONG)w*h>0x1fffffff){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return 0;}
    pixels=HeapAlloc(GetProcessHeap(),0,(SIZE_T)w*h*4);if(!pixels)return 0;
    saved=SaveDC(dc);if(!saved||!alpha_identity(dc))goto done;
    memory=CreateCompatibleDC(dc);if(!memory)goto done;
    bitmap=CreateCompatibleBitmap(dc,w,h);if(!bitmap)goto done;
    old=SelectObject(memory,bitmap);if(!old||old==HGDI_ERROR)goto done;
    ok=BitBlt(memory,0,0,w,h,dc,r->left,r->top,SRCCOPY);
    SelectObject(memory,old);old=0;if(!ok)goto done;
    alpha_info(&bi,w,h);ok=GetDIBits(memory,bitmap,0,h,pixels,&bi,DIB_RGB_COLORS)==h;
 done:error=GetLastError();if(old&&old!=HGDI_ERROR)SelectObject(memory,old);if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);if(saved)RestoreDC(dc,saved);
    if(!ok){HeapFree(GetProcessHeap(),0,pixels);pixels=0;SetLastError(error);}return pixels;
}
static DWORD alpha_combine(DWORD s,DWORD d,BLENDFUNCTION blend)
{
    UINT sa=blend.AlphaFormat?(s>>24):255,ca=blend.SourceConstantAlpha,inv,c,result=0,part;
    sa=(sa*ca+127)/255;inv=255-sa;
    for(c=0;c<3;c++){
        part=(((s>>(8*c))&255)*ca+127)/255+(((d>>(8*c))&255)*inv+127)/255;
        if(part>255)part=255;result|=part<<(8*c);
    }
    part=sa+((d>>24)*inv+127)/255;if(part>255)part=255;
    return result|(part<<24);
}
BOOL WINAPI fp_GdiAlphaBlend(HDC dst,int x,int y,int w,int h,HDC src,int sx,int sy,int sw,int sh,BLENDFUNCTION blend)
{
    Dib *a,*b;RECT source,dest,clip,area,bounds;BITMAP native_bitmap;HGDIOBJ source_bitmap,dest_bitmap;
    DWORD *sp=0,*dp=0,s,d;RGNDATA *region=0;int mode=-1,kind,xx,yy,px,py,sourcew,sourceh,destw,desth,aw,ah,saved=0;
    BOOL ok=FALSE;DWORD error=ERROR_INVALID_PARAMETER;BITMAPINFO bi;
    if(blend.BlendOp!=AC_SRC_OVER||blend.BlendFlags||blend.AlphaFormat&~AC_SRC_ALPHA)return alpha_bad(ERROR_INVALID_PARAMETER);
    if(!alpha_rect(src,sx,sy,sw,sh,&source)||!alpha_rect(dst,x,y,w,h,&dest))return FALSE;
    sourcew=source.right-source.left;sourceh=source.bottom-source.top;destw=dest.right-dest.left;desth=dest.bottom-dest.top;
    EnterCriticalSection(&guard);a=current(src);b=current(dst);
    source_bitmap=GetCurrentObject(src,OBJ_BITMAP);dest_bitmap=GetCurrentObject(dst,OBJ_BITMAP);
    if(GetObjectType(src)==OBJ_MEMDC){
        if(a){bounds.right=a->info->bmiHeader.biWidth;bounds.bottom=a->height;}
        else{if(!GetObjectW(source_bitmap,sizeof(native_bitmap),&native_bitmap))goto done;bounds.right=native_bitmap.bmWidth;bounds.bottom=native_bitmap.bmHeight;}
        if(source.left<0||source.top<0||source.right>bounds.right||source.bottom>bounds.bottom)goto done;
    }
    if((src==dst||(source_bitmap&&source_bitmap==dest_bitmap))&&IntersectRect(&area,&source,&dest))goto done;
    if(blend.AlphaFormat&&(!rgb_surface(a)||a->info->bmiHeader.biBitCount!=32)){
        /* The classic VGA driver rejects selecting a native 32-bit DDB.
           Preserve alpha through our CPU-backed DIBs; other native per-pixel
           formats need independent support and tests on their display driver. */
        error=ERROR_NOT_SUPPORTED;goto done;
    }
    kind=GetClipBox(dst,&clip);if(kind==ERROR){error=GetLastError();goto done;}
    /* NT 3.1 may report COMPLEXREGION with an empty bounding box. */
    if(kind==NULLREGION||clip.right<=clip.left||clip.bottom<=clip.top){ok=TRUE;goto done;}
    if(!alpha_rect(dst,clip.left,clip.top,clip.right-clip.left,clip.bottom-clip.top,&area))goto done;
    if(!IntersectRect(&clip,&area,&dest)){ok=TRUE;goto done;}
    if(b){bounds.left=bounds.top=0;bounds.right=b->info->bmiHeader.biWidth;bounds.bottom=b->height;if(!IntersectRect(&clip,&clip,&bounds)){ok=TRUE;goto done;}}
    if(!blend.SourceConstantAlpha){ok=TRUE;goto done;}
    aw=clip.right-clip.left;ah=clip.bottom-clip.top;
    if(!rgb_surface(a)){if(a&&!transfer(a,1))goto done;sp=alpha_capture(src,&source);if(!sp){error=GetLastError();goto done;}}
    if(!rgb_surface(b)){if(b&&!transfer(b,1))goto done;dp=alpha_capture(dst,&clip);if(!dp){error=GetLastError();goto done;}}
    if(rgb_surface(b)&&kind==COMPLEXREGION)region=clip_snapshot(dst,&mode);
    for(yy=clip.top;yy<clip.bottom;yy++){
        py=(int)ratio(yy-dest.top,sourceh,desth);
        for(xx=clip.left;xx<clip.right;xx++){
            if(rgb_surface(b)&&kind==COMPLEXREGION){
                POINT logical;logical.x=xx;logical.y=yy;
                if(mode<0&&!DPtoLP(dst,&logical,1)){error=GetLastError();goto done;}
                if(!clip_contains(dst,mode,region,xx,yy,logical.x,logical.y))continue;
            }
            px=(int)ratio(xx-dest.left,sourcew,destw);
            s=sp?sp[(sourceh-1-py)*sourcew+px]:pixel_read(a,source.left+px,source.top+py);
            d=dp?dp[(ah-1-(yy-clip.top))*aw+xx-clip.left]:pixel_read(b,xx,yy);
            d=alpha_combine(s,d,blend);
            if(dp)dp[(ah-1-(yy-clip.top))*aw+xx-clip.left]=d;else pixel_write(b,xx,yy,d);
        }
    }
    if(dp){
        saved=SaveDC(dst);if(!saved||!alpha_identity(dst)){error=GetLastError();goto done;}
        alpha_info(&bi,aw,ah);
        kind=StretchDIBits(dst,clip.left,clip.top,aw,ah,0,0,aw,ah,dp,&bi,DIB_RGB_COLORS,SRCCOPY);
        if(!kind||kind==GDI_ERROR){error=GetLastError();goto done;}
        RestoreDC(dst,saved);saved=0;if(b&&!transfer(b,0)){error=GetLastError();goto done;}
    }
    ok=TRUE;
 done:
    if(saved)RestoreDC(dst,saved);if(region)HeapFree(GetProcessHeap(),0,region);if(sp)HeapFree(GetProcessHeap(),0,sp);if(dp)HeapFree(GetProcessHeap(),0,dp);
    LeaveCriticalSection(&guard);if(!ok)SetLastError(error);return ok;
}
