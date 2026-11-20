#ifndef WINDX_TEST_OUTPUT_H
#define WINDX_TEST_OUTPUT_H

static void report(const char *message)
{
    DWORD count;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), message, lstrlenA(message), &count, NULL);
}

static void report_hex(ULONG_PTR value)
{
    static const char digits[] = "0123456789abcdef";
    char text[2 * sizeof(value) + 1];
    unsigned int i = sizeof(text) - 1;
    text[i] = 0;
    while (i) {
        text[--i] = digits[value & 15];
        value >>= 4;
    }
    report(text);
}

static LONG WINAPI record_fault(EXCEPTION_POINTERS *fault)
{
    MEMORY_BASIC_INFORMATION memory;
    char path[MAX_PATH];
    void *address = fault->ExceptionRecord->ExceptionAddress;
    report("FAIL exception ");
    report_hex(fault->ExceptionRecord->ExceptionCode);
    if (VirtualQuery(address, &memory, sizeof(memory)) &&
            GetModuleFileNameA((HMODULE)memory.AllocationBase, path, sizeof(path))) {
        report(" in ");
        report(path);
        report(" at RVA ");
        report_hex((ULONG_PTR)address - (ULONG_PTR)memory.AllocationBase);
    }
    report("\r\n");
    return EXCEPTION_EXECUTE_HANDLER;
}

static void initialize_test(void)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    SetUnhandledExceptionFilter(record_fault);
}
#endif
