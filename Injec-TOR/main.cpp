/*
 * InjecTOR - A stealthy DLL injector v1.1+
 * By: g3nuin3
 *
 * Recreated from reverse engineering
 */

 // Force ANSI API usage (original was compiled with ANSI)
#undef UNICODE
#undef _UNICODE

#include <windows.h>
#include <tlhelp32.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")

// Constants
#define MAX_LISTBOX_TEXT_LEN 32767
#define INI_FILENAME "InjecTOR.ini"

// Global variables
HINSTANCE g_hInstance = NULL;
char g_szDllPath[MAX_PATH] = { 0 };                              // Full DLL path
char g_szDllFilename[1024] = "GunzFuckV4.dll";                 // DLL filename buffer with default value (1024 bytes)
volatile BOOL g_bWatcherActive = FALSE;
volatile BOOL g_bWatcherStop = FALSE;
BOOL g_bWindowMode = FALSE;
DWORD g_dwSelectedIndex = 0;

// String constants
const char* g_szProcesses = "Processes";
const char* g_szWindows = "Windows";
const char* g_szWindow = "Window";
const char* g_szProcess = "Process";
const char* g_szInjectionFailed = "Injection Failed..Falling Back..";

// MessageBox title
const char* g_szMsgBoxTitle = "InjecTOR";

// Function prototypes
INT_PTR CALLBACK MainDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL InjectDLL(HWND hDlg, DWORD dwProcessId, SIZE_T dllPathLen, const char* processName);
void EnumerateProcesses(HWND hDlg);
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
void ExtractFilenameFromPath(const char* fullPath, char* filename);
DWORD GetSelectedProcessPID(HWND hDlg, HWND hListBox, PROCESSENTRY32* pe32, HANDLE hSnapshot);
void ShowAboutDialog(HWND hDlg);
void BrowseForDLL(HWND hDlg);
void SetStatusText(HWND hDlg, const char* format, ...);
DWORD WINAPI ProcessWatcherThread(LPVOID lpParameter);
void SaveLastInjection(const char* dllPath, const char* processName);
void LoadLastInjection(HWND hDlg);
void StartProcessWatcher(HWND hDlg);
void StopProcessWatcher(HWND hDlg);
DWORD FindProcessByName(const char* processName);

// WinMain entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    g_hInstance = hInstance;

    DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_MAIN_DIALOG), NULL, MainDialogProc, 0);

    return 0;
}

// Main dialog procedure
INT_PTR CALLBACK MainDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        // Load icon
        HICON hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON1));
        SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

        // Enumerate processes
        EnumerateProcesses(hDlg);

        return TRUE;
    }

    case WM_CLOSE:
        EndDialog(hDlg, 0);
        return FALSE;

    case WM_COMMAND:
    {
        WORD wmId = LOWORD(wParam);
        WORD wmEvent = HIWORD(wParam);

        // Handle listbox selection changes
        if (wmId == IDC_PROCESS_LIST && wmEvent == LBN_SELCHANGE)
        {
            HWND hListBox = GetDlgItem(hDlg, IDC_PROCESS_LIST);
            LRESULT selIndex = SendMessage(hListBox, LB_GETCURSEL, 0, 0);

            if (selIndex != LB_ERR)
            {
                LRESULT textLen = SendMessage(hListBox, LB_GETTEXTLEN, selIndex, 0);
                if (textLen > 0 && textLen < MAX_PATH)
                {
                    char szSelectedText[MAX_PATH] = { 0 };
                    SendMessage(hListBox, LB_GETTEXT, selIndex, (LPARAM)szSelectedText);

                    // If in window mode, get the actual executable name
                    if (g_bWindowMode)
                    {
                        // Find the window by title
                        HWND hTargetWnd = FindWindowA(NULL, szSelectedText);
                        if (hTargetWnd)
                        {
                            DWORD dwPID = 0;
                            GetWindowThreadProcessId(hTargetWnd, &dwPID);

                            if (dwPID != 0)
                            {
                                // Get the process name from PID
                                HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
                                if (hSnapshot != INVALID_HANDLE_VALUE)
                                {
                                    PROCESSENTRY32 pe32 = { 0 };
                                    pe32.dwSize = sizeof(PROCESSENTRY32);

                                    if (Process32First(hSnapshot, &pe32))
                                    {
                                        do
                                        {
                                            if (pe32.th32ProcessID == dwPID)
                                            {
                                                // Found the process, use its exe name
                                                SetDlgItemTextA(hDlg, IDC_PROCESS_NAME, pe32.szExeFile);

                                                // Update status with PID
                                                char szStatus[512];
                                                _snprintf(szStatus, sizeof(szStatus) - 1, "Selected PID: %i", dwPID);
                                                szStatus[sizeof(szStatus) - 1] = '\0';
                                                SetDlgItemTextA(hDlg, IDC_STATUS_TEXT, szStatus);
                                                break;
                                            }
                                        } while (Process32Next(hSnapshot, &pe32));
                                    }

                                    CloseHandle(hSnapshot);
                                }
                            }
                        }
                    }
                    else
                    {
                        // In process mode, just use the selected text directly
                        SetDlgItemTextA(hDlg, IDC_PROCESS_NAME, szSelectedText);

                        // Find the PID and update status
                        DWORD dwPID = FindProcessByName(szSelectedText);
                        if (dwPID != 0)
                        {
                            char szStatus[512];
                            _snprintf(szStatus, sizeof(szStatus) - 1, "Selected PID: %i", dwPID);
                            szStatus[sizeof(szStatus) - 1] = '\0';
                            SetDlgItemTextA(hDlg, IDC_STATUS_TEXT, szStatus);
                        }
                    }
                }
            }
            return TRUE;
        }

        switch (wmId)
        {
        case IDC_LOAD_DLL:
        {
            BrowseForDLL(hDlg);
            return TRUE;
        }

        case IDC_ABOUT:
        {
            ShowAboutDialog(hDlg);
            return TRUE;
        }

        case IDC_OK:
        case IDCANCEL:
            EndDialog(hDlg, 0);
            return TRUE;

        case IDC_REFRESH:
        {
            HWND hListBox = GetDlgItem(hDlg, IDC_PROCESS_LIST);
            SendMessage(hListBox, LB_RESETCONTENT, 0, 0);

            if (g_bWindowMode)
                EnumWindows(EnumWindowsProc, (LPARAM)hDlg);
            else
                EnumerateProcesses(hDlg);

            return TRUE;
        }

        case IDC_INJECT:
        {
            // Check if a DLL has been selected first
            if (strlen(g_szDllPath) == 0)
            {
                MessageBoxA(hDlg, "Could not inject DLL, did you pick one!?.", g_szMsgBoxTitle, MB_OK);
                return TRUE;
            }

            DWORD dwPID = 0;
            char szProcessName[MAX_PATH] = { 0 };

            // First, try to get process name from the watch field
            GetDlgItemTextA(hDlg, IDC_PROCESS_NAME, szProcessName, MAX_PATH);

            // If process name is provided, try to find it first
            if (szProcessName[0] != '\0')
            {
                dwPID = FindProcessByName(szProcessName);
                if (dwPID != 0)
                {
                    char szStatus[512];
                    _snprintf(szStatus, sizeof(szStatus) - 1, "PID: %i is chosen (by name: %s)", dwPID, szProcessName);
                    szStatus[sizeof(szStatus) - 1] = '\0';
                    SetDlgItemTextA(hDlg, IDC_STATUS_TEXT, szStatus);
                }
            }

            // If no PID yet, fall back to listbox selection
            if (dwPID == 0)
            {
                if (g_bWindowMode)
                {
                    // Window mode - get window title and find window
                    HWND hListBox = GetDlgItem(hDlg, IDC_PROCESS_LIST);

                    LRESULT selIndex = SendMessage(hListBox, LB_GETCURSEL, 0, 0);
                    g_dwSelectedIndex = selIndex;

                    LRESULT textLen = SendMessage(hListBox, LB_GETTEXTLEN, selIndex, 0);
                    if (textLen <= 0 || textLen >= MAX_LISTBOX_TEXT_LEN)
                    {
                        MessageBoxA(hDlg, "Invalid window selection", g_szMsgBoxTitle, MB_OK);
                        return TRUE;
                    }

                    char* szWindowTitle = new char[textLen + 1];
                    SendMessage(hListBox, LB_GETTEXT, g_dwSelectedIndex, (LPARAM)szWindowTitle);

                    HWND hTargetWnd = FindWindowA(NULL, szWindowTitle);
                    delete[] szWindowTitle;

                    if (!hTargetWnd)
                    {
                        MessageBoxA(hDlg, "Window cannot be found or is no longer open", g_szMsgBoxTitle, MB_OK);
                        return TRUE;
                    }

                    GetWindowThreadProcessId(hTargetWnd, &dwPID);

                    if (dwPID)
                    {
                        // Get the actual process name from PID
                        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
                        if (hSnapshot != INVALID_HANDLE_VALUE)
                        {
                            PROCESSENTRY32 pe32 = { 0 };
                            pe32.dwSize = sizeof(PROCESSENTRY32);

                            if (Process32First(hSnapshot, &pe32))
                            {
                                do
                                {
                                    if (pe32.th32ProcessID == dwPID)
                                    {
                                        // Found the process, save its exe name
                                        strncpy(szProcessName, pe32.szExeFile, MAX_PATH - 1);
                                        szProcessName[MAX_PATH - 1] = '\0';
                                        break;
                                    }
                                } while (Process32Next(hSnapshot, &pe32));
                            }

                            CloseHandle(hSnapshot);
                        }

                        char szStatus[512];
                        _snprintf(szStatus, sizeof(szStatus) - 1, "PID: %i is chosen", dwPID);
                        szStatus[sizeof(szStatus) - 1] = '\0';
                        SetDlgItemTextA(hDlg, IDC_STATUS_TEXT, szStatus);
                    }
                }
                else
                {
                    // Process mode
                    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

                    if (hSnapshot != INVALID_HANDLE_VALUE)
                    {
                        HWND hListBox = GetDlgItem(hDlg, IDC_PROCESS_LIST);

                        PROCESSENTRY32 pe32 = { 0 };
                        pe32.dwSize = sizeof(PROCESSENTRY32);

                        dwPID = GetSelectedProcessPID(hDlg, hListBox, &pe32, hSnapshot);

                        if (dwPID)
                        {
                            char szStatus[512];
                            _snprintf(szStatus, sizeof(szStatus) - 1, "PID: %i is chosen", dwPID);
                            szStatus[sizeof(szStatus) - 1] = '\0';
                            SetDlgItemTextA(hDlg, IDC_STATUS_TEXT, szStatus);
                        }

                        CloseHandle(hSnapshot);
                    }
                }
            }

            // Only inject if we successfully got a PID
            if (dwPID != 0)
            {
                InjectDLL(hDlg, dwPID, strlen(g_szDllPath) + 1, szProcessName);
            }
            // Note: If dwPID is 0, error messages have already been shown by
            // GetSelectedProcessPID (for no selection) or earlier window mode checks

            return TRUE;
        }

        case IDC_USE:
        {
            const char* szUsage =
                " 1.) Load a dll \n"
                " 2.) Pick a process in the process list\n"
                " 3.) Press inject dll.\n\n"
                "Read the Read me for use of Process watcher plox";

            MessageBoxA(hDlg, szUsage, g_szMsgBoxTitle, MB_OK);
            return TRUE;
        }

        case IDC_EJECT:
            return TRUE;

        case IDC_USE_WATCH:
        {
            if (g_bWatcherActive)
            {
                StopProcessWatcher(hDlg);
            }
            else
            {
                StartProcessWatcher(hDlg);
            }
            return TRUE;
        }

        case IDC_USE_LAST:
        {
            LoadLastInjection(hDlg);
            return TRUE;
        }

        case IDC_WINDOW_BUTTON:
        {
            HWND hListBox = GetDlgItem(hDlg, IDC_PROCESS_LIST);
            SendMessage(hListBox, LB_RESETCONTENT, 0, 0);

            if (g_bWindowMode)
            {
                // Switch to process mode
                SetDlgItemTextA(hDlg, IDC_PROCESS_GROUP, g_szProcesses);
                HWND hButton = GetDlgItem(hDlg, IDC_WINDOW_BUTTON);
                SetWindowTextA(hButton, g_szWindow);
                EnumerateProcesses(hDlg);
                g_bWindowMode = FALSE;
            }
            else
            {
                // Switch to window mode
                SetDlgItemTextA(hDlg, IDC_PROCESS_GROUP, g_szWindows);
                HWND hButton = GetDlgItem(hDlg, IDC_WINDOW_BUTTON);
                SetWindowTextA(hButton, g_szProcess);
                EnumWindows(EnumWindowsProc, (LPARAM)hDlg);
                g_bWindowMode = TRUE;
            }

            return TRUE;
        }
        }
        break;
    }
    }

    return FALSE;
}

// Process watcher thread
DWORD WINAPI ProcessWatcherThread(LPVOID lpParameter)
{
    HWND hDlg = (HWND)lpParameter;
    char szProcessName[MAX_PATH] = { 0 };
    PROCESSENTRY32 pe32 = { 0 };

    g_bWatcherActive = TRUE;

    GetDlgItemTextA(hDlg, IDC_PROCESS_NAME, szProcessName, MAX_PATH);

    if (g_szDllPath[0] == '\0')
    {
        MessageBoxA(hDlg, "Choose Dll", g_szMsgBoxTitle, MB_OK);
        g_bWatcherActive = FALSE;
        g_bWatcherStop = FALSE;
        CheckDlgButton(hDlg, IDC_USE_WATCH, BST_UNCHECKED);
        return 1;
    }

    HWND hProcessNameEdit = GetDlgItem(hDlg, IDC_PROCESS_NAME);
    if (!GetWindowTextLengthA(hProcessNameEdit))
    {
        MessageBoxA(hDlg, "You need to put in a name, must be valid also, with the extension", g_szMsgBoxTitle, MB_OK);
        g_bWatcherActive = FALSE;
        g_bWatcherStop = FALSE;
        CheckDlgButton(hDlg, IDC_USE_WATCH, BST_UNCHECKED);
        return 1;
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!g_bWatcherStop)
    {
        while (TRUE)
        {
            HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

            if (hSnapshot == INVALID_HANDLE_VALUE)
            {
                g_bWatcherActive = FALSE;
                g_bWatcherStop = FALSE;
                CheckDlgButton(hDlg, IDC_USE_WATCH, BST_UNCHECKED);
                return 1;
            }

            if (Process32First(hSnapshot, &pe32))
            {
                do
                {
                    if (_stricmp(pe32.szExeFile, szProcessName) == 0)
                    {
                        DWORD dwPID = pe32.th32ProcessID;
                        Sleep(500);

                        if (!InjectDLL(hDlg, dwPID, strlen(g_szDllPath) + 1, szProcessName))
                        {
                            CloseHandle(hSnapshot);
                            g_bWatcherActive = FALSE;
                            g_bWatcherStop = FALSE;
                            CheckDlgButton(hDlg, IDC_USE_WATCH, BST_UNCHECKED);
                            return 1;
                        }

                        // Save successful watcher injection (already saved in InjectDLL)
                        Sleep(100);
                        CloseHandle(hSnapshot);
                        goto WATCHER_EXIT;
                    }
                } while (Process32Next(hSnapshot, &pe32));
            }

            CloseHandle(hSnapshot);
            Sleep(10);

            if (g_bWatcherStop)
                goto WATCHER_EXIT;
        }
    }

WATCHER_EXIT:
    g_bWatcherActive = FALSE;
    g_bWatcherStop = FALSE;
    CheckDlgButton(hDlg, IDC_USE_WATCH, BST_UNCHECKED);
    return 0;
}

// Inject DLL into target process
BOOL InjectDLL(HWND hDlg, DWORD dwProcessId, SIZE_T dllPathLen, const char* processName)
{
    HANDLE hProcess = NULL;
    LPVOID lpRemoteMem = NULL;
    HANDLE hThread = NULL;
    BOOL bSuccess = FALSE;

    // Open the target process
    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);
    if (!hProcess)
    {
        char szError[256];
        _snprintf(szError, sizeof(szError) - 1,
            "Failed to open process (PID: %i).\nThe process may have exited or requires elevated privileges.",
            dwProcessId);
        szError[sizeof(szError) - 1] = '\0';
        MessageBoxA(hDlg, szError, g_szMsgBoxTitle, MB_OK);
        return FALSE;
    }

    SetStatusText(hDlg, "Injecting %s into %i", g_szDllPath, dwProcessId);

    // Allocate memory in the target process
    lpRemoteMem = VirtualAllocEx(hProcess, NULL, dllPathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!lpRemoteMem)
    {
        MessageBoxA(hDlg, g_szInjectionFailed, g_szMsgBoxTitle, MB_OK);
        CloseHandle(hProcess);
        return FALSE;
    }

    // Write the DLL path to the allocated memory
    if (!WriteProcessMemory(hProcess, lpRemoteMem, g_szDllPath, dllPathLen, NULL))
    {
        MessageBoxA(hDlg, g_szInjectionFailed, g_szMsgBoxTitle, MB_OK);
        VirtualFreeEx(hProcess, lpRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    // Get address of LoadLibraryA
    HMODULE hKernel32 = GetModuleHandleA("kernel32");
    LPTHREAD_START_ROUTINE pfnLoadLibrary = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryA");

    // Create remote thread
    hThread = CreateRemoteThread(hProcess, NULL, 0, pfnLoadLibrary, lpRemoteMem, 0, NULL);

    if (hThread)
    {
        WaitForSingleObject(hThread, INFINITE);

        SetStatusText(hDlg, "SuccessFul injection......");

        VirtualFreeEx(hProcess, lpRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        CloseHandle(hThread);

        bSuccess = TRUE;

        // Save the last successful injection to INI file
        SaveLastInjection(g_szDllPath, processName);
    }
    else
    {
        MessageBoxA(hDlg, g_szInjectionFailed, g_szMsgBoxTitle, MB_OK);
        VirtualFreeEx(hProcess, lpRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
    }

    return bSuccess;
}

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
            SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)pe32.szExeFile);
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
}

// Callback for EnumWindows
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    HWND hDlg = (HWND)lParam;

    HWND hParent = GetParent(hwnd);

    if (hParent == (HWND)lParam)
        return TRUE;

    int textLen = GetWindowTextLengthA(hParent);
    if (textLen <= 0)
        return TRUE;

    char* szWindowTitle = new char[textLen + 1];
    GetWindowTextA(hParent, szWindowTitle, textLen + 1);

    HWND hListBox = GetDlgItem(hDlg, IDC_PROCESS_LIST);

    // Check if string already exists in listbox
    LRESULT findResult = SendMessageA(hListBox, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)szWindowTitle);

    if (findResult == LB_ERR && szWindowTitle[0] != '\0')
    {
        if (IsWindowVisible(hParent))
        {
            SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)szWindowTitle);
        }
    }

    delete[] szWindowTitle;
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

    LRESULT iSel = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
    g_dwSelectedIndex = iSel;

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

    // Find the process in the snapshot
    if (Process32First(hSnapshot, pe32))
    {
        do
        {
            if (_stricmp(pe32->szExeFile, szSelected) == 0)
            {
                return pe32->th32ProcessID;
            }
        } while (Process32Next(hSnapshot, pe32));
    }

    // Process was selected but not found in snapshot - it may have exited
    MessageBoxA(hDlg, "The selected process is no longer running.", g_szMsgBoxTitle, MB_OK);
    return 0;
}

// Show about dialog
void ShowAboutDialog(HWND hDlg)
{
    const char* szAbout =
        "A stealthy Dll injector v1.1+ \n"
        "By: g3nuin3  Many thanks to Hunter!\n\n"
        "Shoutz:\n"
        "Luap, L.Spiro, st00ner, Fairlight, evobyte, Xan, moklop, catch22, SasukeHa\n"
        "ScOOp, Gunout, Revoked, Borna, kemicza, ILA, etc.. etc";

    const char* szHelp =
        " 1.) Load a dll \n"
        " 2.) Pick a process in the process list\n"
        " 3.) Press inject dll.\n\n"
        "Read the Read me for use of Process watcher plox";

    char szFullAbout[1024] = { 0 };
    _snprintf(szFullAbout, sizeof(szFullAbout) - 1, "%s\n\n%s", szAbout, szHelp);
    szFullAbout[sizeof(szFullAbout) - 1] = '\0';

    MessageBoxA(hDlg, szFullAbout, g_szMsgBoxTitle, MB_OK);
}

// Browse for DLL file
// Opens file dialog to select a DLL, extracts the filename from the full path,
// and displays only the filename in the UI while storing the full path internally.
void BrowseForDLL(HWND hDlg)
{
    OPENFILENAME ofn = { 0 };
    char szFileTitle[128] = { 0 };

    // Clear the DLL path buffer
    g_szDllPath[0] = '\0';

    // Initialize OPENFILENAME structure
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hInstance = NULL;
    ofn.hwndOwner = hDlg;
    ofn.lpstrFile = g_szDllPath;                // stores full path
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "A DLL(.dll)\0*.dll\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = szFileTitle;
    ofn.nMaxFileTitle = 128;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    ofn.lpstrTitle = "Browse for a dll to inject";

    if (GetOpenFileNameA(&ofn) == TRUE)
    {
        // Extract filename from full path and store in g_szDllFilename
        ExtractFilenameFromPath(g_szDllPath, NULL);

        // Display the filename (not full path) in the UI
        HWND hDllPath = GetDlgItem(hDlg, IDC_DLL_PATH);
        SetWindowTextA(hDllPath, g_szDllFilename);
    }
}

// Set status text with formatted string
void SetStatusText(HWND hDlg, const char* format, ...)
{
    char szBuffer[512] = { 0 };
    va_list args;

    va_start(args, format);
    _vsnprintf(szBuffer, sizeof(szBuffer) - 1, format, args);
    szBuffer[sizeof(szBuffer) - 1] = '\0';
    va_end(args);

    SetDlgItemTextA(hDlg, IDC_STATUS_TEXT2, szBuffer);
}

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

    // Extract and display the filename
    ExtractFilenameFromPath(g_szDllPath, NULL);
    HWND hDllPath = GetDlgItem(hDlg, IDC_DLL_PATH);
    SetWindowTextA(hDllPath, g_szDllFilename);

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

// Start process watcher thread
void StartProcessWatcher(HWND hDlg)
{
    if (!g_bWatcherActive)
    {
        g_bWatcherActive = TRUE;
        CheckDlgButton(hDlg, IDC_USE_WATCH, BST_CHECKED);
        CreateThread(NULL, 0, ProcessWatcherThread, hDlg, 0, NULL);
        // Thread handle is intentionally not closed - thread manages its own lifetime
    }
}

// Stop process watcher thread
void StopProcessWatcher(HWND hDlg)
{
    if (g_bWatcherActive)
    {
        g_bWatcherStop = TRUE;
        CheckDlgButton(hDlg, IDC_USE_WATCH, BST_UNCHECKED);
    }
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