/* Direct3D 9 HAL and render-target readback for Windows XP, x86 and x64.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>

typedef struct {
    float x, y, z, rhw;
    DWORD color;
    float u, v;
} VERTEX;

static FILE *logfile;
static unsigned checked, failed;

static BOOL succeeded(HRESULT hr,const char *step)
{
    if (FAILED(hr)) {
        fprintf(logfile,"%s HRESULT=0x%08lx\n",step,(unsigned long)hr);
        fflush(logfile);
        return FALSE;
    }
    return TRUE;
}

static BOOL draw(IDirect3DDevice9 *dev,const VERTEX *v,D3DPRIMITIVETYPE type,UINT count)
{
    HRESULT hr;
    if (!succeeded(IDirect3DDevice9_BeginScene(dev),"BeginScene")) return FALSE;
    hr=IDirect3DDevice9_DrawPrimitiveUP(dev,type,count,v,sizeof(*v));
    if (!succeeded(IDirect3DDevice9_EndScene(dev),"EndScene")) return FALSE;
    return succeeded(hr,"DrawPrimitiveUP");
}

static BOOL quad(IDirect3DDevice9 *dev,float left,float top,float right,float bottom,float z,DWORD color)
{
    const VERTEX v[]={
        {left,top,z,1,color,0,0},{right,top,z,1,color,1,0},
        {left,bottom,z,1,color,0,1},{right,bottom,z,1,color,1,1}
    };
    return draw(dev,v,D3DPT_TRIANGLESTRIP,2);
}

static void pixel(IDirect3DDevice9 *dev,IDirect3DSurface9 *back,IDirect3DSurface9 *copy,
                  const char *name,UINT x,UINT y,DWORD expected,int tolerance)
{
    D3DLOCKED_RECT lock;
    unsigned shift;
    DWORD rgb;
    BOOL match=TRUE;
    if (!succeeded(IDirect3DDevice9_GetRenderTargetData(dev,back,copy),"GetRenderTargetData") ||
        !succeeded(IDirect3DSurface9_LockRect(copy,&lock,NULL,D3DLOCK_READONLY),"LockRect")) {
        failed++;
        return;
    }
    rgb=((const DWORD*)((const BYTE*)lock.pBits+y*lock.Pitch))[x]&0xffffff;
    IDirect3DSurface9_UnlockRect(copy);
    for (shift=0;shift<24;shift+=8) {
        int delta=(int)((rgb>>shift)&255)-(int)((expected>>shift)&255);
        if (delta < -tolerance || delta > tolerance) match=FALSE;
    }
    fprintf(logfile,"%s at %u,%u: %06lx expected %06lx %s\n",name,x,y,rgb,expected,match?"ok":"FAILED");
    ++checked;
    if (!match) ++failed;
}

int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR args,int show)
{
    IDirect3D9 *api=NULL;
    IDirect3DDevice9 *dev=NULL;
    IDirect3DSurface9 *back=NULL,*copy=NULL;
    IDirect3DTexture9 *tex=NULL;
    D3DADAPTER_IDENTIFIER9 adapter;
    D3DPRESENT_PARAMETERS pp={0};
    D3DLOCKED_RECT lock;
    HWND window=NULL;
    char path[MAX_PATH],*name;
    DWORD size=GetModuleFileNameA(NULL,path,sizeof(path));
    const VERTEX triangle[]={
        {0,0,.5f,1,0xffff0000,0,0},{60,0,.5f,1,0xff00ff00,0,0},{0,60,.5f,1,0xff0000ff,0,0}
    };
    (void)previous; (void)args; (void)show;
    if (!size || size>=MAX_PATH-12) return 2;
    for (name=path+size;name>path && name[-1]!='\\';--name) {}
    lstrcpyA(name,"D3D9TEST.TXT");
    logfile=fopen(path,"w");
    if (!logfile) return 2;
    fprintf(logfile,"Direct3D 9 native %u-bit HAL test\n",(unsigned)(8*sizeof(void*)));
    api=Direct3DCreate9(D3D_SDK_VERSION);
    if (!api) { fputs("Direct3DCreate9 failed\n",logfile); goto done; }
    if (!succeeded(IDirect3D9_GetAdapterIdentifier(api,0,0,&adapter),"GetAdapterIdentifier")) goto done;
    fprintf(logfile,"Adapter=%s Driver=%s PCI=%04lx:%04lx\n",
            adapter.Description,adapter.Driver,adapter.VendorId,adapter.DeviceId);
    window=CreateWindowA("STATIC","WINXPDM Direct3D 9",WS_POPUP|WS_VISIBLE,0,0,64,64,NULL,NULL,instance,NULL);
    if (!window) goto done;
    pp.Windowed=TRUE;
    pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth=pp.BackBufferHeight=64;
    pp.BackBufferFormat=D3DFMT_X8R8G8B8;
    pp.BackBufferCount=1;
    pp.hDeviceWindow=window;
    pp.EnableAutoDepthStencil=TRUE;
    pp.AutoDepthStencilFormat=D3DFMT_D16;
    if (!succeeded(IDirect3D9_CreateDevice(api,0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&dev),
                   "CreateDevice HAL with software vertex processing")) goto done;
    if (!succeeded(IDirect3DDevice9_GetBackBuffer(dev,0,0,D3DBACKBUFFER_TYPE_MONO,&back),"GetBackBuffer") ||
        !succeeded(IDirect3DDevice9_CreateOffscreenPlainSurface(dev,64,64,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&copy,NULL),
                   "CreateOffscreenPlainSurface")) goto done;
    IDirect3DDevice9_SetFVF(dev,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
    IDirect3DDevice9_SetRenderState(dev,D3DRS_CULLMODE,D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(dev,D3DRS_LIGHTING,FALSE);
    IDirect3DDevice9_SetRenderState(dev,D3DRS_ZENABLE,FALSE);
    IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
    IDirect3DDevice9_Clear(dev,0,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff000000,1,0);
    if (!draw(dev,triangle,D3DPT_TRIANGLELIST,1)) goto done;
    pixel(dev,back,copy,"Gouraud",20,20,0x555555,12);
    pixel(dev,back,copy,"Background",62,62,0,0);
    IDirect3DDevice9_SetRenderState(dev,D3DRS_ZENABLE,D3DZB_TRUE);
    IDirect3DDevice9_SetRenderState(dev,D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
    if (!quad(dev,12,12,52,52,.2f,0xff00ff00) || !quad(dev,0,0,64,64,.8f,0xffff0000)) goto done;
    pixel(dev,back,copy,"Near depth",32,32,0x00ff00,1);
    pixel(dev,back,copy,"Far depth",4,4,0xff0000,1);
    IDirect3DDevice9_SetRenderState(dev,D3DRS_ZENABLE,FALSE);
    IDirect3DDevice9_Clear(dev,0,NULL,D3DCLEAR_TARGET,0xff0000ff,1,0);
    IDirect3DDevice9_SetRenderState(dev,D3DRS_ALPHABLENDENABLE,TRUE);
    IDirect3DDevice9_SetRenderState(dev,D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(dev,D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    if (!quad(dev,0,0,64,64,.5f,0x80ff0000)) goto done;
    pixel(dev,back,copy,"Alpha blend",32,32,0x80007f,2);
    IDirect3DDevice9_SetRenderState(dev,D3DRS_ALPHABLENDENABLE,FALSE);
    if (!succeeded(IDirect3DDevice9_CreateTexture(dev,2,2,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&tex,NULL),
                   "CreateTexture") || !succeeded(IDirect3DTexture9_LockRect(tex,0,&lock,NULL,0),"Texture LockRect")) goto done;
    ((DWORD*)lock.pBits)[0]=0xff00ffff;
    ((DWORD*)lock.pBits)[1]=0xffff00ff;
    ((DWORD*)((BYTE*)lock.pBits+lock.Pitch))[0]=0xffffffff;
    ((DWORD*)((BYTE*)lock.pBits+lock.Pitch))[1]=0xffffff00;
    IDirect3DTexture9_UnlockRect(tex,0);
    IDirect3DDevice9_SetTexture(dev,0,(IDirect3DBaseTexture9*)tex);
    IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
    IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_MINFILTER,D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
    if (!quad(dev,0,0,64,64,.5f,0xffffffff)) goto done;
    pixel(dev,back,copy,"Texture top left",16,16,0x00ffff,1);
    pixel(dev,back,copy,"Texture top right",48,16,0xff00ff,1);
    pixel(dev,back,copy,"Texture bottom left",16,48,0xffffff,1);
    pixel(dev,back,copy,"Texture bottom right",48,48,0xffff00,1);
    if (!succeeded(IDirect3DDevice9_Present(dev,NULL,NULL,NULL,NULL),"Present")) failed++;
done:
    fprintf(logfile,"pixel checks: %u/9 executed, %u failed\nresult: %s\n",checked,failed,
            checked==9 && !failed?"PASS":"FAIL");
    fclose(logfile);
    if (tex) IDirect3DTexture9_Release(tex);
    if (copy) IDirect3DSurface9_Release(copy);
    if (back) IDirect3DSurface9_Release(back);
    if (dev) IDirect3DDevice9_Release(dev);
    if (window) DestroyWindow(window);
    if (api) IDirect3D9_Release(api);
    return checked==9 && !failed?0:1;
}
