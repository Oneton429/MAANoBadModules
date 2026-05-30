// MAANoBadModules - Launcher
//
// Place this exe alongside MAA.exe, MAANoBadModules.dll, MinHook.x64.dll.
// Run this instead of MAA.exe. It will:
//   1. Start MAA.exe in a suspended state
//   2. Inject MAANoBadModules.dll (hook is in place before any MAA code runs)
//   3. Resume MAA normally
//
// All command-line arguments passed to the launcher are forwarded to MAA.exe.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <wchar.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool GetSelfDir(wchar_t* out, DWORD capacity)
{
    wchar_t self[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, self, MAX_PATH)) return false;
    wchar_t* slash = wcsrchr(self, L'\\');
    if (!slash) return false;
    *slash = L'\0';
    wcsncpy_s(out, capacity, self, _TRUNCATE);
    return true;
}

static bool FileExists(const wchar_t* path)
{
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

// ---------------------------------------------------------------------------
// Injection (CreateRemoteThread + LoadLibraryW)
// When called on a CREATE_SUSPENDED process the main thread is paused, but
// the loader has already mapped all import DLLs so kernel32 is available.
// ---------------------------------------------------------------------------

static bool InjectDll(HANDLE hProcess, const wchar_t* dllPath)
{
    size_t bytes = (wcslen(dllPath) + 1) * sizeof(wchar_t);

    LPVOID remote = VirtualAllocEx(hProcess, nullptr, bytes,
                                   MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) return false;

    if (!WriteProcessMemory(hProcess, remote, dllPath, bytes, nullptr))
    {
        VirtualFreeEx(hProcess, remote, 0, MEM_RELEASE);
        return false;
    }

    // kernel32.dll is mapped at the same VA in every process on the same boot.
    FARPROC loadLib = GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");

    HANDLE hThread = CreateRemoteThread(
        hProcess, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLib),
        remote, 0, nullptr);

    if (!hThread)
    {
        VirtualFreeEx(hProcess, remote, 0, MEM_RELEASE);
        return false;
    }

    WaitForSingleObject(hThread, 8000);

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remote, 0, MEM_RELEASE);

    // LoadLibraryW returns the HMODULE (non-zero on success)
    return exitCode != 0;
}

// ---------------------------------------------------------------------------
// Entry point (Windows subsystem - no console window)
// ---------------------------------------------------------------------------

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    wchar_t selfDir[MAX_PATH];
    if (!GetSelfDir(selfDir, MAX_PATH))
    {
        MessageBoxW(nullptr, L"Failed to determine launcher directory.",
                    L"MAANoBadModules", MB_ICONERROR);
        return 1;
    }

    // Paths relative to the launcher's own directory
    wchar_t maaPath[MAX_PATH];
    wchar_t dllPath[MAX_PATH];
    swprintf_s(maaPath, L"%s\\MAA.exe",                selfDir);
    swprintf_s(dllPath, L"%s\\MAANoBadModules.dll", selfDir);

    if (!FileExists(maaPath))
    {
        wchar_t msg[512];
        swprintf_s(msg, L"MAA.exe not found:\n%s", maaPath);
        MessageBoxW(nullptr, msg, L"MAANoBadModules", MB_ICONERROR);
        return 1;
    }
    if (!FileExists(dllPath))
    {
        wchar_t msg[512];
        swprintf_s(msg, L"MAANoBadModules.dll not found:\n%s", dllPath);
        MessageBoxW(nullptr, msg, L"MAANoBadModules", MB_ICONERROR);
        return 1;
    }

    // Parse our own command line so we can forward extra args to MAA.exe
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    // Build command line for MAA: "MAA.exe" [arg1] [arg2] ...
    // (skip argv[0] which is our own exe path)
    wchar_t cmdLine[32768] = {};
    swprintf_s(cmdLine, L"\"%s\"", maaPath);
    if (argv)
    {
        for (int i = 1; i < argc; i++)
        {
            wcscat_s(cmdLine, L" \"");
            wcscat_s(cmdLine, argv[i]);
            wcscat_s(cmdLine, L"\"");
        }
        LocalFree(argv);
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    // Start MAA suspended so the hook is in place before any code runs
    if (!CreateProcessW(
            maaPath, cmdLine,
            nullptr, nullptr,
            FALSE,             // don't inherit handles
            CREATE_SUSPENDED,  // key flag: hook before entry point
            nullptr,           // inherit environment
            selfDir,           // working directory = launcher's dir
            &si, &pi))
    {
        wchar_t msg[256];
        swprintf_s(msg, L"CreateProcess failed: %lu", GetLastError());
        MessageBoxW(nullptr, msg, L"MAANoBadModules", MB_ICONERROR);
        return 1;
    }

    // Inject; if it fails we still resume MAA (just without the patch)
    InjectDll(pi.hProcess, dllPath);

    ResumeThread(pi.hThread);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
