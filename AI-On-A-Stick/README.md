# AI-On-A-Stick

Portable local AI runtime with a static web UI, native C++ backend, bundled `llama.cpp` runtimes, and optional portable data storage on the project root or USB bundle.

## Windows Build Guide

This project builds with CMake and C++20. On Windows, the application requires `SQLite3` at build time and can now auto-build `llama-server.exe` during the normal CMake preset flow.

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

The Windows presets now fetch a pinned `llama.cpp` revision and place the built runtime here:

```text
bin\windows-x64\llama-server.exe
```

If you disable the auto-build, you must provide that file manually. The configured runtime path is defined in:

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

This repo now includes Windows CMake presets. On the first configure, CMake downloads the pinned `llama.cpp` source and wires `llama-server` into the build automatically.

### Visual Studio 2022

```powershell
cmake --preset windows-vs2022-x64
cmake --build --preset build-windows-vs2022-x64-release
```

App output:

```text
build\windows-vs2022-x64\Release\ai_on_a_stick.exe
```

### Ninja

```powershell
cmake --preset windows-ninja-x64
cmake --build --preset build-windows-ninja-x64
```

App output:

```text
build\windows-ninja-x64\ai_on_a_stick.exe
```

### Manual Configure Alternative

If you do not want to use presets:

```powershell
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" -DAI_ON_A_STICK_BUILD_LLAMA_SERVER=ON
cmake --build build --config Release
```

If you want a CUDA-enabled `llama-server`, add:

```powershell
-DAI_ON_A_STICK_LLAMA_CUDA=ON
```

## After Build

The app binary is expected at one of these preset outputs:

```text
build\windows-vs2022-x64\Release\ai_on_a_stick.exe
build\windows-ninja-x64\ai_on_a_stick.exe
```

The bundled runtime should be generated here:

```text
bin\windows-x64\llama-server.exe
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
7. Build with the Windows preset
8. Confirm `bin\windows-x64\llama-server.exe` was generated
9. Put models in `models\...`
10. Run `launch\start-windows.bat`



-error :)
H:\ai-on-a-stick\AI-On-A-Stick\src\database.cpp(102,13): error C2664: 'void Database::insert_file(const MachineProfile &,const std::string &,uint64_t,int64_t)': cannot convert argument 2 from 'const std::filesys
tem::path' to 'const std::string &' [H:\ai-on-a-stick\AI-On-A-Stick\build\ai_on_a_stick.vcxproj]
      H:\ai-on-a-stick\AI-On-A-Stick\src\database.cpp(102,44):
      Reason: cannot convert from 'const std::filesystem::path' to 'const std::string'
      H:\ai-on-a-stick\AI-On-A-Stick\src\database.cpp(102,44):
      No user-defined-conversion operator available that can perform this conversion, or the operator cannot be called
      H:\ai-on-a-stick\AI-On-A-Stick\src\database.h(33,10):
      see declaration of 'Database::insert_file'
      H:\ai-on-a-stick\AI-On-A-Stick\src\database.cpp(102,13):
      while trying to match the argument list '(const MachineProfile, const std::filesystem::path, const uintmax_t, const int64_t)'
