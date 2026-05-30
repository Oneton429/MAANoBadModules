// MAANoBadModules - DLL Hook
//
// Hooks GetModuleHandleW so that any DLL name listed in the whitelist config
// file appears "not loaded" to the caller (returns NULL).
//
// Config file: MAANoBadModules.txt, placed next to this DLL.
// Format:
//   - One DLL filename per line (case-insensitive)
//   - Lines starting with '#' are comments
//   - Blank lines are ignored
//
// If the config file is missing, the following built-in defaults are used:
//   NahimicOSD.dll, AudioDevProps2.dll, GTII-OSD64.dll, GTIII-OSD64.dll
// (These are the exact names MAA's BadModules.cs checks for.)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <MinHook.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>

// ---------------------------------------------------------------------------
// Whitelist storage
// ---------------------------------------------------------------------------

// Case-folded (lower-case) set for O(1) lookup.
static std::unordered_set<std::wstring> g_whitelist;


// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// ASCII-only tolower (safe for DLL filenames, which are always ASCII).
static std::wstring AsciiToLower(std::wstring s)
{
    for (auto& c : s)
        if (c >= L'A' && c <= L'Z') c += 32;
    return s;
}

// Returns the directory of this DLL (no trailing backslash).
static std::wstring GetSelfDir(HINSTANCE hSelf)
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(hSelf, path, MAX_PATH);
    std::wstring ws(path);
    auto pos = ws.rfind(L'\\');
    return (pos != std::wstring::npos) ? ws.substr(0, pos) : ws;
}

// Trim leading/trailing ASCII whitespace (space, tab, CR, LF) from a narrow string.
static std::string TrimNarrow(std::string s)
{
    auto notSpace = [](char c) { return c != ' ' && c != '\t' && c != '\r' && c != '\n'; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

// ---------------------------------------------------------------------------
// Config loading
// ---------------------------------------------------------------------------
// We read the file as narrow bytes (ifstream) because:
//   1. DLL filenames are pure ASCII -- no wide/Unicode needed.
//   2. wifstream + C locale breaks on UTF-8 encoded comment lines,
//      causing the stream to fail and leaving the whitelist empty.
//   3. ifstream handles CRLF cleanly after we strip '\r' ourselves.

static void LoadWhitelistFromFile(const std::wstring& path)
{
    // Open via Win32 so we can pass a wide path on any system locale.
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    // Read whole file into a buffer.
    DWORD size = GetFileSize(hFile, nullptr);
    std::string buf(size, '\0');
    DWORD read = 0;
    ReadFile(hFile, buf.data(), size, &read, nullptr);
    CloseHandle(hFile);
    buf.resize(read);

    // Parse line by line.
    std::string line;
    for (char ch : buf)
    {
        if (ch == '\n')
        {
            line = TrimNarrow(line); // also strips trailing '\r'
            if (!line.empty() && line[0] != '#')
            {
                // Convert ASCII narrow string to wstring for the set.
                std::wstring wline(line.begin(), line.end());
                g_whitelist.insert(AsciiToLower(wline));
            }
            line.clear();
        }
        else
        {
            line += ch;
        }
    }
    // Handle file that doesn't end with '\n'.
    if (!line.empty())
    {
        line = TrimNarrow(line);
        if (!line.empty() && line[0] != '#')
        {
            std::wstring wline(line.begin(), line.end());
            g_whitelist.insert(AsciiToLower(wline));
        }
    }
}

static void InitWhitelist(HINSTANCE hSelf)
{
    std::wstring dir  = GetSelfDir(hSelf);
    std::wstring conf = dir + L"\\MAANoBadModules.txt";

    LoadWhitelistFromFile(conf);

}

// ---------------------------------------------------------------------------
// Hook
// ---------------------------------------------------------------------------

using FnGetModuleHandleW = HMODULE(WINAPI*)(LPCWSTR);
static FnGetModuleHandleW g_orig = nullptr;

static bool IsWhitelisted(LPCWSTR lpModuleName)
{
    if (!lpModuleName) return false;
    return g_whitelist.count(AsciiToLower(std::wstring(lpModuleName))) > 0;
}

static HMODULE WINAPI Hooked_GetModuleHandleW(LPCWSTR lpModuleName)
{
    if (IsWhitelisted(lpModuleName))
        return nullptr; // pretend it's not loaded -> hmod.IsInvalid == true
    return g_orig(lpModuleName);
}

// ---------------------------------------------------------------------------
// Install / remove
// ---------------------------------------------------------------------------

static void* GetTarget()
{
    return reinterpret_cast<void*>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetModuleHandleW"));
}

static void InstallHook()
{
    if (MH_Initialize() != MH_OK) return;

    void* target = GetTarget();
    if (!target) return;

    if (MH_CreateHook(target,
                      reinterpret_cast<void*>(&Hooked_GetModuleHandleW),
                      reinterpret_cast<void**>(&g_orig)) != MH_OK)
        return;

    MH_EnableHook(target);
}

static void RemoveHook()
{
    void* target = GetTarget();
    if (target) MH_DisableHook(target);
    MH_Uninitialize();
}

// ---------------------------------------------------------------------------
// DllMain
// ---------------------------------------------------------------------------

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInst);
        InitWhitelist(hInst);   // load config before installing hook
        InstallHook();
        break;
    case DLL_PROCESS_DETACH:
        RemoveHook();
        break;
    }
    return TRUE;
}
