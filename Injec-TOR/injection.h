/*
 * InjecTOR - Injection Module
 * Core DLL injection functionality and process watcher thread
 */

#ifndef INJECTION_H
#define INJECTION_H

#include "common.h"

// Injection functions
BOOL InjectDLL(HWND hDlg, DWORD dwProcessId, SIZE_T dllPathLen, const char* processName);

// Process watcher thread
DWORD WINAPI ProcessWatcherThread(LPVOID lpParameter);
void StartProcessWatcher(HWND hDlg);
void StopProcessWatcher(HWND hDlg);

#endif // INJECTION_H
