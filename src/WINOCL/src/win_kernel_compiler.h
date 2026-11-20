#ifndef WINOCL_KERNEL_COMPILER_H
#define WINOCL_KERNEL_COMPILER_H

#include <windows.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace FreeOCL {
static std::string windows_quote(const std::string &path)
{
    std::string result = "\"";
    size_t slashes = 0;
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '\\') { ++slashes; continue; }
        if (path[i] == '"') result.append(slashes + 1, '\\');
        result.append(slashes, '\\');
        slashes = 0;
        result += path[i];
    }
    result.append(slashes * 2, '\\');
    return result + '"';
}

static std::string windows_temp_file(const char *suffix)
{
    char dir[MAX_PATH], file[MAX_PATH];
    DWORD size = GetTempPathA(sizeof(dir), dir);
    if (!size || size >= sizeof(dir) || !GetTempFileNameA(dir, "ocl", 0, file)) return std::string();
    if (!*suffix) return file;
    std::string target = std::string(file) + suffix;
    if (MoveFileA(file, target.c_str())) return target;
    DeleteFileA(file);
    return std::string();
}

static std::string windows_kernel_include()
{
    MEMORY_BASIC_INFORMATION memory;
    char path[MAX_PATH];
    if (!VirtualQuery((void *)&windows_kernel_include, &memory, sizeof(memory))) return "include";
    DWORD n = GetModuleFileNameA((HMODULE)memory.AllocationBase, path, sizeof(path));
    if (!n || n >= sizeof(path)) return "include";
    while (n && path[n - 1] != '\\') --n;
    return std::string(path, n) + "include";
}

static bool windows_run_tool(const std::string &application, const std::string &arguments, std::ostream &log)
{
    char executable[MAX_PATH];
    DWORD length = SearchPathA(NULL, application.c_str(), ".exe", sizeof(executable), executable, NULL);
    if (!length || length >= sizeof(executable)) {
        log << "Cannot locate " << application << ", Windows error " << GetLastError() << ".\n";
        return false;
    }
    const std::string log_path = windows_temp_file("");
    if (log_path.empty()) { log << "Cannot create compiler log file.\n"; return false; }
    SECURITY_ATTRIBUTES attrs = {sizeof(attrs), NULL, TRUE};
    HANDLE out = CreateFileA(log_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &attrs, TRUNCATE_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE in = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &attrs,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    STARTUPINFOA startup = {};
    PROCESS_INFORMATION child = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = in;
    startup.hStdOutput = startup.hStdError = out;
    std::vector<char> command(arguments.begin(), arguments.end());
    command.push_back(0);
    bool started = out != INVALID_HANDLE_VALUE && in != INVALID_HANDLE_VALUE &&
            CreateProcessA(executable, command.data(), NULL, NULL, TRUE, 0, NULL, NULL, &startup, &child);
    if (!started) log << "Cannot start " << executable << ", Windows error " << GetLastError() << ".\n";
    if (in != INVALID_HANDLE_VALUE) CloseHandle(in);
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    DWORD status = 1;
    if (started) {
        CloseHandle(child.hThread);
        if (WaitForSingleObject(child.hProcess, INFINITE) == WAIT_OBJECT_0)
            GetExitCodeProcess(child.hProcess, &status);
        CloseHandle(child.hProcess);
    }
    std::ifstream messages(log_path.c_str(), std::ios::binary);
    char buffer[4096];
    while (messages.read(buffer, sizeof(buffer)) || messages.gcount())
        log.write(buffer, messages.gcount());
    messages.close();
    DeleteFileA(log_path.c_str());
    if (started && status) log << "Compiler exit code " << status << ".\n";
    return started && status == 0;
}

static std::string windows_build_kernel(const std::string &source, const std::string &compiler,
        const std::string &flags, bool compile_only, std::string *keep_source, std::ostream &log)
{
    const std::string input = windows_temp_file("");
    if (input.empty()) { log << "Cannot create compiler input.\n"; return std::string(); }
    std::string output = windows_temp_file(compile_only ? ".obj" : ".dll");
    if (output.empty()) { DeleteFileA(input.c_str()); log << "Cannot create compiler output.\n"; return output; }
    std::ofstream file(input.c_str(), std::ios::binary);
    file << "#include <FreeOCL/opencl_c.h>\n" << source;
    file.close();
    const std::string command = windows_quote(compiler) + " " + flags + " -o " + windows_quote(output) +
            " " + windows_quote(input);
    if (!file || !windows_run_tool(compiler, command, log)) {
        DeleteFileA(output.c_str());
        output.clear();
    }
    if (keep_source && !output.empty()) *keep_source = input;
    else DeleteFileA(input.c_str());
    return output;
}
}
#endif
