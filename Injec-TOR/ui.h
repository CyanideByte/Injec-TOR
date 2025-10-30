/*
 * InjecTOR - UI Module
 * Dialog procedures, UI management, and theming
 */

#ifndef UI_H
#define UI_H

#include "common.h"

// Dialog procedures
INT_PTR CALLBACK FrameDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK MainDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// View management
void ShowMainView();
void ShowSettingsView();

// UI utilities
void BrowseForDLL(HWND hDlg);
void SetStatusText(HWND hDlg, const char* format, ...);
void ApplySearchFilter(HWND hDlg);
void ApplyDarkMode(HWND hDlg, BOOL bEnable);
void ShowAboutDialog(HWND hDlg);

#endif // UI_H
