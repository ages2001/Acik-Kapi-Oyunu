#ifndef WINDX_WINMM_WINE_DEBUG_H
#define WINDX_WINMM_WINE_DEBUG_H
/* A standalone DirectSound library does not load Wine's debug server. */
#define WINE_DEFAULT_DEBUG_CHANNEL(name)
#define TRACE_ON(channel) 0
#define TRACE(...) ((void)0)
#define WARN(...) ((void)0)
#define ERR(...) ((void)0)
#define FIXME(...) ((void)0)
#define debugstr_guid(value) ""
#define debugstr_w(value) ""
#define wine_dbg_sprintf(...) ""
#endif
