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

// Global variables
HINSTANCE g_hInstance = NULL;
char g_szDllPath[MAX_PATH] = { 0 };                              // Full DLL path
char g_szDllFilename[1024] = "GunzFuckV4.dll";                 // DLL filename buffer with default value (1024 bytes)
DWORD g_dwListBoxHandle = 0;
BOOL g_bWatcherActive = FALSE;
BOOL g_bWatcherStop = FALSE;
BOOL g_bWindowMode = FALSE;
DWORD g_dwSelectedIndex = 0;

// String constants
const char* g_szProcesses = "Processes";
const char* g_szWindows = "Windows";
const char* g_szWindow = "Window";
const char* g_szProcess = "Process";

// Function prototypes
INT_PTR CALLBACK MainDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL InjectDLL(HWND hDlg, DWORD dwProcessId, SIZE_T dllPathLen);
BOOL WriteMemoryWrapper(HANDLE hProcess, LPVOID lpAddress, LPCVOID lpBuffer, SIZE_T nSize);
void EnumerateProcesses(HWND hDlg);
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
void ExtractFilenameFromPath(const char* fullPath, char* filename);
DWORD GetSelectedProcessPID(HWND hDlg, HWND hListBox, PROCESSENTRY32* pe32, HANDLE hSnapshot);
void ShowAboutDialog(HWND hDlg);
void BrowseForDLL(HWND hDlg);
void SetStatusText(HWND hDlg, const char* format, ...);
DWORD WINAPI ProcessWatcherThread(LPVOID lpParameter);

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
            g_dwListBoxHandle = (DWORD)GetDlgItem(hDlg, IDC_PROCESS_LIST);
            SendMessage((HWND)g_dwListBoxHandle, LB_RESETCONTENT, 0, 0);

            if (g_bWindowMode)
                EnumWindows(EnumWindowsProc, (LPARAM)hDlg);
            else
                EnumerateProcesses(hDlg);

            return TRUE;
        }

        case IDC_INJECT:
        {
            DWORD dwPID = 0;

            if (g_bWindowMode)
            {
                // Window mode - get window title and find window
                HWND hListBox = GetDlgItem(hDlg, IDC_PROCESS_LIST);

                LRESULT selIndex = SendMessage(hListBox, LB_GETCURSEL, 0, 0);
                g_dwSelectedIndex = selIndex;

                LRESULT textLen = SendMessage(hListBox, LB_GETTEXTLEN, selIndex, 0);
                char* szWindowTitle = new char[textLen + 1];

                SendMessage(hListBox, LB_GETTEXT, g_dwSelectedIndex, (LPARAM)szWindowTitle);

                HWND hTargetWnd = FindWindowA(NULL, szWindowTitle);

                if (!hTargetWnd)
                {
                    MessageBoxA(hDlg, "Window cannot be found", "Error", MB_OK);
                    delete[] szWindowTitle;
                    return TRUE;
                }

                GetWindowThreadProcessId(hTargetWnd, &dwPID);
                delete[] szWindowTitle;

                if (dwPID)
                {
                    char szStatus[512];
                    sprintf(szStatus, "PID: %i is chosen", dwPID);
                    SetDlgItemTextA(hDlg, IDC_STATUS_TEXT, szStatus);
                }

                InjectDLL(hDlg, dwPID, strlen(g_szDllPath) + 1);
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
                        sprintf(szStatus, "PID: %i is chosen", dwPID);
                        SetDlgItemTextA(hDlg, IDC_STATUS_TEXT, szStatus);
                    }

                    CloseHandle(hSnapshot);
                }

                InjectDLL(hDlg, dwPID, strlen(g_szDllPath) + 1);
            }

            return TRUE;
        }

        case IDC_USE:
        {
            const char* szUsage =
                " 1.) Load a dll \n"
                " 2.) Pick a process in the process list\n"
                " 3.) Press inject dll.\n\n"
                "Read the Read me for use of Process watcher plox";

            MessageBoxA(hDlg, szUsage, "How to use InjecTOR", MB_OK | MB_ICONINFORMATION);
            return TRUE;
        }

        case IDC_EJECT:
            return TRUE;

        case IDC_USE_WATCH:
        {
            if (g_bWatcherActive)
            {
                g_bWatcherStop = TRUE;
                CheckDlgButton(hDlg, IDC_USE_WATCH, BST_CHECKED);
            }
            else
            {
                g_bWatcherActive = TRUE;
                HANDLE hThread = CreateThread(NULL, 0, ProcessWatcherThread, hDlg, 0, NULL);
                Sleep(100);
                CloseHandle(hThread);
            }
            return TRUE;
        }

        case IDC_WINDOW_BUTTON:
        {
            g_dwListBoxHandle = (DWORD)GetDlgItem(hDlg, IDC_PROCESS_LIST);
            SendMessage((HWND)g_dwListBoxHandle, LB_RESETCONTENT, 0, 0);

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

    if (strcmp(g_szDllPath, "") == 0)
    {
        MessageBoxA(hDlg, "Choose Dll", "injector", MB_OK);
        g_bWatcherActive = FALSE;
        g_bWatcherStop = FALSE;
        CheckDlgButton(hDlg, IDC_USE_WATCH, BST_UNCHECKED);
        return -1;
    }

    HWND hProcessNameEdit = GetDlgItem(hDlg, IDC_PROCESS_NAME);
    if (!GetWindowTextLengthA(hProcessNameEdit))
    {
        MessageBoxA(hDlg, "You need to put in a name, must be valid also, with the extension", "Error", MB_OK);
        g_bWatcherActive = FALSE;
        g_bWatcherStop = FALSE;
        CheckDlgButton(hDlg, IDC_USE_WATCH, BST_UNCHECKED);
        return -1;
    }

    memset(&pe32, 0, sizeof(pe32));
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!g_bWatcherStop)
    {
        while (TRUE)
        {
            HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

            if (hSnapshot == INVALID_HANDLE_VALUE)
            {
                CloseHandle(INVALID_HANDLE_VALUE);
                g_bWatcherActive = FALSE;
                g_bWatcherStop = FALSE;
                CheckDlgButton(hDlg, IDC_USE_WATCH, BST_UNCHECKED);
                return -1;
            }

            if (Process32First(hSnapshot, &pe32) && Process32Next(hSnapshot, &pe32))
            {
                do
                {
                    if (_stricmp(pe32.szExeFile, szProcessName) == 0)
                    {
                        DWORD dwPID = pe32.th32ProcessID;
                        Sleep(500);

                        if (!InjectDLL(hDlg, dwPID, strlen(g_szDllPath) + 1))
                        {
                            CloseHandle(hSnapshot);
                            g_bWatcherActive = FALSE;
                            g_bWatcherStop = FALSE;
                            CheckDlgButton(hDlg, IDC_USE_WATCH, BST_UNCHECKED);
                            return -1;
                        }

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
BOOL InjectDLL(HWND hDlg, DWORD dwProcessId, SIZE_T dllPathLen)
{
    HANDLE hProcess = NULL;
    LPVOID lpRemoteMem = NULL;
    HANDLE hThread = NULL;
    BOOL bSuccess = FALSE;

    // Check if a DLL has been selected
    if (strlen(g_szDllPath) == 0)
    {
        MessageBoxA(hDlg, "Could not inject DLL, did you pick one!?.", "injecTOR", MB_OK);
        return FALSE;
    }

    // Open the target process
    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);
    if (!hProcess)
    {
        MessageBoxA(hDlg, "Injection Failed..Falling Back..", "InjecTOR", MB_OK);
        return FALSE;
    }

    SetStatusText(hDlg, "Injecting %s into %i", g_szDllPath, dwProcessId);

    // Allocate memory in the target process
    lpRemoteMem = VirtualAllocEx(hProcess, NULL, dllPathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!lpRemoteMem)
    {
        MessageBoxA(hDlg, "Injection Failed..Falling Back..", "InjecTOR", MB_OK);
        CloseHandle(hProcess);
        return FALSE;
    }

    // Write the DLL path to the allocated memory
    if (!WriteMemoryWrapper(hProcess, lpRemoteMem, g_szDllPath, dllPathLen))
    {
        MessageBoxA(hDlg, "Injection Failed..Falling Back..", "injecTOR", MB_OK);
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
    }
    else
    {
        MessageBoxA(hDlg, "Injection Failed..Falling Back..", "InjecTOR", MB_OK);
        VirtualFreeEx(hProcess, lpRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
    }

    return bSuccess;
}

// Wrapper for WriteProcessMemory with VirtualProtect
BOOL WriteMemoryWrapper(HANDLE hProcess, LPVOID lpAddress, LPCVOID lpBuffer, SIZE_T nSize)
{
    DWORD dwOldProtect = 0;
    BOOL bSuccess = FALSE;

    if (hProcess == NULL)
    {
        // Local process - use VirtualProtect
        VirtualProtect(lpAddress, nSize, PAGE_EXECUTE_READWRITE, &dwOldProtect);
        memcpy(lpAddress, lpBuffer, nSize);
        VirtualProtect(lpAddress, nSize, dwOldProtect, &dwOldProtect);
        bSuccess = TRUE;
    }
    else
    {
        // Remote process - use VirtualProtectEx and WriteProcessMemory
        VirtualProtectEx(hProcess, lpAddress, nSize, PAGE_EXECUTE_READWRITE, &dwOldProtect);
        bSuccess = WriteProcessMemory(hProcess, lpAddress, lpBuffer, nSize, NULL);
        VirtualProtectEx(hProcess, lpAddress, nSize, dwOldProtect, &dwOldProtect);
    }

    return bSuccess;
}

// Enumerate all running processes
void EnumerateProcesses(HWND hDlg)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        MessageBoxA(hDlg, "Could not take Snapshot", "Error", MB_ICONERROR);
        CloseHandle(hSnapshot);
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
    char* szWindowTitle = new char[textLen + 1];

    int actualLen = GetWindowTextLengthA(hParent);
    GetWindowTextA(hParent, szWindowTitle, actualLen + 1);

    HWND hListBox = GetDlgItem(hDlg, IDC_PROCESS_LIST);

    // Check if string already exists in listbox
    LRESULT findResult = SendMessageA(hListBox, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)szWindowTitle);

    if (findResult == LB_ERR && strcmp(szWindowTitle, "") != 0)
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

    // Process each character in the path
    for (size_t i = 0; i < pathLen; i++)
    {
        if (fullPath[i] == '\\')
        {
            // Reset filename buffer when we encounter a backslash
            filenameIndex = 0;
            memset(g_szDllFilename, 0, sizeof(g_szDllFilename));
        }
        else
        {
            // Copy character to global filename buffer
            g_szDllFilename[filenameIndex] = fullPath[i];
            filenameIndex++;
        }
    }

    // Copy the extracted filename to the output parameter if provided
    if (filename != NULL)
        strcpy(filename, g_szDllFilename);
}

// Get PID of selected process from listbox
DWORD GetSelectedProcessPID(HWND hDlg, HWND hListBox, PROCESSENTRY32* pe32, HANDLE hSnapshot)
{
    char szSelected[256] = { 0 };

    LRESULT iSel = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
    g_dwSelectedIndex = iSel;

    if (iSel == LB_ERR)
    {
        MessageBoxA(hDlg, "You need to pick a process....", "Error", MB_OK);
        return 0;
    }

    SendMessageA(hListBox, LB_GETTEXT, iSel, (LPARAM)szSelected);

    if (strlen(szSelected) == 0)
    {
        MessageBoxA(hDlg, "Error getting text from ListBox", "Error", MB_OK);
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
    sprintf(szFullAbout, "%s\n\n%s", szAbout, szHelp);

    MessageBoxA(hDlg, szFullAbout, "Yo yo", MB_OK | MB_ICONINFORMATION);
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
    vsprintf(szBuffer, format, args);
    va_end(args);

    SetDlgItemTextA(hDlg, IDC_STATUS_TEXT2, szBuffer);
}