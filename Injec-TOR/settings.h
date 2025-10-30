/*
 * InjecTOR - Settings Module
 * Handles INI file persistence for application settings
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include "common.h"

// Save/load last injection settings
void SaveLastInjection(const char* dllPath, const char* processName);
void LoadLastInjection(HWND hDlg);

// Save/load dark mode setting
void SaveDarkModeSetting(BOOL bDarkMode);
void LoadDarkModeSetting();

// Save/load compatible only filter setting
void SaveCompatibleOnlySetting(BOOL bCompatibleOnly);
void LoadCompatibleOnlySetting();

#endif // SETTINGS_H
