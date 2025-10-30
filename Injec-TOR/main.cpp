/*
 * InjecTOR - A stealthy DLL injector v1.2
 * By: g3nuin3
 * Recreated and updated by CyanideByte
 *
 * Recreated from reverse engineering
 */

#include "common.h"
#include "ui.h"

// Global variables (definitions)
HINSTANCE g_hInstance = NULL;
HWND g_hMainDialog = NULL;
HWND g_hSettingsDialog = NULL;
HWND g_hFrameDialog = NULL;
char g_szDllPath[MAX_PATH] = { 0 };
char g_szDllFilename[1024] = { 0 };
volatile BOOL g_bWatcherActive = FALSE;
volatile BOOL g_bWatcherStop = FALSE;
BOOL g_bWindowMode = FALSE;
DWORD g_dwSelectedIndex = 0;
ProcessArchitecture g_DllArchitecture = ARCH_UNKNOWN;
char g_szSearchFilter[256] = { 0 };
BOOL g_bCompatibleOnly = FALSE;
BOOL g_bDarkMode = FALSE;
HBRUSH g_hDarkBrush = NULL;
HBRUSH g_hLightBrush = NULL;
HBRUSH g_hButtonBrush = NULL;

// String constants
const char* g_szProcesses = "Processes";
const char* g_szWindows = "Windows";
const char* g_szWindow = "Window";
const char* g_szProcess = "Process";
const char* g_szInjectionFailed = "Injection Failed..Falling Back..";
const char* g_szMsgBoxTitle = "InjecTOR";

// WinMain entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    g_hInstance = hInstance;

    DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_FRAME_DIALOG), NULL, FrameDialogProc, 0);

    return 0;
}
