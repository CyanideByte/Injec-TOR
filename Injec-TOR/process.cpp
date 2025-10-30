/*
 * InjecTOR - Process Module Implementation
 * Process and window enumeration, architecture detection, and helper functions
 */

#include "process.h"

// Enumerate all running processes
void EnumerateProcesses(HWND hDlg)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        MessageBoxA(hDlg, "Could not take Snapshot", g_szMsgBoxTitle, MB_OK);
        return;
    }

    PROCESSENTRY32 pe32 = { 0 };
    pe32.dwSize = sizeof(PROCESSENTRY32);

    HWND hListBox = GetDlgItem(hDlg, IDC_PROCESS_LIST);

    if (Process32First(hSnapshot, &pe32))
    {
        do
        {
            // Apply search filter if one is set
            if (g_szSearchFilter[0] != '\0')
            {
                // Case-insensitive search
                char szLowerProcess[MAX_PATH] = { 0 };
                char szLowerFilter[256] = { 0 };

                strncpy(szLowerProcess, pe32.szExeFile, MAX_PATH - 1);
                strncpy(szLowerFilter, g_szSearchFilter, 255);

                _strlwr(szLowerProcess);
                _strlwr(szLowerFilter);

                // Skip if doesn't match filter
                if (!strstr(szLowerProcess, szLowerFilter))
                {
                    continue;
                }
            }

            // Get architecture for this process
            ProcessArchitecture arch = GetProcessArchitecture(pe32.th32ProcessID);

            // Check if we can open the process (if not, it likely requires elevation)
            BOOL bCanOpen = CanOpenProcess(pe32.th32ProcessID);

            // Apply compatible filter if enabled (only if DLL is loaded)
            if (g_bCompatibleOnly && g_DllArchitecture != ARCH_UNKNOWN)
            {
                // Only show if architectures match (elevated processes still show with [!] marker)
                if (!IsArchitectureCompatible(g_DllArchitecture, arch))
                {
                    continue; // Skip this process
                }
            }

            // Format: "processname.exe [x86]" or "[!] processname.exe [x86]" for protected processes
            char szProcessWithArch[MAX_PATH + 20] = { 0 };
            if (!bCanOpen)
            {
                // Add warning symbol
                _snprintf(szProcessWithArch, sizeof(szProcessWithArch) - 1,
                          "[!] %s [%s]", pe32.szExeFile, ArchitectureToString(arch));
            }
            else
            {
                _snprintf(szProcessWithArch, sizeof(szProcessWithArch) - 1,
                          "%s [%s]", pe32.szExeFile, ArchitectureToString(arch));
            }
            szProcessWithArch[sizeof(szProcessWithArch) - 1] = '\0';

            SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)szProcessWithArch);
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
}

// Callback for EnumWindows
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    HWND hDlg = (HWND)lParam;

    HWND hParent = GetParent(hwnd);

    int textLen = GetWindowTextLengthA(hParent);
    if (textLen <= 0)
        return TRUE;

    char* szWindowTitle = new char[textLen + 1];
    GetWindowTextA(hParent, szWindowTitle, textLen + 1);

    // Apply search filter if one is set
    if (g_szSearchFilter[0] != '\0')
    {
        // Case-insensitive search
        char szLowerWindow[MAX_PATH] = { 0 };
        char szLowerFilter[256] = { 0 };

        strncpy(szLowerWindow, szWindowTitle, MAX_PATH - 1);
        strncpy(szLowerFilter, g_szSearchFilter, 255);

        _strlwr(szLowerWindow);
        _strlwr(szLowerFilter);

        // Skip if doesn't match filter
        if (!strstr(szLowerWindow, szLowerFilter))
        {
            delete[] szWindowTitle;
            return TRUE;
        }
    }

    // Get process ID and architecture for the window
    DWORD dwPID = 0;
    GetWindowThreadProcessId(hParent, &dwPID);
    ProcessArchitecture arch = ARCH_UNKNOWN;
    BOOL bCanOpen = FALSE;
    if (dwPID != 0)
    {
        arch = GetProcessArchitecture(dwPID);
        bCanOpen = CanOpenProcess(dwPID);
    }

    // Apply compatible filter if enabled (only if DLL is loaded)
    if (g_bCompatibleOnly && g_DllArchitecture != ARCH_UNKNOWN)
    {
        // Only show if architectures match (elevated processes still show with [!] marker)
        if (!IsArchitectureCompatible(g_DllArchitecture, arch))
        {
            delete[] szWindowTitle;
            return TRUE; // Skip this window
        }
    }

    // Format with architecture: "Window Title [x86]" or "[!] Window Title [x86]" for protected processes
    char* szWindowWithArch = new char[textLen + 20];
    if (!bCanOpen && dwPID != 0)
    {
        // Add warning symbol
        _snprintf(szWindowWithArch, textLen + 19, "[!] %s [%s]", szWindowTitle, ArchitectureToString(arch));
    }
    else
    {
        _snprintf(szWindowWithArch, textLen + 19, "%s [%s]", szWindowTitle, ArchitectureToString(arch));
    }
    szWindowWithArch[textLen + 19] = '\0';

    HWND hListBox = GetDlgItem(hDlg, IDC_PROCESS_LIST);

    // Check if string already exists in listbox (check with architecture tag)
    LRESULT findResult = SendMessageA(hListBox, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)szWindowWithArch);

    if (findResult == LB_ERR && szWindowTitle[0] != '\0')
    {
        if (IsWindowVisible(hParent))
        {
            SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)szWindowWithArch);
        }
    }

    delete[] szWindowTitle;
    delete[] szWindowWithArch;
    return TRUE;
}

// Extract filename from full path
// Processes paths by clearing the g_szDllFilename buffer whenever a backslash is encountered,
// then copying subsequent characters. This effectively extracts just the filename portion from a full path.
void ExtractFilenameFromPath(const char* fullPath, char* filename)
{
    int filenameIndex = 0;
    size_t pathLen = strlen(fullPath);

    // Clear filename buffer once at start
    memset(g_szDllFilename, 0, sizeof(g_szDllFilename));

    // Process each character in the path
    for (size_t i = 0; i < pathLen; i++)
    {
        if (fullPath[i] == '\\')
        {
            // Reset filename index when we encounter a backslash
            filenameIndex = 0;
        }
        else
        {
            // Copy character to global filename buffer with bounds check
            if (filenameIndex < (int)sizeof(g_szDllFilename) - 1)
            {
                g_szDllFilename[filenameIndex] = fullPath[i];
                filenameIndex++;
            }
        }
    }

    // Copy the extracted filename to the output parameter if provided
    if (filename != NULL)
    {
        strncpy(filename, g_szDllFilename, 1023);
        filename[1023] = '\0';  // Ensure null termination
    }
}

// Get PID of selected process from listbox
DWORD GetSelectedProcessPID(HWND hDlg, HWND hListBox, PROCESSENTRY32* pe32, HANDLE hSnapshot)
{
    char szSelected[256] = { 0 };
    char szProcessName[256] = { 0 };

    LRESULT iSel = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
    g_dwSelectedIndex = (DWORD)iSel;

    if (iSel == LB_ERR)
    {
        MessageBoxA(hDlg, "You need to pick a process....", g_szMsgBoxTitle, MB_OK);
        return 0;
    }

    SendMessageA(hListBox, LB_GETTEXT, iSel, (LPARAM)szSelected);

    if (strlen(szSelected) == 0)
    {
        MessageBoxA(hDlg, "Error getting text from ListBox", g_szMsgBoxTitle, MB_OK);
        return 0;
    }

    // Extract process name (remove architecture suffix)
    ExtractProcessName(szSelected, szProcessName, sizeof(szProcessName));

    // Find the process in the snapshot
    if (Process32First(hSnapshot, pe32))
    {
        do
        {
            if (_stricmp(pe32->szExeFile, szProcessName) == 0)
            {
                return pe32->th32ProcessID;
            }
        } while (Process32Next(hSnapshot, pe32));
    }

    // Process was selected but not found in snapshot - it may have exited
    MessageBoxA(hDlg, "The selected process is no longer running.", g_szMsgBoxTitle, MB_OK);
    return 0;
}

// Find process by name and return PID
DWORD FindProcessByName(const char* processName)
{
    DWORD dwPID = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    PROCESSENTRY32 pe32 = { 0 };
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32))
    {
        do
        {
            if (_stricmp(pe32.szExeFile, processName) == 0)
            {
                dwPID = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return dwPID;
}

// Get process architecture (x86 or x64)
ProcessArchitecture GetProcessArchitecture(DWORD dwProcessId)
{
    // Get IsWow64Process function pointer
    typedef BOOL(WINAPI* LPFN_ISWOW64PROCESS)(HANDLE, PBOOL);
    LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
        GetModuleHandleA("kernel32"), "IsWow64Process");

    if (!fnIsWow64Process)
    {
        // Very old Windows version, assume x86
        return ARCH_X86;
    }

    // Try with PROCESS_QUERY_LIMITED_INFORMATION first (works better with protected processes)
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwProcessId);
    if (!hProcess)
    {
        // Fall back to PROCESS_QUERY_INFORMATION
        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwProcessId);
        if (!hProcess)
        {
            // Can't open process - likely permission issue
            // Try to make an educated guess based on our own architecture
#ifdef _WIN64
            // We're 64-bit, most processes will be 64-bit
            return ARCH_X64;
#else
            // We're 32-bit, assume process is also 32-bit
            return ARCH_X86;
#endif
        }
    }

    BOOL bIsWow64 = FALSE;
    BOOL bResult = fnIsWow64Process(hProcess, &bIsWow64);

    CloseHandle(hProcess);

    if (!bResult)
    {
        // Function call failed - make educated guess
#ifdef _WIN64
        return ARCH_X64;
#else
        return ARCH_X86;
#endif
    }

#ifdef _WIN64
    // We're running as 64-bit
    // If target is WOW64, it's 32-bit; otherwise it's 64-bit
    return bIsWow64 ? ARCH_X86 : ARCH_X64;
#else
    // We're running as 32-bit
    if (bIsWow64)
    {
        // We're 32-bit on 64-bit Windows, target is also 32-bit
        return ARCH_X86;
    }
    else
    {
        // Either we're on 32-bit Windows (target must be 32-bit),
        // or target is 64-bit (which we can't access from 32-bit process)
        // Check if we're on 64-bit Windows
        BOOL bWeAreWow64 = FALSE;
        fnIsWow64Process(GetCurrentProcess(), &bWeAreWow64);
        if (bWeAreWow64)
        {
            // We're 32-bit on 64-bit Windows, target returned FALSE for IsWow64
            // This means target is 64-bit
            return ARCH_X64;
        }
        return ARCH_X86;
    }
#endif
}

// Get DLL architecture by parsing PE header
ProcessArchitecture GetDllArchitecture(const char* dllPath)
{
    HANDLE hFile = CreateFileA(dllPath, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        return ARCH_UNKNOWN;
    }

    // Read DOS header
    IMAGE_DOS_HEADER dosHeader = { 0 };
    DWORD dwBytesRead = 0;

    if (!ReadFile(hFile, &dosHeader, sizeof(IMAGE_DOS_HEADER), &dwBytesRead, NULL) ||
        dwBytesRead != sizeof(IMAGE_DOS_HEADER))
    {
        CloseHandle(hFile);
        return ARCH_UNKNOWN;
    }

    // Check DOS signature
    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
    {
        CloseHandle(hFile);
        return ARCH_UNKNOWN;
    }

    // Seek to PE header
    if (SetFilePointer(hFile, dosHeader.e_lfanew, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
    {
        CloseHandle(hFile);
        return ARCH_UNKNOWN;
    }

    // Read PE signature
    DWORD dwPESignature = 0;
    if (!ReadFile(hFile, &dwPESignature, sizeof(DWORD), &dwBytesRead, NULL) ||
        dwBytesRead != sizeof(DWORD))
    {
        CloseHandle(hFile);
        return ARCH_UNKNOWN;
    }

    if (dwPESignature != IMAGE_NT_SIGNATURE)
    {
        CloseHandle(hFile);
        return ARCH_UNKNOWN;
    }

    // Read file header
    IMAGE_FILE_HEADER fileHeader = { 0 };
    if (!ReadFile(hFile, &fileHeader, sizeof(IMAGE_FILE_HEADER), &dwBytesRead, NULL) ||
        dwBytesRead != sizeof(IMAGE_FILE_HEADER))
    {
        CloseHandle(hFile);
        return ARCH_UNKNOWN;
    }

    CloseHandle(hFile);

    // Check machine type
    switch (fileHeader.Machine)
    {
    case IMAGE_FILE_MACHINE_I386:
        return ARCH_X86;
    case IMAGE_FILE_MACHINE_AMD64:
        return ARCH_X64;
    default:
        return ARCH_UNKNOWN;
    }
}

// Get the architecture of the injector itself
ProcessArchitecture GetInjectorArchitecture()
{
#ifdef _WIN64
    return ARCH_X64;
#else
    return ARCH_X86;
#endif
}

// Convert architecture enum to string
const char* ArchitectureToString(ProcessArchitecture arch)
{
    switch (arch)
    {
    case ARCH_X86:
        return "x86";
    case ARCH_X64:
        return "x64";
    default:
        return "???";
    }
}

// Check if DLL and process architectures are compatible
BOOL IsArchitectureCompatible(ProcessArchitecture dllArch, ProcessArchitecture procArch)
{
    if (dllArch == ARCH_UNKNOWN || procArch == ARCH_UNKNOWN)
    {
        return TRUE; // Don't block if we can't determine
    }

    ProcessArchitecture injectorArch = GetInjectorArchitecture();

    // x64 injector can inject both x86 and x64 DLLs into matching processes
    if (injectorArch == ARCH_X64)
    {
        return dllArch == procArch;
    }

    // x86 injector can only inject x86 DLLs into x86 processes
    return dllArch == ARCH_X86 && procArch == ARCH_X86;
}

// Extract process name from listbox text (removes [x86]/[x64] suffix and warning symbol)
void ExtractProcessName(const char* listboxText, char* processName, size_t maxLen)
{
    const char* pStart = listboxText;

    // Skip warning symbol if present ("[!] ")
    if (listboxText[0] == '[' && listboxText[1] == '!' && listboxText[2] == ']' && listboxText[3] == ' ')
    {
        pStart = listboxText + 4; // Skip "[!] "
    }

    strncpy(processName, pStart, maxLen - 1);
    processName[maxLen - 1] = '\0';

    // Find and remove " [x86]" or " [x64]" or " [???]" suffix
    char* pBracket = strstr(processName, " [");
    if (pBracket)
    {
        *pBracket = '\0';
    }
}

// Check if running with administrator privileges
BOOL IsRunningAsAdmin()
{
    BOOL bIsAdmin = FALSE;
    PSID pAdminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &pAdminGroup))
    {
        CheckTokenMembership(NULL, pAdminGroup, &bIsAdmin);
        FreeSid(pAdminGroup);
    }

    return bIsAdmin;
}

// Check if we can open a process (to detect if it requires elevation)
BOOL CanOpenProcess(DWORD dwProcessId)
{
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);
    if (hProcess)
    {
        CloseHandle(hProcess);
        return TRUE;
    }
    return FALSE;
}

// Check if a process exists by PID
BOOL DoesProcessExist(DWORD dwProcessId)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

    PROCESSENTRY32 pe32 = { 0 };
    pe32.dwSize = sizeof(PROCESSENTRY32);

    BOOL bExists = FALSE;
    if (Process32First(hSnapshot, &pe32))
    {
        do
        {
            if (pe32.th32ProcessID == dwProcessId)
            {
                bExists = TRUE;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return bExists;
}
