# AI-On-A-Stick

Portable local AI runtime with a static web UI, native C++ backend, bundled `llama.cpp` runtimes, and optional portable data storage on the project root or USB bundle.

## Windows Build Guide

This project builds with CMake and C++20. On Windows, the application currently requires `SQLite3` at build time and a prebuilt `llama-server.exe` at runtime.

### What To Install

Install these on the Windows machine:

- Visual Studio 2022 with `Desktop development with C++`
- Git
- CMake
- vcpkg
- `sqlite3:x64-windows` through vcpkg

Recommended Visual Studio components:

- MSVC v143
- Windows 10 or 11 SDK
- C++ CMake tools for Windows

### Runtime File Required

The application does not build `llama.cpp` for you. You must provide:

```text
bin\windows-x64\llama-server.exe
```

The configured runtime path is defined in:

```text
config\runtime.ini
```

## Windows Build Setup

### 1. Install vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg $env:USERPROFILE\vcpkg
& $env:USERPROFILE\vcpkg\bootstrap-vcpkg.bat
```

### 2. Install SQLite3

```powershell
& $env:USERPROFILE\vcpkg\vcpkg install sqlite3:x64-windows
```

### 3. Set VCPKG_ROOT

```powershell
$env:VCPKG_ROOT="$env:USERPROFILE\vcpkg"
```

## Build On Windows

This repo now includes Windows CMake presets.

### Visual Studio 2022

```powershell
cmake --preset windows-vs2022-x64
cmake --build --preset build-windows-vs2022-x64-release
```

### Ninja

```powershell
cmake --preset windows-ninja-x64
cmake --build --preset build-windows-ninja-x64
```

### Manual Configure Alternative

If you do not want to use presets:

```powershell
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Release
```

## After Build

The app binary is expected at:

```text
build\Release\ai_on_a_stick.exe
```

To run it successfully, you also need:

```text
bin\windows-x64\llama-server.exe
models\small\model.gguf
models\coder\model.gguf
config\
ui\
data\
```

## Run On Windows

### Preferred

Run from the repo root:

```powershell
launch\start-windows.bat
```

### If You Already Built

Run from the repo root:

```powershell
build\Release\ai_on_a_stick.exe
```

Do not run it from inside `build\Release` by itself. The app expects relative paths for:

- `ui\`
- `config\`
- `models\`
- `bin\`

### Packaged Layout Option

If you want to use the packaged-style Windows launcher:

```powershell
copy build\Release\ai_on_a_stick.exe ai_on_a_stick.exe
launch\run-windows.bat
```

## Common Problems

### SQLite3 Not Found

Typical errors:

- `Could NOT find SQLite3`
- missing `sqlite3.h`
- missing `SQLite3::SQLite3`

Fix:

1. Install SQLite through vcpkg
2. Set `VCPKG_ROOT`
3. Delete the old `build\` folder
4. Configure again with a preset or toolchain file

Commands:

```powershell
rmdir /s /q build
cmake --preset windows-vs2022-x64
cmake --build --preset build-windows-vs2022-x64-release
```

### Source Does Not Match

If you copied the repo or `build\` directory from another computer, CMake may fail with a message like:

- `does not match the source`
- `The source directory ... does not match the source directory used to generate cache`

Cause:

- `build\CMakeCache.txt` stores absolute paths from the original machine

Fix:

```powershell
rmdir /s /q build
cmake --preset windows-vs2022-x64
cmake --build --preset build-windows-vs2022-x64-release
```

Do not move these between machines:

- `build\`
- `CMakeCache.txt`
- `CMakeFiles\`

Only move source files or final packaged outputs.

### UI Starts But No Model Loads

Check that these files exist:

```text
bin\windows-x64\llama-server.exe
models\small\model.gguf
models\coder\model.gguf
```

Also confirm this config entry still points to the Windows runtime:

```ini
binary_windows_x64=bin/windows-x64/llama-server.exe
```

## Expected Startup Output

When it launches correctly, the terminal should print something like:

```text
UI: http://127.0.0.1:8090
General runtime: http://127.0.0.1:8080
Coder runtime: http://127.0.0.1:8081
```

## Recommended Windows Flow

1. Install Visual Studio 2022 C++ tools
2. Install Git
3. Install CMake
4. Install vcpkg
5. Install `sqlite3:x64-windows`
6. Set `VCPKG_ROOT`
7. Put `llama-server.exe` in `bin\windows-x64\`
8. Put models in `models\...`
9. Build with the Windows preset
10. Run `launch\start-windows.bat`
