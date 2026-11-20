/* Exercise the public driver entry from a native Win16 task.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <windows.h>
#include <conio.h>
#include "scene.h"
#include "engines.h"
typedef int (FAR PASCAL *Draw)(const WxpDraw FAR *, WxpFrame FAR *);
static WxpFrame frame;
static void report(const char *s)
{
    HFILE f = _lopen("C:\\WXPGPU.TXT", OF_WRITE);
    if (f == HFILE_ERROR) f = _lcreat("C:\\WXPGPU.TXT", 0);
    if (f != HFILE_ERROR) { _llseek(f, 0, 2); _lwrite(f, s, lstrlen(s)); _lclose(f); }
    while (*s) outp(0xe9, *s++);
}

int PASCAL WinMain(HINSTANCE self, HINSTANCE previous, LPSTR args, int show)
{
    HINSTANCE driver;
    Draw draw;
    char msg[200];
    int rc;
    WORD version = LOWORD(GetVersion());
    (void)self; (void)previous; (void)args; (void)show;
    wsprintf(msg,"WINXPDM guest %u.%u enhanced=%u\r\n", LOBYTE(version), HIBYTE(version),
             (unsigned)!!(GetWinFlags() & WF_ENHANCED));
    report(msg);
    if ((UINT)(driver = LoadLibrary("WXPGPU.DRV")) < 32) { report("driver load failed\r\n"); return 1; }
    draw = (Draw)GetProcAddress(driver, "WXPDRAWTRIANGLES");
    if (!draw) { report("driver entry missing\r\n"); FreeLibrary(driver); return 1; }
    check_engines(report);
    rc = draw(&scene, &frame);
    wsprintf(msg,"draw=%u request=%04x response=%04x commands=%lu fence=%lu outputs=%lu enabled=%lx edid=%lx scanouts=%lx center=%08lx\r\n",
             rc,(unsigned)frame.request,(unsigned)frame.response,frame.completed,frame.fence,frame.outputs,
             frame.enabled_outputs,frame.edid_outputs,frame.tested_outputs,frame.pixels[16*WXP_SIDE+16]);
    report(msg);
    rc = !rc && check_scene(&frame) ? 0 : 1;
    report(rc ? "pixel check FAIL\r\n" : "interpolation and depth PASS 1024 pixels\r\n");
    if (!rc) {
        rc = draw(&textured,&frame) || !check_texture(&frame);
        report(rc ? "texture FAIL\r\n" : "texture PASS 1024 pixels\r\n");
    }
    FreeLibrary(driver);
    return rc;
}
