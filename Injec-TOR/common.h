/*
 * InjecTOR - Common definitions and declarations
 * Shared includes, constants, enums, and global variable declarations
 */

#ifndef COMMON_H
#define COMMON_H

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

// Global variables (extern declarations)
extern HINSTANCE g_hInstance;
extern HWND g_hMainDialog;
extern HWND g_hSettingsDialog;
extern HWND g_hFrameDialog;
extern char g_szDllPath[MAX_PATH];
extern char g_szDllFilename[1024];
extern volatile BOOL g_bWatcherActive;
extern volatile BOOL g_bWatcherStop;
extern BOOL g_bWindowMode;
extern DWORD g_dwSelectedIndex;
extern ProcessArchitecture g_DllArchitecture;
extern char g_szSearchFilter[256];
extern BOOL g_bCompatibleOnly;
extern BOOL g_bDarkMode;
extern HBRUSH g_hDarkBrush;
extern HBRUSH g_hLightBrush;
extern HBRUSH g_hButtonBrush;

// String constants
extern const char* g_szProcesses;
extern const char* g_szWindows;
extern const char* g_szWindow;
extern const char* g_szProcess;
extern const char* g_szInjectionFailed;
extern const char* g_szMsgBoxTitle;

#endif // COMMON_H
