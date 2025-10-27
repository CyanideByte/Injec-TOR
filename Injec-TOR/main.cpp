/*
 * InjecTOR - A stealthy DLL injector v1.2
 * By: g3nuin3
 * Recreated and updated by CyanideByte
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
#include <shellapi.h>
#include <dwmapi.h>
#include <stdio.h>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")

// Dark mode attribute (for Windows 10 version 1809+)
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// Constants
#define MAX_LISTBOX_TEXT_LEN 32767
#define INI_FILENAME "Injec-TOR.ini"

// Define PROCESS_QUERY_LIMITED_INFORMATION if not available (for older SDKs)
#ifndef PROCESS_QUERY_LIMITED_INFORMATION
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#endif

// Architecture types
enum ProcessArchitecture
{
    ARCH_UNKNOWN = 0,
    ARCH_X86 = 1,
    ARCH_X64 = 2
};

// Global variables
HINSTANCE g_hInstance = NULL;
HWND g_hMainDialog = NULL;                                       // Main child dialog handle
HWND g_hSettingsDialog = NULL;                                   // Settings child dialog handle
HWND g_hFrameDialog = NULL;                                      // Frame dialog handle
char g_szDllPath[MAX_PATH] = { 0 };                              // Full DLL path
char g_szDllFilename[1024] = { 0 };                              // DLL filename buffer (1024 bytes)
volatile BOOL g_bWatcherActive = FALSE;
volatile BOOL g_bWatcherStop = FALSE;
BOOL g_bWindowMode = FALSE;
DWORD g_dwSelectedIndex = 0;
ProcessArchitecture g_DllArchitecture = ARCH_UNKNOWN;            // Architecture of loaded DLL
char g_szSearchFilter[256] = { 0 };                              // Search filter text
BOOL g_bCompatibleOnly = FALSE;                                  // Filter to show only compatible processes
BOOL g_bDarkMode = FALSE;                                        // Dark mode enabled
HBRUSH g_hDarkBrush = NULL;                                      // Dark background brush
HBRUSH g_hLightBrush = NULL;                                     // Light background brush
HBRUSH g_hButtonBrush = NULL;                                    // Button background brush

// String constants
const char* g_szProcesses = "Processes";
const char* g_szWindows = "Windows";
const char* g_szWindow = "Window";
const char* g_szProcess = "Process";
const char* g_szInjectionFailed = "Injection Failed..Falling Back..";

// MessageBox title
const char* g_szMsgBoxTitle = "InjecTOR";

// Function prototypes
INT_PTR CALLBACK FrameDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK MainDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void ShowMainView();
void ShowSettingsView();
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
void SaveDarkModeSetting(BOOL bDarkMode);
void LoadDarkModeSetting();
void StartProcessWatcher(HWND hDlg);
void StopProcessWatcher(HWND hDlg);
DWORD FindProcessByName(const char* processName);
ProcessArchitecture GetProcessArchitecture(DWORD dwProcessId);
ProcessArchitecture GetDllArchitecture(const char* dllPath);
ProcessArchitecture GetInjectorArchitecture();
const char* ArchitectureToString(ProcessArchitecture arch);
BOOL IsArchitectureCompatible(ProcessArchitecture dllArch, ProcessArchitecture procArch);
void ApplySearchFilter(HWND hDlg);
void ExtractProcessName(const char* listboxText, char* processName, size_t maxLen);
BOOL IsRunningAsAdmin();
BOOL CanOpenProcess(DWORD dwProcessId);
void ApplyDarkMode(HWND hDlg, BOOL bEnable);

// WinMain entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    g_hInstance = hInstance;

    DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_FRAME_DIALOG), NULL, FrameDialogProc, 0);

    return 0;
}

// Show main view (hide settings, show main)
void ShowMainView()
{
    if (g_hSettingsDialog)
        ShowWindow(g_hSettingsDialog, SW_HIDE);
    if (g_hMainDialog)
    {
        ShowWindow(g_hMainDialog, SW_SHOW);
        // Apply dark mode to main dialog in case it changed
        ApplyDarkMode(g_hMainDialog, g_bDarkMode);
    }
}

// Show settings view (hide main, show settings)
void ShowSettingsView()
{
    if (g_hMainDialog)
        ShowWindow(g_hMainDialog, SW_HIDE);
    if (g_hSettingsDialog)
        ShowWindow(g_hSettingsDialog, SW_SHOW);
}

// Frame dialog procedure
INT_PTR CALLBACK FrameDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        g_hFrameDialog = hDlg;

        // Load icon
        HICON hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON1));
        SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

        // Enable drag-and-drop for DLL files
        DragAcceptFiles(hDlg, TRUE);

        // Initialize color brushes
        g_hDarkBrush = CreateSolidBrush(RGB(30, 30, 30));
        g_hLightBrush = CreateSolidBrush(GetSysColor(COLOR_3DFACE));
        g_hButtonBrush = CreateSolidBrush(RGB(50, 50, 50));

        // Load dark mode setting from INI file
        LoadDarkModeSetting();

        // Apply dark mode to frame if enabled
        if (g_bDarkMode)
        {
            BOOL useDarkMode = TRUE;
            DwmSetWindowAttribute(hDlg, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
        }

        // Get client area
        RECT rcClient;
        GetClientRect(hDlg, &rcClient);

        // Calculate where child dialogs should start by finding the banner's actual bottom position
        // The banner and text are static controls in the frame, we need their actual pixel positions
        RECT rcBanner = {8, 7, 8+237, 7+105};    // Dialog units
        RECT rcText = {8, 114, 8+80, 114+8};     // Dialog units
        MapDialogRect(hDlg, &rcBanner);           // Convert to pixels
        MapDialogRect(hDlg, &rcText);             // Convert to pixels

        // Child dialogs start after the text
        int childY = rcText.bottom + 4;  // Add small gap

        // Create main child dialog
        g_hMainDialog = CreateDialogParam(g_hInstance, MAKEINTRESOURCE(IDD_MAIN_DIALOG),
                                          hDlg, MainDialogProc, 0);
        if (g_hMainDialog)
        {
            // Get the actual size of the child dialog after creation
            RECT rcChild;
            GetWindowRect(g_hMainDialog, &rcChild);
            int childWidth = rcChild.right - rcChild.left;
            int childHeight = rcChild.bottom - rcChild.top;

            // Position it below the banner
            MoveWindow(g_hMainDialog, 0, childY, childWidth, childHeight, FALSE);
            ShowWindow(g_hMainDialog, SW_SHOW);
        }

        // Create settings child dialog (hidden initially)
        g_hSettingsDialog = CreateDialogParam(g_hInstance, MAKEINTRESOURCE(IDD_SETTINGS_DIALOG),
                                               hDlg, SettingsDialogProc, 0);
        if (g_hSettingsDialog)
        {
            // Get the actual size of the child dialog after creation
            RECT rcChild;
            GetWindowRect(g_hSettingsDialog, &rcChild);
            int childWidth = rcChild.right - rcChild.left;
            int childHeight = rcChild.bottom - rcChild.top;

            // Position it below the banner
            MoveWindow(g_hSettingsDialog, 0, childY, childWidth, childHeight, FALSE);
            ShowWindow(g_hSettingsDialog, SW_HIDE);
        }

        // Check if running as admin and update title bar
        if (IsRunningAsAdmin())
        {
            char szTitle[256] = { 0 };
            GetWindowTextA(hDlg, szTitle, sizeof(szTitle) - 20);
            strncat(szTitle, " - Elevated", sizeof(szTitle) - strlen(szTitle) - 1);
            szTitle[sizeof(szTitle) - 1] = '\0';
            SetWindowTextA(hDlg, szTitle);
        }

        return TRUE;
    }

    case WM_SIZE:
    {
        // Calculate where child dialogs should be positioned (below banner and text)
        RECT rcText = {8, 114, 8+80, 114+8};     // Dialog units
        MapDialogRect(hDlg, &rcText);             // Convert to pixels
        int childY = rcText.bottom + 4;           // Add small gap

        if (g_hMainDialog)
        {
            RECT rcChild;
            GetWindowRect(g_hMainDialog, &rcChild);
            int childWidth = rcChild.right - rcChild.left;
            int childHeight = rcChild.bottom - rcChild.top;
            MoveWindow(g_hMainDialog, 0, childY, childWidth, childHeight, TRUE);
        }

        if (g_hSettingsDialog)
        {
            RECT rcChild;
            GetWindowRect(g_hSettingsDialog, &rcChild);
            int childWidth = rcChild.right - rcChild.left;
            int childHeight = rcChild.bottom - rcChild.top;
            MoveWindow(g_hSettingsDialog, 0, childY, childWidth, childHeight, TRUE);
        }

        return TRUE;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
    {
        if (g_bDarkMode)
        {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(220, 220, 220));
            SetBkColor(hdcStatic, RGB(30, 30, 30));
            return (INT_PTR)g_hDarkBrush;
        }
        break;
    }

    case WM_CTLCOLOREDIT:
    {
        if (g_bDarkMode)
        {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, RGB(220, 220, 220));
            SetBkColor(hdcEdit, RGB(45, 45, 45));
            return (INT_PTR)CreateSolidBrush(RGB(45, 45, 45));
        }
        break;
    }

    case WM_CTLCOLORBTN:
    {
        if (g_bDarkMode)
        {
            HDC hdcButton = (HDC)wParam;
            SetTextColor(hdcButton, RGB(220, 220, 220));
            SetBkMode(hdcButton, TRANSPARENT);
            return (INT_PTR)g_hButtonBrush;
        }
        break;
    }

    case WM_CTLCOLORLISTBOX:
    {
        if (g_bDarkMode)
        {
            HDC hdcListBox = (HDC)wParam;
            SetTextColor(hdcListBox, RGB(220, 220, 220));
            SetBkColor(hdcListBox, RGB(45, 45, 45));
            return (INT_PTR)CreateSolidBrush(RGB(45, 45, 45));
        }
        break;
    }

    case WM_DROPFILES:
    {
        // Forward to main dialog if visible
        if (g_hMainDialog && IsWindowVisible(g_hMainDialog))
        {
            SendMessage(g_hMainDialog, WM_DROPFILES, wParam, lParam);
        }
        return TRUE;
    }

    case WM_CLOSE:
        // Cleanup brushes
        if (g_hDarkBrush)
            DeleteObject(g_hDarkBrush);
        if (g_hLightBrush)
            DeleteObject(g_hLightBrush);
        if (g_hButtonBrush)
            DeleteObject(g_hButtonBrush);

        // Destroy child dialogs
        if (g_hMainDialog)
            DestroyWindow(g_hMainDialog);
        if (g_hSettingsDialog)
            DestroyWindow(g_hSettingsDialog);

        EndDialog(hDlg, 0);
        return TRUE;
    }

    return FALSE;
}

// Main dialog procedure
INT_PTR CALLBACK MainDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        // Apply dark mode if enabled
        if (g_bDarkMode)
        {
            ApplyDarkMode(hDlg, TRUE);
        }

        // Show status based on elevation
        if (!IsRunningAsAdmin())
        {
            SetStatusText(hDlg, "WARNING: Not elevated - some processes inaccessible to the injector");
        }

        // Enumerate processes
        EnumerateProcesses(hDlg);

        return TRUE;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
    {
        if (g_bDarkMode)
        {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(220, 220, 220));
            SetBkColor(hdcStatic, RGB(30, 30, 30));
            return (INT_PTR)g_hDarkBrush;
        }
        break;
    }

    case WM_CTLCOLOREDIT:
    {
        if (g_bDarkMode)
        {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, RGB(220, 220, 220));
            SetBkColor(hdcEdit, RGB(45, 45, 45));
            if (!g_hDarkBrush)
                g_hDarkBrush = CreateSolidBrush(RGB(45, 45, 45));
            return (INT_PTR)CreateSolidBrush(RGB(45, 45, 45));
        }
        break;
    }

    case WM_CTLCOLORBTN:
    {
        if (g_bDarkMode)
        {
            HDC hdcButton = (HDC)wParam;
            SetTextColor(hdcButton, RGB(220, 220, 220));
            SetBkMode(hdcButton, TRANSPARENT);
            return (INT_PTR)g_hButtonBrush;
        }
        break;
    }

    case WM_CTLCOLORLISTBOX:
    {
        if (g_bDarkMode)
        {
            HDC hdcListBox = (HDC)wParam;
            SetTextColor(hdcListBox, RGB(220, 220, 220));
            SetBkColor(hdcListBox, RGB(45, 45, 45));
            return (INT_PTR)CreateSolidBrush(RGB(45, 45, 45));
        }
        break;
    }

    case WM_DROPFILES:
    {
        HDROP hDrop = (HDROP)wParam;
        char szDroppedFile[MAX_PATH] = { 0 };

        // Get the first dropped file
        UINT fileCount = DragQueryFileA(hDrop, 0xFFFFFFFF, NULL, 0);
        if (fileCount > 0)
        {
            DragQueryFileA(hDrop, 0, szDroppedFile, MAX_PATH);

            // Check if it's a DLL file
            char* pExtension = strrchr(szDroppedFile, '.');
            if (pExtension && _stricmp(pExtension, ".dll") == 0)
            {
                // Set the DLL path
                strncpy(g_szDllPath, szDroppedFile, MAX_PATH - 1);
                g_szDllPath[MAX_PATH - 1] = '\0';

                // Extract filename from full path and store in g_szDllFilename
                ExtractFilenameFromPath(g_szDllPath, NULL);

                // Detect DLL architecture
                g_DllArchitecture = GetDllArchitecture(g_szDllPath);

                // Check if 32-bit injector trying to load 64-bit DLL
                ProcessArchitecture injectorArch = GetInjectorArchitecture();
                if (injectorArch == ARCH_X86 && g_DllArchitecture == ARCH_X64)
                {
                    MessageBoxA(hDlg, "Cannot load x64 DLL in 32-bit injector. Please use the 64-bit build.", g_szMsgBoxTitle, MB_OK);
                    g_szDllPath[0] = '\0';  // Clear the DLL path
                    g_DllArchitecture = ARCH_UNKNOWN;
                    return TRUE;
                }

                // Display the filename with architecture (not full path) in the UI
                char szDllWithArch[1024 + 16] = { 0 };
                _snprintf(szDllWithArch, sizeof(szDllWithArch) - 1,
                          "%s [%s]", g_szDllFilename, ArchitectureToString(g_DllArchitecture));
                szDllWithArch[sizeof(szDllWithArch) - 1] = '\0';

                HWND hDllPath = GetDlgItem(hDlg, IDC_DLL_PATH);
                SetWindowTextA(hDllPath, szDllWithArch);

                // Refresh the process list to update color coding
                ApplySearchFilter(hDlg);

                SetStatusText(hDlg, "DLL loaded: %s", g_szDllFilename);
            }
            else
            {
                MessageBoxA(hDlg, "Please drop a .DLL file", g_szMsgBoxTitle, MB_OK | MB_ICONWARNING);
            }
        }

        DragFinish(hDrop);
        return TRUE;
    }

    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;

        // Handle buttons in dark mode
        if (pDIS->CtlType == ODT_BUTTON)
        {
            // Get button text
            char szText[256] = { 0 };
            GetWindowTextA(pDIS->hwndItem, szText, sizeof(szText));

            // Set colors based on state and mode
            COLORREF bgColor, textColor;
            if (g_bDarkMode)
            {
                if (pDIS->itemState & ODS_SELECTED)
                {
                    bgColor = RGB(70, 70, 70);
                    textColor = RGB(255, 255, 255);
                }
                else
                {
                    bgColor = RGB(50, 50, 50);
                    textColor = RGB(220, 220, 220);
                }
            }
            else
            {
                bgColor = GetSysColor(COLOR_BTNFACE);
                textColor = GetSysColor(COLOR_BTNTEXT);
            }

            // Fill button background
            HBRUSH hBrush = CreateSolidBrush(bgColor);
            FillRect(pDIS->hDC, &pDIS->rcItem, hBrush);
            DeleteObject(hBrush);

            // Draw 3D border effect
            RECT rc = pDIS->rcItem;
            BOOL bPressed = (pDIS->itemState & ODS_SELECTED);

            if (g_bDarkMode)
            {
                // Dark mode 3D effect with double border for depth
                if (bPressed)
                {
                    // Pressed/sunken look
                    HPEN hPenDarkOuter = CreatePen(PS_SOLID, 1, RGB(10, 10, 10));
                    HPEN hPenDarkInner = CreatePen(PS_SOLID, 1, RGB(30, 30, 30));

                    // Outer dark border (top and left)
                    SelectObject(pDIS->hDC, hPenDarkOuter);
                    MoveToEx(pDIS->hDC, rc.left, rc.bottom - 1, NULL);
                    LineTo(pDIS->hDC, rc.left, rc.top);
                    LineTo(pDIS->hDC, rc.right - 1, rc.top);

                    // Inner shadow (top and left)
                    SelectObject(pDIS->hDC, hPenDarkInner);
                    MoveToEx(pDIS->hDC, rc.left + 1, rc.bottom - 2, NULL);
                    LineTo(pDIS->hDC, rc.left + 1, rc.top + 1);
                    LineTo(pDIS->hDC, rc.right - 2, rc.top + 1);

                    DeleteObject(hPenDarkOuter);
                    DeleteObject(hPenDarkInner);
                }
                else
                {
                    // Raised look with highlight and shadow
                    HPEN hPenHighlight = CreatePen(PS_SOLID, 1, RGB(90, 90, 90));
                    HPEN hPenLight = CreatePen(PS_SOLID, 1, RGB(70, 70, 70));
                    HPEN hPenShadow = CreatePen(PS_SOLID, 1, RGB(30, 30, 30));
                    HPEN hPenDark = CreatePen(PS_SOLID, 1, RGB(15, 15, 15));

                    // Outer highlight (top and left)
                    SelectObject(pDIS->hDC, hPenHighlight);
                    MoveToEx(pDIS->hDC, rc.left, rc.bottom - 1, NULL);
                    LineTo(pDIS->hDC, rc.left, rc.top);
                    LineTo(pDIS->hDC, rc.right - 1, rc.top);

                    // Inner highlight
                    SelectObject(pDIS->hDC, hPenLight);
                    MoveToEx(pDIS->hDC, rc.left + 1, rc.bottom - 2, NULL);
                    LineTo(pDIS->hDC, rc.left + 1, rc.top + 1);
                    LineTo(pDIS->hDC, rc.right - 2, rc.top + 1);

                    // Outer shadow (bottom and right)
                    SelectObject(pDIS->hDC, hPenDark);
                    MoveToEx(pDIS->hDC, rc.right - 1, rc.top, NULL);
                    LineTo(pDIS->hDC, rc.right - 1, rc.bottom - 1);
                    LineTo(pDIS->hDC, rc.left, rc.bottom - 1);

                    // Inner shadow
                    SelectObject(pDIS->hDC, hPenShadow);
                    MoveToEx(pDIS->hDC, rc.right - 2, rc.top + 1, NULL);
                    LineTo(pDIS->hDC, rc.right - 2, rc.bottom - 2);
                    LineTo(pDIS->hDC, rc.left + 1, rc.bottom - 2);

                    DeleteObject(hPenHighlight);
                    DeleteObject(hPenLight);
                    DeleteObject(hPenShadow);
                    DeleteObject(hPenDark);
                }
            }
            else
            {
                // Light mode - use Windows DrawEdge for proper 3D look
                UINT edge = bPressed ? BDR_SUNKENOUTER : BDR_RAISEDOUTER;
                DrawEdge(pDIS->hDC, &rc, edge, BF_RECT);
            }

            // Draw text
            SetTextColor(pDIS->hDC, textColor);
            SetBkMode(pDIS->hDC, TRANSPARENT);
            DrawTextA(pDIS->hDC, szText, -1, &pDIS->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // Draw focus rectangle
            if (pDIS->itemState & ODS_FOCUS)
            {
                RECT rcFocus = pDIS->rcItem;
                InflateRect(&rcFocus, -3, -3);
                DrawFocusRect(pDIS->hDC, &rcFocus);
            }

            return TRUE;
        }

        // Handle our process list
        if (pDIS->CtlID != IDC_PROCESS_LIST)
            return FALSE;

        // No items to draw
        if (pDIS->itemID == (UINT)-1)
            return TRUE;

        // Get item text
        char szItemText[MAX_PATH + 16] = { 0 };
        SendMessageA(pDIS->hwndItem, LB_GETTEXT, pDIS->itemID, (LPARAM)szItemText);

        // Extract process name to determine architecture
        char szProcessName[MAX_PATH] = { 0 };
        ExtractProcessName(szItemText, szProcessName, sizeof(szProcessName));

        // Determine architecture from the text
        ProcessArchitecture itemArch = ARCH_UNKNOWN;
        if (strstr(szItemText, "[x86]"))
            itemArch = ARCH_X86;
        else if (strstr(szItemText, "[x64]"))
            itemArch = ARCH_X64;

        // Check if process has warning symbol (inaccessible)
        BOOL bHasWarning = (szItemText[0] == '[' && szItemText[1] == '!' && szItemText[2] == ']');

        // Determine if compatible with loaded DLL
        BOOL bCompatible = IsArchitectureCompatible(g_DllArchitecture, itemArch);

        // Set colors based on compatibility
        COLORREF textColor;
        COLORREF bgColor;

        if (pDIS->itemState & ODS_SELECTED)
        {
            // Selected item
            bgColor = GetSysColor(COLOR_HIGHLIGHT);
            textColor = GetSysColor(COLOR_HIGHLIGHTTEXT);
        }
        else
        {
            // Not selected - apply color coding
            bgColor = g_bDarkMode ? RGB(30, 30, 30) : GetSysColor(COLOR_WINDOW);

            if (g_DllArchitecture == ARCH_UNKNOWN)
            {
                // No DLL loaded, show normal color
                textColor = g_bDarkMode ? RGB(220, 220, 220) : GetSysColor(COLOR_WINDOWTEXT);
            }
            else if (bHasWarning && bCompatible)
            {
                // Compatible but inaccessible - orange
                textColor = RGB(255, 140, 0);
            }
            else if (bCompatible)
            {
                // Compatible and accessible - green
                textColor = RGB(0, 128, 0);
            }
            else
            {
                // Incompatible - red
                textColor = RGB(192, 0, 0);
            }
        }

        // Fill background
        HBRUSH hBrush = CreateSolidBrush(bgColor);
        FillRect(pDIS->hDC, &pDIS->rcItem, hBrush);
        DeleteObject(hBrush);

        // Draw text
        SetTextColor(pDIS->hDC, textColor);
        SetBkMode(pDIS->hDC, TRANSPARENT);

        RECT rcText = pDIS->rcItem;
        rcText.left += 2; // Small margin

        DrawTextA(pDIS->hDC, szItemText, -1, &rcText, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        // Draw focus rectangle if needed
        if (pDIS->itemState & ODS_FOCUS)
        {
            DrawFocusRect(pDIS->hDC, &pDIS->rcItem);
        }

        return TRUE;
    }

    case WM_COMMAND:
    {
        WORD wmId = LOWORD(wParam);
        WORD wmEvent = HIWORD(wParam);

        // Handle search filter text changes
        if (wmId == IDC_SEARCH_FILTER && wmEvent == EN_CHANGE)
        {
            GetDlgItemTextA(hDlg, IDC_SEARCH_FILTER, g_szSearchFilter, sizeof(g_szSearchFilter));
            ApplySearchFilter(hDlg);
            return TRUE;
        }

        // Handle compatible filter checkbox
        if (wmId == IDC_COMPATIBLE_ONLY && wmEvent == BN_CLICKED)
        {
            g_bCompatibleOnly = (IsDlgButtonChecked(hDlg, IDC_COMPATIBLE_ONLY) == BST_CHECKED);
            ApplySearchFilter(hDlg);
            return TRUE;
        }

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
                        // Extract clean window title (remove [!] prefix and [x86]/[x64] suffix)
                        char szCleanTitle[MAX_PATH] = { 0 };
                        ExtractProcessName(szSelectedText, szCleanTitle, sizeof(szCleanTitle));

                        // Find the window by title
                        HWND hTargetWnd = FindWindowA(NULL, szCleanTitle);
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
                        // In process mode, extract process name (remove architecture suffix)
                        char szProcessName[MAX_PATH] = { 0 };
                        ExtractProcessName(szSelectedText, szProcessName, sizeof(szProcessName));
                        SetDlgItemTextA(hDlg, IDC_PROCESS_NAME, szProcessName);

                        // Find the PID and update status
                        DWORD dwPID = FindProcessByName(szProcessName);
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

        case IDC_SETTINGS_BUTTON:
        {
            ShowSettingsView();
            return TRUE;
        }

        case IDC_OK:
        case IDCANCEL:
            // Close the frame dialog (parent)
            if (g_hFrameDialog)
                SendMessage(g_hFrameDialog, WM_CLOSE, 0, 0);
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
                    _snprintf(szStatus, sizeof(szStatus) - 1, "Injecting: %s (%i)", szProcessName, dwPID);
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
                    g_dwSelectedIndex = (DWORD)selIndex;

                    LRESULT textLen = SendMessage(hListBox, LB_GETTEXTLEN, selIndex, 0);
                    if (textLen <= 0 || textLen >= MAX_LISTBOX_TEXT_LEN)
                    {
                        MessageBoxA(hDlg, "Invalid window selection", g_szMsgBoxTitle, MB_OK);
                        return TRUE;
                    }

                    char* szWindowTitle = new char[textLen + 1];
                    SendMessage(hListBox, LB_GETTEXT, g_dwSelectedIndex, (LPARAM)szWindowTitle);

                    // Extract clean window title (remove [!] prefix and [x86]/[x64] suffix)
                    char szCleanTitle[512] = { 0 };
                    ExtractProcessName(szWindowTitle, szCleanTitle, sizeof(szCleanTitle));

                    HWND hTargetWnd = FindWindowA(NULL, szCleanTitle);
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

    // Check architecture compatibility
    ProcessArchitecture procArch = GetProcessArchitecture(dwProcessId);
    if (!IsArchitectureCompatible(g_DllArchitecture, procArch))
    {
        char szError[512];
        _snprintf(szError, sizeof(szError) - 1,
            "Architecture Mismatch!\n\n"
            "DLL Architecture: %s\n"
            "Process Architecture: %s\n\n"
            "Injection will fail. Please use a %s version of the DLL.",
            ArchitectureToString(g_DllArchitecture),
            ArchitectureToString(procArch),
            ArchitectureToString(procArch));
        szError[sizeof(szError) - 1] = '\0';
        MessageBoxA(hDlg, szError, "Architecture Mismatch", MB_OK | MB_ICONERROR);
        return FALSE;
    }

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
    // For x64 injector injecting into x86 process, we need to get LoadLibraryA from the target process's kernel32
    LPTHREAD_START_ROUTINE pfnLoadLibrary = NULL;

#ifdef _WIN64
    // Check if target process is 32-bit when we're 64-bit
    if (procArch == ARCH_X86)
    {
        // For WOW64 injection, we need to find LoadLibraryA in the target's kernel32
        // We'll use a different approach: enumerate modules in the target process
        HMODULE hKernel32 = NULL;
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, dwProcessId);
        if (hSnapshot != INVALID_HANDLE_VALUE)
        {
            MODULEENTRY32 me32 = { 0 };
            me32.dwSize = sizeof(MODULEENTRY32);

            if (Module32First(hSnapshot, &me32))
            {
                do
                {
                    if (_stricmp(me32.szModule, "kernel32.dll") == 0)
                    {
                        hKernel32 = me32.hModule;
                        break;
                    }
                } while (Module32Next(hSnapshot, &me32));
            }
            CloseHandle(hSnapshot);
        }

        if (hKernel32)
        {
            // For WOW64 injection, we need to find LoadLibraryA in the target's 32-bit kernel32
            // Since x64 process can't load x86 DLL, we'll map the file and parse exports
            char szSysWOW64Path[MAX_PATH];
            GetSystemWow64DirectoryA(szSysWOW64Path, MAX_PATH);
            strncat_s(szSysWOW64Path, MAX_PATH, "\\kernel32.dll", _TRUNCATE);

            // Map the 32-bit kernel32.dll file
            HANDLE hFile = CreateFileA(szSysWOW64Path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (hFile != INVALID_HANDLE_VALUE)
            {
                HANDLE hMapping = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
                if (hMapping)
                {
                    BYTE* pBase = (BYTE*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
                    if (pBase)
                    {
                        // Parse PE headers
                        IMAGE_DOS_HEADER* pDosHeader = (IMAGE_DOS_HEADER*)pBase;
                        if (pDosHeader->e_magic == IMAGE_DOS_SIGNATURE)
                        {
                            IMAGE_NT_HEADERS32* pNtHeaders = (IMAGE_NT_HEADERS32*)(pBase + pDosHeader->e_lfanew);
                            if (pNtHeaders->Signature == IMAGE_NT_SIGNATURE)
                            {
                                // Get export directory RVA
                                DWORD exportRVA = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
                                if (exportRVA != 0)
                                {
                                    // Convert RVA to file offset using section headers
                                    IMAGE_SECTION_HEADER* pSection = IMAGE_FIRST_SECTION(pNtHeaders);
                                    DWORD exportFileOffset = 0;

                                    for (int i = 0; i < pNtHeaders->FileHeader.NumberOfSections; i++)
                                    {
                                        if (exportRVA >= pSection[i].VirtualAddress &&
                                            exportRVA < pSection[i].VirtualAddress + pSection[i].Misc.VirtualSize)
                                        {
                                            exportFileOffset = pSection[i].PointerToRawData + (exportRVA - pSection[i].VirtualAddress);
                                            break;
                                        }
                                    }

                                    if (exportFileOffset != 0)
                                    {
                                        IMAGE_EXPORT_DIRECTORY* pExportDir = (IMAGE_EXPORT_DIRECTORY*)(pBase + exportFileOffset);

                                        // Convert export table RVAs to file offsets
                                        auto RvaToFileOffset = [&](DWORD rva) -> DWORD {
                                            for (int i = 0; i < pNtHeaders->FileHeader.NumberOfSections; i++)
                                            {
                                                if (rva >= pSection[i].VirtualAddress &&
                                                    rva < pSection[i].VirtualAddress + pSection[i].Misc.VirtualSize)
                                                {
                                                    return pSection[i].PointerToRawData + (rva - pSection[i].VirtualAddress);
                                                }
                                            }
                                            return 0;
                                        };

                                        DWORD functionsOffset = RvaToFileOffset(pExportDir->AddressOfFunctions);
                                        DWORD namesOffset = RvaToFileOffset(pExportDir->AddressOfNames);
                                        DWORD ordinalsOffset = RvaToFileOffset(pExportDir->AddressOfNameOrdinals);

                                        if (functionsOffset && namesOffset && ordinalsOffset)
                                        {
                                            DWORD* pFunctions = (DWORD*)(pBase + functionsOffset);
                                            DWORD* pNames = (DWORD*)(pBase + namesOffset);
                                            WORD* pOrdinals = (WORD*)(pBase + ordinalsOffset);

                                            // Search for LoadLibraryA
                                            for (DWORD i = 0; i < pExportDir->NumberOfNames; i++)
                                            {
                                                DWORD nameOffset = RvaToFileOffset(pNames[i]);
                                                if (nameOffset)
                                                {
                                                    char* szFuncName = (char*)(pBase + nameOffset);
                                                    if (strcmp(szFuncName, "LoadLibraryA") == 0)
                                                    {
                                                        DWORD funcRVA = pFunctions[pOrdinals[i]];
                                                        // Calculate address in target process (RVA is used directly for memory address)
                                                        pfnLoadLibrary = (LPTHREAD_START_ROUTINE)((DWORD_PTR)hKernel32 + funcRVA);
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        UnmapViewOfFile(pBase);
                    }
                    CloseHandle(hMapping);
                }
                CloseHandle(hFile);
            }

            if (!pfnLoadLibrary)
            {
                MessageBoxA(hDlg, "Could not resolve LoadLibraryA for WOW64 injection.", g_szMsgBoxTitle, MB_OK);
                VirtualFreeEx(hProcess, lpRemoteMem, 0, MEM_RELEASE);
                CloseHandle(hProcess);
                return FALSE;
            }
        }
        else
        {
            MessageBoxA(hDlg, "Could not find kernel32.dll in target process for WOW64 injection.", g_szMsgBoxTitle, MB_OK);
            VirtualFreeEx(hProcess, lpRemoteMem, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return FALSE;
        }
    }
    else
#endif
    {
        // Same architecture injection - use local LoadLibraryA address
        HMODULE hKernel32 = GetModuleHandleA("kernel32");
        pfnLoadLibrary = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryA");
    }

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
                // Only show if: architectures match AND process is accessible
                if (!IsArchitectureCompatible(g_DllArchitecture, arch) || !bCanOpen)
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
        // Only show if: architectures match AND process is accessible
        if (!IsArchitectureCompatible(g_DllArchitecture, arch) || !bCanOpen)
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

// Show about dialog
void ShowAboutDialog(HWND hDlg)
{
    const char* szAbout =
        "A stealthy Dll injector v1.2\n"
        "By: g3nuin3  Many thanks to Hunter!\n\n"
        "Shoutz:\n"
        "Luap, L.Spiro, st00ner, Fairlight, evobyte, Xan, moklop, catch22, SasukeHa\n"
        "ScOOp, Gunout, Revoked, Borna, kemicza, ILA, etc.. etc\n\n"
        "Recreated and updated by CyanideByte";

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

        // Detect DLL architecture
        g_DllArchitecture = GetDllArchitecture(g_szDllPath);

        // Check if 32-bit injector trying to load 64-bit DLL
        ProcessArchitecture injectorArch = GetInjectorArchitecture();
        if (injectorArch == ARCH_X86 && g_DllArchitecture == ARCH_X64)
        {
            MessageBoxA(hDlg, "Cannot load x64 DLL in 32-bit injector. Please use the 64-bit build.", g_szMsgBoxTitle, MB_OK);
            g_szDllPath[0] = '\0';  // Clear the DLL path
            g_DllArchitecture = ARCH_UNKNOWN;
            return;
        }

        // Display the filename with architecture (not full path) in the UI
        char szDllWithArch[1024 + 16] = { 0 };
        _snprintf(szDllWithArch, sizeof(szDllWithArch) - 1,
                  "%s [%s]", g_szDllFilename, ArchitectureToString(g_DllArchitecture));
        szDllWithArch[sizeof(szDllWithArch) - 1] = '\0';

        HWND hDllPath = GetDlgItem(hDlg, IDC_DLL_PATH);
        SetWindowTextA(hDllPath, szDllWithArch);

        // Refresh the process list to update color coding
        ApplySearchFilter(hDlg);
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

// Apply search filter to process list
void ApplySearchFilter(HWND hDlg)
{
    HWND hListBox = GetDlgItem(hDlg, IDC_PROCESS_LIST);
    SendMessage(hListBox, LB_RESETCONTENT, 0, 0);

    if (g_bWindowMode)
    {
        EnumWindows(EnumWindowsProc, (LPARAM)hDlg);
    }
    else
    {
        EnumerateProcesses(hDlg);
    }
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

// Apply dark mode to dialog
void ApplyDarkMode(HWND hDlg, BOOL bEnable)
{
    // Set dark mode for title bar (Windows 10 1809+)
    BOOL useDarkMode = bEnable;
    DwmSetWindowAttribute(hDlg, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

    // Workaround: Briefly hide/show to skip the fade animation
    // This makes the transition appear instant
    ShowWindow(hDlg, SW_HIDE);

    // Force immediate redraw of the non-client area (title bar)
    SetWindowPos(hDlg, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    ShowWindow(hDlg, SW_SHOW);

    // Toggle BS_OWNERDRAW style on all buttons based on dark mode
    const int buttonIds[] = {
        IDC_OK, IDC_LOAD_DLL, IDC_ABOUT, IDC_REFRESH,
        IDC_INJECT, IDC_USE_LAST, IDC_USE, IDC_EJECT,
        IDC_WINDOW_BUTTON, IDC_SETTINGS_BUTTON
    };

    for (int i = 0; i < sizeof(buttonIds) / sizeof(buttonIds[0]); i++)
    {
        HWND hButton = GetDlgItem(hDlg, buttonIds[i]);
        if (hButton)
        {
            LONG_PTR style = GetWindowLongPtr(hButton, GWL_STYLE);
            if (bEnable)
            {
                // Enable owner draw in dark mode
                style |= BS_OWNERDRAW;
            }
            else
            {
                // Disable owner draw in light mode (use native rendering)
                style &= ~BS_OWNERDRAW;
            }
            SetWindowLongPtr(hButton, GWL_STYLE, style);

            // Force button to redraw with new style
            InvalidateRect(hButton, NULL, TRUE);
        }
    }

    // Force redraw of all controls
    InvalidateRect(hDlg, NULL, TRUE);

    // Refresh process list to update colors
    ApplySearchFilter(hDlg);
}

// Settings dialog procedure
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        // Set the dark mode checkbox state
        CheckDlgButton(hDlg, IDC_SETTINGS_DARK_MODE, g_bDarkMode ? BST_CHECKED : BST_UNCHECKED);

        // Apply dark mode if enabled
        if (g_bDarkMode)
        {
            ApplyDarkMode(hDlg, TRUE);
        }

        return TRUE;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
    {
        if (g_bDarkMode)
        {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(220, 220, 220));
            SetBkColor(hdcStatic, RGB(30, 30, 30));
            return (INT_PTR)g_hDarkBrush;
        }
        break;
    }

    case WM_COMMAND:
    {
        WORD wmId = LOWORD(wParam);
        WORD wmEvent = HIWORD(wParam);

        // Handle dark mode checkbox
        if (wmId == IDC_SETTINGS_DARK_MODE && wmEvent == BN_CLICKED)
        {
            g_bDarkMode = (IsDlgButtonChecked(hDlg, IDC_SETTINGS_DARK_MODE) == BST_CHECKED);

            // Save dark mode setting to INI file
            SaveDarkModeSetting(g_bDarkMode);

            // Apply dark mode to frame window title bar
            if (g_hFrameDialog)
            {
                BOOL useDarkMode = g_bDarkMode;
                DwmSetWindowAttribute(g_hFrameDialog, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

                // Workaround: Briefly hide/show to update title bar without animation
                ShowWindow(g_hFrameDialog, SW_HIDE);
                SetWindowPos(g_hFrameDialog, NULL, 0, 0, 0, 0,
                            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
                ShowWindow(g_hFrameDialog, SW_SHOW);
            }

            // Force settings dialog redraw to update colors
            RedrawWindow(hDlg, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);

            return TRUE;
        }

        switch (wmId)
        {
        case IDC_SETTINGS_OK:
        case IDCANCEL:
            ShowMainView();
            return TRUE;
        }
        break;
    }
    }

    return FALSE;
}