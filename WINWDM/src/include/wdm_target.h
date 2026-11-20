#ifndef WINWDM_TARGET_H
#define WINWDM_TARGET_H

/* Microsoft's WDM minor numbers are hexadecimal. Keep the installer order
 * stable because the private profile query uses the same six indices. */
#define WK_WDM_TARGETS(X) \
    X(100, 0x0100, "1.00", "W100") \
    X(105, 0x0105, "1.05", "W105") \
    X(110, 0x0110, "1.10", "W110") \
    X(120, 0x0120, "1.20", "W120") \
    X(130, 0x0130, "1.30", "W130") \
    X(600, 0x0600, "6.0",  "WINWDM")

#define WK_TARGET_ENUM(id, code, label, file) WK_INDEX_##id,
enum { WK_WDM_TARGETS(WK_TARGET_ENUM) WK_TARGET_COUNT };
#undef WK_TARGET_ENUM
#define WK_FULL_TARGET 0x0600
unsigned WkTargetVersion(void);
int WkMatchTarget(unsigned long controlCode);
#endif
