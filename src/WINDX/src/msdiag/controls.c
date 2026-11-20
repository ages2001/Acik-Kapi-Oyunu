#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

static HMODULE controls;
static FARPROC control_proc(const char *name)
{
    HMODULE loaded = controls;
    if (!loaded) {
        loaded = LoadLibraryA("wxctl.dll");
        if (!loaded) return NULL;
        HMODULE other = InterlockedCompareExchangePointer((void *volatile *)&controls, loaded, NULL);
        if (other) { FreeLibrary(loaded); loaded = other; }
    }
    return GetProcAddress(loaded, name);
}

void WINAPI wx_init_controls(void)
{
    void (WINAPI *initialize)(void) = (void *)control_proc("InitCommonControls");
    if (initialize) initialize();
}

HIMAGELIST WINAPI wx_image_list(int width, int height, UINT flags, int initial, int grow)
{
    HIMAGELIST (WINAPI *create)(int,int,UINT,int,int) = (void *)control_proc("ImageList_Create");
    if (!create) return NULL;
    HIMAGELIST list = create(width, height, flags, initial, grow);
    if (!list && !GetProcAddress(GetModuleHandleA("gdi32.dll"), "CreateDIBSection"))
        list = create(width, height, (flags & ~ILC_COLORDDB) | ILC_COLORDDB, initial, grow);
    return list;
}

int WINAPI wx_image_replace(HIMAGELIST list, int index, HICON icon)
{
    int (WINAPI *replace)(HIMAGELIST,int,HICON) = (void *)control_proc("ImageList_ReplaceIcon");
    return replace ? replace(list, index, icon) : -1;
}

BOOL WINAPI wx_image_destroy(HIMAGELIST list)
{
    BOOL (WINAPI *destroy)(HIMAGELIST) = (void *)control_proc("ImageList_Destroy");
    return destroy && destroy(list);
}
