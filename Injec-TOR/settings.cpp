/*
 * InjecTOR - Settings Module Implementation
 * Handles INI file persistence for application settings
 */

#include "settings.h"

// Save last successful injection to INI file
void SaveLastInjection(const char* dllPath, const char* processName)
{
    char szIniPath[MAX_PATH] = { 0 };

    // Get the current module path
    GetModuleFileNameA(NULL, szIniPath, MAX_PATH);

    // Remove the executable filename to get the directory
    char* pLastSlash = strrchr(szIniPath, '\\');
    if (pLastSlash)
    {
        *(pLastSlash + 1) = '\0';
    }

    // Append INI filename
    strncat(szIniPath, INI_FILENAME, MAX_PATH - strlen(szIniPath) - 1);

    // Write DLL path to INI file
    WritePrivateProfileStringA("LastInjection", "DllPath", dllPath, szIniPath);

    // Write process name to INI file (only if not empty)
    if (processName && processName[0] != '\0')
    {
        WritePrivateProfileStringA("LastInjection", "ProcessName", processName, szIniPath);
    }
}

// Load last injection settings from INI file
void LoadLastInjection(HWND hDlg)
{
    char szIniPath[MAX_PATH] = { 0 };
    char szDllPath[MAX_PATH] = { 0 };
    char szProcessName[MAX_PATH] = { 0 };

    // Get the current module path
    GetModuleFileNameA(NULL, szIniPath, MAX_PATH);

    // Remove the executable filename to get the directory
    char* pLastSlash = strrchr(szIniPath, '\\');
    if (pLastSlash)
    {
        *(pLastSlash + 1) = '\0';
    }

    // Append INI filename
    strncat(szIniPath, INI_FILENAME, MAX_PATH - strlen(szIniPath) - 1);

    // Read DLL path from INI file
    GetPrivateProfileStringA("LastInjection", "DllPath", "", szDllPath, MAX_PATH, szIniPath);

    // Read process name from INI file
    GetPrivateProfileStringA("LastInjection", "ProcessName", "", szProcessName, MAX_PATH, szIniPath);

    // Check if we got valid data
    if (szDllPath[0] == '\0')
    {
        MessageBoxA(hDlg, "No previous injection found in INI file.", g_szMsgBoxTitle, MB_OK);
        return;
    }

    // Set the DLL path
    strncpy(g_szDllPath, szDllPath, MAX_PATH - 1);
    g_szDllPath[MAX_PATH - 1] = '\0';

    // Forward declaration from process.h
    extern void ExtractFilenameFromPath(const char* fullPath, char* filename);
    extern ProcessArchitecture GetDllArchitecture(const char* dllPath);
    extern const char* ArchitectureToString(ProcessArchitecture arch);
    extern void ApplySearchFilter(HWND hDlg);
    extern void SetStatusText(HWND hDlg, const char* format, ...);

    // Extract filename from full path and store in g_szDllFilename
    ExtractFilenameFromPath(g_szDllPath, NULL);

    // Detect DLL architecture
    g_DllArchitecture = GetDllArchitecture(g_szDllPath);

    // Display the filename with architecture (not full path) in the UI
    char szDllWithArch[1024 + 16] = { 0 };
    _snprintf(szDllWithArch, sizeof(szDllWithArch) - 1,
              "%s [%s]", g_szDllFilename, ArchitectureToString(g_DllArchitecture));
    szDllWithArch[sizeof(szDllWithArch) - 1] = '\0';

    HWND hDllPath = GetDlgItem(hDlg, IDC_DLL_PATH);
    SetWindowTextA(hDllPath, szDllWithArch);

    // Refresh the process list to update color coding
    ApplySearchFilter(hDlg);

    // Set the process name and start watcher if we have a process name
    if (szProcessName[0] != '\0')
    {
        SetDlgItemTextA(hDlg, IDC_PROCESS_NAME, szProcessName);

        SetStatusText(hDlg, "Loaded last injection: %s -> %s", g_szDllFilename, szProcessName);
    }
    else
    {
        SetStatusText(hDlg, "Loaded last DLL: %s", g_szDllFilename);
    }
}

// Save dark mode setting to INI file
void SaveDarkModeSetting(BOOL bDarkMode)
{
    char szIniPath[MAX_PATH] = { 0 };

    // Get the current module path
    GetModuleFileNameA(NULL, szIniPath, MAX_PATH);

    // Remove the executable filename to get the directory
    char* pLastSlash = strrchr(szIniPath, '\\');
    if (pLastSlash)
    {
        *(pLastSlash + 1) = '\0';
    }

    // Append INI filename
    strncat(szIniPath, INI_FILENAME, MAX_PATH - strlen(szIniPath) - 1);

    // Write dark mode setting to INI file
    WritePrivateProfileStringA("Settings", "DarkMode", bDarkMode ? "1" : "0", szIniPath);
}

// Load dark mode setting from INI file
void LoadDarkModeSetting()
{
    char szIniPath[MAX_PATH] = { 0 };
    char szDarkMode[16] = { 0 };

    // Get the current module path
    GetModuleFileNameA(NULL, szIniPath, MAX_PATH);

    // Remove the executable filename to get the directory
    char* pLastSlash = strrchr(szIniPath, '\\');
    if (pLastSlash)
    {
        *(pLastSlash + 1) = '\0';
    }

    // Append INI filename
    strncat(szIniPath, INI_FILENAME, MAX_PATH - strlen(szIniPath) - 1);

    // Read dark mode setting from INI file (default is "0" - off)
    GetPrivateProfileStringA("Settings", "DarkMode", "0", szDarkMode, sizeof(szDarkMode), szIniPath);

    // Set global dark mode variable
    g_bDarkMode = (szDarkMode[0] == '1');
}

// Save compatible only setting to INI file
void SaveCompatibleOnlySetting(BOOL bCompatibleOnly)
{
    char szIniPath[MAX_PATH] = { 0 };

    // Get the current module path
    GetModuleFileNameA(NULL, szIniPath, MAX_PATH);

    // Remove the executable filename to get the directory
    char* pLastSlash = strrchr(szIniPath, '\\');
    if (pLastSlash)
    {
        *(pLastSlash + 1) = '\0';
    }

    // Append INI filename
    strncat(szIniPath, INI_FILENAME, MAX_PATH - strlen(szIniPath) - 1);

    // Write compatible only setting to INI file
    WritePrivateProfileStringA("Settings", "CompatibleOnly", bCompatibleOnly ? "1" : "0", szIniPath);
}

// Load compatible only setting from INI file
void LoadCompatibleOnlySetting()
{
    char szIniPath[MAX_PATH] = { 0 };
    char szCompatibleOnly[16] = { 0 };

    // Get the current module path
    GetModuleFileNameA(NULL, szIniPath, MAX_PATH);

    // Remove the executable filename to get the directory
    char* pLastSlash = strrchr(szIniPath, '\\');
    if (pLastSlash)
    {
        *(pLastSlash + 1) = '\0';
    }

    // Append INI filename
    strncat(szIniPath, INI_FILENAME, MAX_PATH - strlen(szIniPath) - 1);

    // Read compatible only setting from INI file (default is "1" - on)
    GetPrivateProfileStringA("Settings", "CompatibleOnly", "1", szCompatibleOnly, sizeof(szCompatibleOnly), szIniPath);

    // Set global compatible only variable
    g_bCompatibleOnly = (szCompatibleOnly[0] == '1');
}
