/* SPDX-License-Identifier: GPL-3.0-or-later */
extern char wxp_source[PATH_CAP], wxp_state[PATH_CAP];
int install_driver(HWND parent);
int remove_driver(HWND parent);
int exists(const char *path);
int copy_new(const char *from, const char *to);
int delete_owned(const char *path);
int fail(const char *message);
