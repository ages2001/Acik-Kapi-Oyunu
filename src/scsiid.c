#define WIN32_LEAN_AND_MEAN
#include "scsiid.h"
static int read_name(HKEY key, const char *value, char *out, DWORD capacity) {
    DWORD type, size = capacity - 1;
    if (RegQueryValueExA(key, value, 0, &type, (BYTE *)out, &size) || type != REG_SZ)
        return 0;
    out[size] = 0;
    return 1;
}
int port_owner(HKEY port, char *out, DWORD capacity) {
    HKEY bus;
    char name[80], type[64];
    int found = 0;
    if (read_name(port, "Driver", out, capacity))
        return 1;
    /* NT 3.1 records the adapter as a target on its own SCSI bus. */
    if (RegOpenKeyExA(port, "Scsi Bus 0", 0, KEY_READ, &bus))
        return 0;
    for (DWORD i = 0; !found && !RegEnumKeyA(bus, i, name, sizeof(name)); ++i) {
        HKEY target;
        if (RegOpenKeyExA(bus, name, 0, KEY_QUERY_VALUE, &target))
            continue;
        found = read_name(target, "Type", type, sizeof(type)) && !lstrcmpiA(type, "ScsiAdapter") &&
                read_name(target, "Identifier", out, capacity);
        RegCloseKey(target);
    }
    RegCloseKey(bus);
    return found;
}
