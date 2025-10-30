/*
 * InjecTOR - Process Module
 * Process and window enumeration, architecture detection, and helper functions
 */

#ifndef PROCESS_H
#define PROCESS_H

#include "common.h"

// Process enumeration
void EnumerateProcesses(HWND hDlg);
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
DWORD GetSelectedProcessPID(HWND hDlg, HWND hListBox, PROCESSENTRY32* pe32, HANDLE hSnapshot);

// Architecture detection
ProcessArchitecture GetProcessArchitecture(DWORD dwProcessId);
ProcessArchitecture GetDllArchitecture(const char* dllPath);
ProcessArchitecture GetInjectorArchitecture();
const char* ArchitectureToString(ProcessArchitecture arch);
BOOL IsArchitectureCompatible(ProcessArchitecture dllArch, ProcessArchitecture procArch);

// Process utilities
DWORD FindProcessByName(const char* processName);
BOOL IsRunningAsAdmin();
BOOL CanOpenProcess(DWORD dwProcessId);
BOOL DoesProcessExist(DWORD dwProcessId);

// String utilities
void ExtractFilenameFromPath(const char* fullPath, char* filename);
void ExtractProcessName(const char* listboxText, char* processName, size_t maxLen);

#endif // PROCESS_H
