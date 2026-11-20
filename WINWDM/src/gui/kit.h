#ifndef WDM_KIT_GUI_H
#define WDM_KIT_GUI_H
#include <windows.h>
#include "protocol.h"
#include "wdm_target.h"
typedef struct { unsigned version; const char *label, *stem; } WK_TARGET_OPTION;
extern const WK_TARGET_OPTION WkTargets[WK_TARGET_COUNT];
int WkActiveTarget(void);
#define KIT_TITLE "WINWDM"
#define KIT_DIR "WINWDM"
#define PATH_CAP 260
#ifndef _WIN32
#define WK_EXPORT __export
#else
#define WK_EXPORT
#endif
BOOL WkKernelReady(void);
BOOL WkPciCheck(void);
BOOL WkInstall(HWND owner, unsigned selection);
BOOL WkUninstall(HWND owner);
LRESULT WkDdeMessage(HWND w, UINT msg, WPARAM wp, LPARAM lp);
BOOL WkEditVxd(const char *driverPath, BOOL install);
BOOL WkNtHost(void);
BOOL WkService(const char *path, BOOL install);
#endif
