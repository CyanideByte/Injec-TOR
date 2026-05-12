/*
 * InjecTOR - UI Module Implementation
 * Dialog procedures, UI management, and theming
 */

#include "ui.h"
#include "process.h"
#include "injection.h"
#include "settings.h"

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

        // Load compatible only setting from INI file
        LoadCompatibleOnlySetting();

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

        // Set the compatible only checkbox state from loaded setting
        CheckDlgButton(hDlg, IDC_COMPATIBLE_ONLY, g_bCompatibleOnly ? BST_CHECKED : BST_UNCHECKED);

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

            // Save compatible only setting to INI file
            SaveCompatibleOnlySetting(g_bCompatibleOnly);

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

    MessageBoxA(hDlg, szAbout, g_szMsgBoxTitle, MB_OK);
}
