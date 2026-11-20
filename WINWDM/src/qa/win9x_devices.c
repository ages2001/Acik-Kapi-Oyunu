/* Capture the device instance and class records created by Win9x Plug and Play. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static void printValue(FILE *report, HKEY key, const char *name)
{
    char data[512];
    DWORD bytes = sizeof(data) - 1, type;
    if (RegQueryValueExA(key, name, NULL, &type, (BYTE *)data, &bytes) != ERROR_SUCCESS)
        return;
    if (type == REG_SZ || type == REG_EXPAND_SZ) {
        data[bytes] = 0;
        fprintf(report, "  %s = %s\n", name, data);
    }
}

static void printInstance(FILE *report, HKEY key)
{
    char driver[128], classPath[192];
    DWORD bytes = sizeof(driver) - 1, type;
    HKEY classKey;
    printValue(report, key, "DeviceDesc");
    printValue(report, key, "Class");
    printValue(report, key, "Driver");
    if (RegQueryValueExA(key, "Driver", NULL, &type, (BYTE *)driver, &bytes)
        != ERROR_SUCCESS || type != REG_SZ || !bytes)
        return;
    driver[bytes] = 0;
    strcpy(classPath, "System\\CurrentControlSet\\Services\\Class\\");
    strcat(classPath, driver);
    if (RegOpenKeyA(HKEY_LOCAL_MACHINE, classPath, &classKey) != ERROR_SUCCESS)
        return;
    printValue(report, classKey, "DriverDesc");
    printValue(report, classKey, "InfPath");
    printValue(report, classKey, "InfSection");
    printValue(report, classKey, "DevLoader");
    printValue(report, classKey, "NTMPDriver");
    RegCloseKey(classKey);
}

static unsigned scanBus(FILE *report, const char *bus, const char *first, const char *second)
{
    char model[256], instance[256];
    HKEY busKey, modelKey, deviceKey;
    DWORD i, j;
    unsigned found = 0;
    if (RegOpenKeyA(HKEY_LOCAL_MACHINE, bus, &busKey) != ERROR_SUCCESS) return 0;
    for (i = 0; RegEnumKeyA(busKey, i, model, sizeof(model)) == ERROR_SUCCESS; ++i) {
        if (first && strncmp(model, first, strlen(first)) &&
            (!second || strncmp(model, second, strlen(second)))) continue;
        if (RegOpenKeyA(busKey, model, &modelKey) != ERROR_SUCCESS) continue;
        for (j = 0; RegEnumKeyA(modelKey, j, instance, sizeof(instance)) == ERROR_SUCCESS; ++j) {
            if (RegOpenKeyA(modelKey, instance, &deviceKey) != ERROR_SUCCESS) continue;
            fprintf(report, "%s\\%s\\%s\n", bus, model, instance);
            printInstance(report, deviceKey);
            ++found;
            RegCloseKey(deviceKey);
        }
        RegCloseKey(modelKey);
    }
    RegCloseKey(busKey);
    return found;
}

int PASCAL WinMain(HINSTANCE app, HINSTANCE previous, LPSTR args, int show)
{
    FILE *report;
    unsigned controllers, children = 0;
    (void)app; (void)previous; (void)show;
    if (lstrcmpiA(args, "/usb") && lstrcmpiA(args, "/audio")) return 2;
    CreateDirectoryA("C:\\WDMQA", NULL);
    report = fopen(!lstrcmpiA(args, "/usb") ? "C:\\WDMQA\\USB.TXT" : "C:\\WDMQA\\AUDIO.TXT", "w");
    if (!report) return 2;
    fprintf(report, "GetVersion 0x%08lX\n", GetVersion());
    if (!lstrcmpiA(args, "/usb")) {
        controllers = scanBus(report, "Enum\\PCI", "VEN_8086&DEV_7020", "VEN_8086&DEV_7112");
        children = scanBus(report, "Enum\\USB", NULL, NULL);
    } else {
        controllers = scanBus(report, "Enum\\PCI", "VEN_8086&DEV_2415", NULL);
    }
    fprintf(report, "Matching controllers %u, USB instance records %u\n", controllers, children);
    return fclose(report) || !controllers;
}
