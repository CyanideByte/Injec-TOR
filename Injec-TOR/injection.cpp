/*
 * InjecTOR - Injection Module Implementation
 * Core DLL injection functionality and process watcher thread
 */

#include "injection.h"
#include "process.h"
#include "settings.h"

// Forward declaration from ui.h
extern void SetStatusText(HWND hDlg, const char* format, ...);

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

        // Check if process still exists to determine the error message
        if (DoesProcessExist(dwProcessId))
        {
            // Process exists but we can't open it - likely needs elevation
            _snprintf(szError, sizeof(szError) - 1,
                "Failed to open process (PID: %i).\nThe process requires elevated privileges.",
                dwProcessId);
        }
        else
        {
            // Process doesn't exist - it has exited
            _snprintf(szError, sizeof(szError) - 1,
                "Failed to open process (PID: %i).\nThe process may have exited.",
                dwProcessId);
        }

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
