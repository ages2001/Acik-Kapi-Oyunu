/* Keep two locks across memory pressure. Check realloc, shared and fixed
   objects, preservation of every byte and the zero-filled extension. */
#include <windows.h>
#include <dos.h>
#include <conio.h>
#define SMALL 12000u
#define GROWN 20000u
static HGLOBAL others[192];
static void text(const char *p) {
    while (*p)
        outp(0xe9, *p++);
}
static int check(int condition, const char *operation) {
    if (!condition) {
        text("Failed ");
        text(operation);
        text("\n");
    }
    return condition;
}
static int run(void) {
    HGLOBAL a, fixed, shared, grown;
    BYTE FAR *p, FAR *q, FAR *f, FAR *s;
    unsigned i, n = 0;
    int ok = 1;
    a = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, SMALL);
    fixed = GlobalAlloc(GMEM_FIXED, 512);
    shared = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, 512);
    if (!check(a && fixed && shared, "initial allocation"))
        return 0;
    p = GlobalLock(a);
    q = GlobalLock(a);
    f = GlobalLock(fixed);
    s = GlobalLock(shared);
    if (!check(p && p == q && f && s, "nested lock"))
        return 0;
    for (i = 0; i < SMALL; ++i) {
        if (p[i] != 0)
            ok = 0;
        p[i] = (BYTE)(i * 31u + 17u);
    }
    f[0] = 0x35;
    s[0] = 0x53;
    GlobalUnlock(a);
    for (n = 0; n < 192; ++n) {
        others[n] = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, 32768UL);
        if (!others[n])
            break;
        q = GlobalLock(others[n]);
        if (!q) {
            ok = 0;
            ++n;
            break;
        }
        q[n] = (BYTE)n;
        GlobalUnlock(others[n]);
    }
    ok &= check(n >= 32, "pressure allocation");
    q = GlobalLock(a);
    ok &= check(q == p && f[0] == 0x35 && s[0] == 0x53, "resident pointers");
    if (q)
        GlobalUnlock(a);
    for (i = 0; i < SMALL; ++i)
        if (p[i] != (BYTE)(i * 31u + 17u))
            ok = 0;
    GlobalUnlock(a);
    for (i = 0; i < n; ++i) {
        p = GlobalLock(others[i]);
        if (!p || p[i] != (BYTE)i)
            ok = 0;
        if (p)
            GlobalUnlock(others[i]);
        if (GlobalFree(others[i]))
            ok = 0;
    }
    grown = GlobalReAlloc(a, GROWN, GMEM_MOVEABLE | GMEM_ZEROINIT);
    if (!check(grown != 0, "growing the block"))
        return 0;
    p = GlobalLock(grown);
    if (!check(p && GlobalSize(grown) >= GROWN, "locking the resized block"))
        return 0;
    for (i = 0; i < SMALL; ++i)
        if (p[i] != (BYTE)(i * 31u + 17u))
            ok = 0;
    /* Windows may round GlobalSize up beyond the first requested length. */
    for (i = SMALL + 64; i < GROWN; ++i)
        if (p[i])
            ok = 0;
    GlobalUnlock(grown);
    GlobalUnlock(fixed);
    GlobalUnlock(shared);
    if (GlobalFree(grown) || GlobalFree(fixed) || GlobalFree(shared))
        ok = 0;
    return ok;
}
int PASCAL WinMain(HINSTANCE h, HINSTANCE prev, LPSTR cmd, int show) {
    int ok;
    (void)h;
    (void)prev;
    (void)cmd;
    (void)show;
    ok = run();
    text(ok ? "Lock contract PASS\n" : "Lock contract FAIL\n");
    text("LOCK TEST END\n");
    return !ok;
}
