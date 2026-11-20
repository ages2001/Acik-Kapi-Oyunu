/* SPDX-License-Identifier: GPL-3.0-or-later */
extern char wxp_source[PATH_CAP], wxp_state[PATH_CAP];
int install_driver(HWND parent);
int remove_driver(HWND parent);
int exists(const char *path);
int copy_new(const char *from, const char *to);
int copy_reserved(const char *from, const char *to);
int delete_owned(const char *path);
int fail(const char *message);
extern int wxp_keep_display;
int install_accelerator(void);
int remove_accelerator(void);
#ifdef WXP_9X
void queue_temp_delete(const char *path);
#endif
