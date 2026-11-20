/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <windows.h>

#ifdef _WIN32
#define FARCB CALLBACK
#define DLGRET INT_PTR
#else
#define FARCB __export FAR PASCAL
#define DLGRET BOOL
#endif

/* 8.3 product filenames work in both Program Manager and Explorer. */
#define PATH_CAP 260
extern HINSTANCE wxp_instance;
extern char wxp_error[420];
extern char wxp_folder[PATH_CAP];
extern int wxp_program_manager;

int platform_init(void);
int install_product(HWND parent);
int remove_product(HWND parent);
void describe_display(char *text);
int change_shortcuts(HWND parent, int create);
int path_join(char *out, const char *dir, const char *name);
