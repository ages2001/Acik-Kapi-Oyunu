/* CommandLineToArgvW compatibility for the original projector.
   The result is one LocalAlloc block, released by the caller with LocalFree.
   argv[0] has the shell's special executable-name quoting rules. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static int white(WCHAR c) { return c == ' ' || c == '\t'; }

LPWSTR *WINAPI fp_CommandLineToArgvW(LPCWSTR command, int *count)
{
    DWORD length = 0, capacity, n;
    LPWSTR *argv, out;
    LPCWSTR input;
    int argc = 0, quoted;
    if (!count || !command) { SetLastError(ERROR_INVALID_PARAMETER); return 0; }
    *count = 0;
    while (command[length]) {
        if (length == 0x0fffffff) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return 0; }
        length++;
    }
    if (!length) {
        /* GetModuleFileNameA is implemented on Win9x and Win32s as well as NT. */
        char name[MAX_PATH];
        n = GetModuleFileNameA(0, name, sizeof(name));
        if (!n) return 0;
        if (n >= sizeof(name)) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
        capacity = MultiByteToWideChar(CP_ACP, 0, name, -1, 0, 0);
        if (!capacity) return 0;
        argv = LocalAlloc(LMEM_FIXED, 2 * sizeof(*argv) + capacity * sizeof(WCHAR));
        if (!argv) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return 0; }
        argv[0] = (LPWSTR)(argv + 2); argv[1] = 0;
        if (!MultiByteToWideChar(CP_ACP, 0, name, -1, argv[0], capacity)) {
            DWORD error = GetLastError(); LocalFree(argv); SetLastError(error); return 0;
        }
        *count = 1; return argv;
    }
    /* Upper bounds include the empty first argument after leading whitespace. */
    capacity = length + 2;
    argv = LocalAlloc(LMEM_FIXED, capacity * sizeof(*argv) + (length + 1) * sizeof(WCHAR));
    if (!argv) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return 0; }
    out = (LPWSTR)(argv + capacity); input = command;
    argv[argc++] = out;
    if (*input == '"') {
        input++;
        while (*input && *input != '"') *out++ = *input++;
        if (*input) input++;
    } else {
        while (*input && !white(*input)) *out++ = *input++;
    }
    *out++ = 0;
    while (*input) {
        while (white(*input)) input++;
        if (!*input) break;
        argv[argc++] = out; quoted = 0;
        while (*input && (quoted || !white(*input))) {
            DWORD slashes = 0;
            while (*input == '\\') { slashes++; input++; }
            if (*input == '"') {
                for (n = 0; n < slashes / 2; n++) *out++ = '\\';
                if (slashes & 1) { *out++ = '"'; input++; continue; }
                /* Quote runs use the shell's three-state rule. */
                do {
                    input++;
                    if (++quoted == 3) { *out++ = '"'; quoted = 0; }
                } while (*input == '"');
                if (quoted == 2) quoted = 0;
            } else {
                while (slashes--) *out++ = '\\';
                if (!*input || (!quoted && white(*input))) break;
                *out++ = *input++;
            }
        }
        *out++ = 0;
    }
    argv[argc] = 0; *count = argc; return argv;
}
