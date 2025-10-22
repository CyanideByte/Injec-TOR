# Injec-TOR

A reverse-engineered recreation of the classic Injec-TOR DLL injector from the early days of GunZ: The Duel hacking scene.

## Overview

This project is a faithful reconstruction of the original Injec-TOR v1.1+ by g3nuin3, recreated through reverse engineering to preserve a piece of gaming history from the GunZ: The Duel modding community. The original tool was widely used in the early hacking scene and represents an important part of that era's nostalgia.

## Historical Context

Injec-TOR was a popular DLL injection tool used by the GunZ: The Duel community in the game's early days. This recreation aims to preserve the original's functionality and user interface, serving as both a tribute to the original work and a reverse engineering educational project.

## Features

- **Process Injection**: Browse and inject DLLs into running processes
- **Window Mode**: Switch between process list and window list views
- **Process Watcher**: Automatically inject DLL when a target process starts
- **Classic GUI**: Faithful recreation of the original dialog-based interface
- **Real-time Process List**: Refresh and view all running processes on the system

## Reverse Engineering Notes

This version was recreated by analyzing the original binary and reconstructing the functionality. Notable implementation details include:

- Uses classic CreateRemoteThread injection technique
- ANSI API usage matching the original compilation
- Original buffer sizes and string constants preserved (e.g., 1024-byte DLL filename buffer)
- Default DLL name reference preserved: "GunzFuckV4.dll"
- Memory protection wrapper matching original behavior

## How to Use

### Manual Injection

1. Click "Load DLL" and browse to select your DLL file
2. Select a target process from the list (or switch to Window mode for window-based targeting)
3. Click "Inject DLL"
4. Check the status message for confirmation

### Process Watcher

The Process Watcher feature monitors for a specific process to start and automatically injects your DLL when it appears:

1. Load your desired DLL using "Load DLL"
2. Enter the target process name (with extension, e.g., `gunz.exe`) in the Process Name field
3. Click the "Use Watch" checkbox to activate the watcher
4. The injector will monitor for the process and inject automatically when found

### Toggle Between Process/Window Mode

- Click the "Window"/"Process" button to switch between viewing:
  - **Process Mode**: Lists all running processes
  - **Window Mode**: Lists all visible windows

## Technical Details

### Injection Method

The tool uses the CreateRemoteThread injection technique:
- Opens target process with `PROCESS_ALL_ACCESS`
- Allocates memory in target process using `VirtualAllocEx`
- Writes DLL path to allocated memory
- Creates remote thread pointing to `LoadLibraryA`
- Waits for injection to complete

### Code Features

- Custom `WriteMemoryWrapper` with VirtualProtect handling
- Process enumeration via Toolhelp32 snapshots
- Window enumeration for window-based injection
- Thread-based process monitoring

## Building from Source

### Requirements

- Windows OS
- Visual Studio (with C++ support)
- Windows SDK

### Build Instructions

1. Open `Injec-TOR.sln` in Visual Studio
2. Set build configuration (Debug/Release)
3. Build Solution (Ctrl+Shift+B)
4. Executable will be in `Debug` or `Release` folder

## System Requirements

- Windows XP or later
- Administrator privileges may be required for certain processes

## Credits

**Original Author**: g3nuin3 (with thanks to Hunter)

**Original Shoutouts**: Luap, L.Spiro, st00ner, Fairlight, evobyte, Xan, moklop, catch22, SasukeHa, ScOOp, Gunout, Revoked, Borna, kemicza, ILA, and others from the GunZ community

**This Recreation**: Reverse engineered by CyanideByte to preserve the original tool's functionality

## Purpose

This project serves multiple purposes:
- **Nostalgia**: Preserving a piece of GunZ: The Duel hacking history
- **Education**: Demonstrating reverse engineering techniques
- **Research**: Understanding classic DLL injection methods

## Disclaimer

This software is provided for educational and research purposes only. The recreator is not responsible for any misuse or damage caused by this tool. This is a historical preservation project. Always ensure you have explicit permission before injecting code into processes you do not own. Unauthorized access to computer systems is illegal.

---

**A tribute to the early days of GunZ: The Duel hacking and the community that made it memorable.**
