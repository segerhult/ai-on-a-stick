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

## error ;(
 cmake --preset windows-vs2022-x64
>> cmake --build --preset build-windows-vs2022-x64-release
-- Selecting Windows SDK version 10.0.26100.0 to target Windows 10.0.26200.
-- The C compiler identification is MSVC 19.44.35228.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe - skipped
-- Detecting C compile features
-- Detecting C compile features - done
CMAKE_BUILD_TYPE=
-- Found Git: C:/Program Files/Git/cmd/git.exe (found version "2.55.0.windows.2")
-- The ASM compiler identification is MSVC
CMake Warning (dev) at C:/Program Files/CMake/share/cmake-4.3/Modules/CMakeDetermineASMCompiler.cmake:234 (message):
  Policy CMP194 is not set: MSVC is not an assembler for language ASM.  Run
  "cmake --help-policy CMP194" for policy details.  Use the cmake_policy
  command to set the policy and suppress this warning.
Call Stack (most recent call first):
  build/windows-vs2022-x64/_deps/llama_cpp-src/ggml/CMakeLists.txt:3 (project)
This warning is for project developers.  Use -Wno-dev to suppress it.

-- Found assembler: C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe
-- Warning: ccache not found - consider installing it for faster compilation or disable this warning with GGML_CCACHE=OFF
-- CMAKE_SYSTEM_PROCESSOR: AMD64
-- CMAKE_GENERATOR_PLATFORM: x64
-- GGML_SYSTEM_ARCH: x86
-- Found OpenMP_C: -openmp (found version "2.0")
-- Found OpenMP_CXX: -openmp (found version "2.0")
-- Found OpenMP: TRUE (found version "2.0")
-- Including CPU backend
-- x86 detected
-- Performing Test HAS_AVX_1
-- Performing Test HAS_AVX_1 - Success
-- Performing Test HAS_AVX2_1
-- Performing Test HAS_AVX2_1 - Success
-- Performing Test HAS_FMA_1
-- Performing Test HAS_FMA_1 - Success
-- Performing Test HAS_AVX512_1
-- Performing Test HAS_AVX512_1 - Success
-- Adding CPU backend variant ggml-cpu: /arch:AVX512 GGML_AVX512
-- ggml version: 0.15.3
-- ggml commit:  20a04b220
CMake Error at CMakeLists.txt:88 (get_target_property):
  get_target_property() called with non-existent target "llama-server".


-- llama.cpp auto-build enabled from https://github.com/ggml-org/llama.cpp.git@20a04b22063020cd0f29b7781f5352d7a6abf786
-- llama-server bundle destination: H:/ai-on-a-stick/ai-on-a-stick/bin/windows-x64
-- Windows build configured for AI-On-A-Stick
-- Using toolchain file: C:\Users\Martin Jonsson\vcpkg/scripts/buildsystems/vcpkg.cmake
-- Configuring incomplete, errors occurred!
CMake is re-running because H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/CMakeFiles/generate.stamp is out-of-date.
  the file 'H:/ai-on-a-stick/ai-on-a-stick/CMakeLists.txt'
  is newer than 'H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/CMakeFiles/generate.stamp.depend'
  result='-1'
-- Selecting Windows SDK version 10.0.26100.0 to target Windows 10.0.26200.
CMAKE_BUILD_TYPE=
-- Warning: ccache not found - consider installing it for faster compilation or disable this warning with GGML_CCACHE=OFF
-- CMAKE_SYSTEM_PROCESSOR: AMD64
-- CMAKE_GENERATOR_PLATFORM: x64
-- GGML_SYSTEM_ARCH: x86
-- Including CPU backend
-- x86 detected
-- Adding CPU backend variant ggml-cpu: /arch:AVX512 GGML_AVX512
-- ggml version: 0.15.3
-- ggml commit:  20a04b220
CMake Error at CMakeLists.txt:88 (get_target_property):
  get_target_property() called with non-existent target "llama-server".


-- llama.cpp auto-build enabled from https://github.com/ggml-org/llama.cpp.git@20a04b22063020cd0f29b7781f5352d7a6abf786
-- llama-server bundle destination: H:/ai-on-a-stick/ai-on-a-stick/bin/windows-x64
-- Windows build configured for AI-On-A-Stick
-- Using toolchain file: C:\Users\Martin Jonsson\vcpkg/scripts/buildsystems/vcpkg.cmake
-- Configuring incomplete, errors occurred!
CMake Configure step failed.  Build files cannot be regenerated correctly.




## Error :(:(:(

cmake --preset windows-vs2022-x64
>> cmake --build --preset build-windows-vs2022-x64-release
-- Selecting Windows SDK version 10.0.26100.0 to target Windows 10.0.26200.
CMAKE_BUILD_TYPE=
-- Warning: ccache not found - consider installing it for faster compilation or disable this warning with GGML_CCACHE=OFF
-- CMAKE_SYSTEM_PROCESSOR: AMD64
-- CMAKE_GENERATOR_PLATFORM: x64
-- GGML_SYSTEM_ARCH: x86
-- Including CPU backend
-- x86 detected
-- Adding CPU backend variant ggml-cpu: /arch:AVX512 GGML_AVX512
-- ggml version: 0.15.3
-- ggml commit:  20a04b220
-- llama.cpp auto-build enabled from https://github.com/ggml-org/llama.cpp.git@20a04b22063020cd0f29b7781f5352d7a6abf786
-- llama-server bundle destination: H:/ai-on-a-stick/ai-on-a-stick/bin/windows-x64
-- Windows build configured for AI-On-A-Stick
-- Using toolchain file: C:\Users\Martin Jonsson\vcpkg/scripts/buildsystems/vcpkg.cmake
-- Configuring done (1.3s)
-- Generating done (0.4s)
-- Build files have been written to: H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64
MSBuild version 17.14.40+3e7442088 for .NET Framework

  1>Checking Build System
  Building Custom Rule H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_cpp-src/vendor/cpp-httplib/CMakeLists.txt
cl : command line  warning D9025: overriding '/W1' with '/w' [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\vendor\cpp-httplib\cpp-httplib.vcxproj]
  httplib.cpp
  cpp-httplib.vcxproj -> H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\vendor\cpp-httplib\Release\cpp-httplib.lib
  Building Custom Rule H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_cpp-src/ggml/src/CMakeLists.txt
  ggml.c
  ggml.cpp
  ggml-alloc.c
  ggml-quants.c
  Generating Code...
  ggml-backend.cpp
  ggml-backend-meta.cpp
  ggml-opt.cpp
  ggml-threading.cpp
  gguf.cpp
  Generating Code...
     Creating library H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_cpp-build/ggml/src/Release/ggml-base.lib and object H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_c
  pp-build/ggml/src/Release/ggml-base.exp
  ggml-base.vcxproj -> H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\bin\Release\ggml-base.dll
  H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/bin/Release/ggml-base.dll: message: deploying dependencies
  Building Custom Rule H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_cpp-src/ggml/src/CMakeLists.txt
  ggml-cpu.c
  ggml-cpu.cpp
  repack.cpp
  hbm.cpp
  traits.cpp
  amx.cpp
  mmq.cpp
  binary-ops.cpp
  unary-ops.cpp
  vec.cpp
  ops.cpp
  sgemm.cpp
  Generating Code...
  quants.c
  quants.c
  repack.cpp
     Creating library H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_cpp-build/ggml/src/Release/ggml-cpu.lib and object H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_cp
  p-build/ggml/src/Release/ggml-cpu.exp
  ggml-cpu.vcxproj -> H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\bin\Release\ggml-cpu.dll
  H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/bin/Release/ggml-cpu.dll: message: deploying dependencies
  H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/bin/Release\ggml-base.dll: message: deploying dependencies
  Building Custom Rule H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_cpp-src/ggml/src/CMakeLists.txt
  ggml-backend-dl.cpp
  ggml-backend-reg.cpp
  Generating Code...
     Creating library H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_cpp-build/ggml/src/Release/ggml.lib and object H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_cpp-bu
  ild/ggml/src/Release/ggml.exp
  ggml.vcxproj -> H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\bin\Release\ggml.dll
  H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/bin/Release/ggml.dll: message: deploying dependencies
  H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/bin/Release\ggml-cpu.dll: message: deploying dependencies
  H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/bin/Release\ggml-base.dll: message: deploying dependencies
  Building Custom Rule H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_cpp-src/src/CMakeLists.txt
  llama.cpp
  llama-adapter.cpp
  llama-arch.cpp
  llama-batch.cpp
  llama-chat.cpp
H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(180,16): error C2664: 'bool llm_chat_detect_template::<lambda_1>::operator ()(const char *) const': cannot convert a
rgument 1 from 'const char8_t [9]' to 'const char *' [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
      H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(180,30):
      Types pointed to are unrelated; conversion requires reinterpret_cast, C-style cast or parenthesized function-style cast
      H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(96,65):
      see declaration of 'llm_chat_detect_template::<lambda_1>::operator ()'
      H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(180,16):
      while trying to match the argument list '(const char8_t [9])'

H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(185,16): error C2664: 'bool llm_chat_detect_template::<lambda_1>::operator ()(const char *) const': cannot convert a
rgument 1 from 'const char8_t [18]' to 'const char *' [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
      H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(185,30):
      Types pointed to are unrelated; conversion requires reinterpret_cast, C-style cast or parenthesized function-style cast
      H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(96,65):
      see declaration of 'llm_chat_detect_template::<lambda_1>::operator ()'
      H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(185,16):
      while trying to match the argument list '(const char8_t [18])'

H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(185,59): error C2664: 'bool llm_chat_detect_template::<lambda_1>::operator ()(const char *) const': cannot convert a
rgument 1 from 'const char8_t [13]' to 'const char *' [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
      H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(185,73):
      Types pointed to are unrelated; conversion requires reinterpret_cast, C-style cast or parenthesized function-style cast
      H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(96,65):
      see declaration of 'llm_chat_detect_template::<lambda_1>::operator ()'
      H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(185,59):
      while trying to match the argument list '(const char8_t [13])'

H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(185,97): error C2664: 'bool llm_chat_detect_template::<lambda_1>::operator ()(const char *) const': cannot convert a
rgument 1 from 'const char8_t [28]' to 'const char *' [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
      H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(185,111):
      Types pointed to are unrelated; conversion requires reinterpret_cast, C-style cast or parenthesized function-style cast
      H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(96,65):
      see declaration of 'llm_chat_detect_template::<lambda_1>::operator ()'
      H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(185,97):
      while trying to match the argument list '(const char8_t [28])'

H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(234,16): error C2664: 'bool llm_chat_detect_template::<lambda_1>::operator ()(const char *) const': cannot convert a
rgument 1 from 'const char8_t [29]' to 'const char *' [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
      H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(234,30):
      Types pointed to are unrelated; conversion requires reinterpret_cast, C-style cast or parenthesized function-style cast
      H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(96,65):
      see declaration of 'llm_chat_detect_template::<lambda_1>::operator ()'
      H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(234,16):
      while trying to match the argument list '(const char8_t [29])'

H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(526,20): error C2280: 'std::basic_ostream<char,std::char_traits<char>> &std::operator <<<std::char_traits<char>>(std
::basic_ostream<char,std::char_traits<char>> &,const char8_t *)': attempting to reference a deleted function [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
      C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\__msvc_ostream.hpp(955,31):
      see declaration of 'std::operator <<'

H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(526,20): error C2088: built-in operator '<<' cannot be applied to an operand of type 'std::stringstream' [H:\ai-on-a
-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(542,57): error C2280: 'std::basic_ostream<char,std::char_traits<char>> &std::operator <<<std::char_traits<char>>(std
::basic_ostream<char,std::char_traits<char>> &,const char8_t *)': attempting to reference a deleted function [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
      C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\__msvc_ostream.hpp(955,31):
      see declaration of 'std::operator <<'

H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(542,57): error C2088: built-in operator '<<' cannot be applied to an operand of type 'std::basic_ostream<char,std::c
har_traits<char>>' [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(555,20): error C2280: 'std::basic_ostream<char,std::char_traits<char>> &std::operator <<<std::char_traits<char>>(std
::basic_ostream<char,std::char_traits<char>> &,const char8_t *)': attempting to reference a deleted function [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
      C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\__msvc_ostream.hpp(955,31):
      see declaration of 'std::operator <<'

H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(555,20): error C2088: built-in operator '<<' cannot be applied to an operand of type 'std::stringstream' [H:\ai-on-a
-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(557,20): error C2280: 'std::basic_ostream<char,std::char_traits<char>> &std::operator <<<std::char_traits<char>>(std
::basic_ostream<char,std::char_traits<char>> &,const char8_t *)': attempting to reference a deleted function [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
      C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\__msvc_ostream.hpp(955,31):
      see declaration of 'std::operator <<'

H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(557,20): error C2088: built-in operator '<<' cannot be applied to an operand of type 'std::stringstream' [H:\ai-on-a
-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(561,16): error C2280: 'std::basic_ostream<char,std::char_traits<char>> &std::operator <<<std::char_traits<char>>(std
::basic_ostream<char,std::char_traits<char>> &,const char8_t *)': attempting to reference a deleted function [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
      C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\__msvc_ostream.hpp(955,31):
      see declaration of 'std::operator <<'

H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-chat.cpp(561,16): error C2088: built-in operator '<<' cannot be applied to an operand of type 'std::stringstream' [H:\ai-on-a
-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
  llama-context.cpp
  llama-cparams.cpp
  llama-grammar.cpp
  llama-graph.cpp
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\memory(3630,56): warning C4244: 'argument': conversion from 'const int64_t' to 'uint32_t', possible loss of data [H:\ai-o
n-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
  (compiling source file '../../llama_cpp-src/src/llama-graph.cpp')
      C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\memory(3630,56):
      the template instantiation context (the oldest one first) is
          H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-graph.cpp(2268,21):
          see reference to function template instantiation 'std::unique_ptr<llm_graph_input_out_ids,std::default_delete<llm_graph_input_out_ids>> std::make_unique<llm_graph_input_out_ids,const llama_hparams&,con
  st llama_cparams&,const int64_t&,0>(const llama_hparams &,const llama_cparams &,const int64_t &)' being compiled

  llama-hparams.cpp
  llama-impl.cpp
  llama-io.cpp
  llama-kv-cache.cpp
  llama-kv-cache-iswa.cpp
  llama-kv-cache-dsa.cpp
  llama-kv-cache-dsv4.cpp
  llama-memory.cpp
  llama-memory-hybrid.cpp
  llama-memory-hybrid-iswa.cpp
  llama-memory-recurrent.cpp
  llama-mmap.cpp
  Generating Code...
  Compiling...
  llama-model-loader.cpp
  llama-model-saver.cpp
  llama-model.cpp
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\utility(274,54): warning C4244: 'initializing': conversion from '_Ty' to '_Ty2', possible loss of data [H:\ai-on-a-stick\
ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\utility(274,54): warning C4244:         with [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-bui
ld\src\llama.vcxproj]
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\utility(274,54): warning C4244:         [ [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\
src\llama.vcxproj]
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\utility(274,54): warning C4244:             _Ty=int64_t [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\ll
ama_cpp-build\src\llama.vcxproj]
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\utility(274,54): warning C4244:         ] [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\
src\llama.vcxproj]
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\utility(274,54): warning C4244:         and [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-buil
d\src\llama.vcxproj]
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\utility(274,54): warning C4244:         [ [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\
src\llama.vcxproj]
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\utility(274,54): warning C4244:             _Ty2=uint32_t [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\
llama_cpp-build\src\llama.vcxproj]
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\utility(274,54): warning C4244:         ] [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\
src\llama.vcxproj]
  (compiling source file '../../llama_cpp-src/src/llama-model.cpp')
      C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\utility(274,54):
      the template instantiation context (the oldest one first) is
          H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-model.cpp(525,29):
          see reference to function template instantiation 'std::pair<int64_t,uint32_t>::pair<const int64_t&,int64_t,0>(_Other1,_Other2 &&) noexcept' being compiled
          with
          [
              _Other1=const int64_t &,
              _Other2=int64_t
          ]
              H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-model.cpp(525,21):
              see the first reference to 'std::pair<int64_t,uint32_t>::pair' in 'llama_meta_device_get_split_state::<lambda_3>::operator ()'

C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\utility(274,54): warning C4244: 'initializing': conversion from 'const int64_t' to '_Ty2', possible loss of data [H:\ai-o
n-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\utility(274,54): warning C4244:         with [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-bui
ld\src\llama.vcxproj]
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\utility(274,54): warning C4244:         [ [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\
src\llama.vcxproj]
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\utility(274,54): warning C4244:             _Ty2=uint32_t [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\
llama_cpp-build\src\llama.vcxproj]
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\utility(274,54): warning C4244:         ] [H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\
src\llama.vcxproj]
  (compiling source file '../../llama_cpp-src/src/llama-model.cpp')
      C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\utility(274,54):
      the template instantiation context (the oldest one first) is
          H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-model.cpp(528,29):
          see reference to function template instantiation 'std::pair<int64_t,uint32_t>::pair<const int64_t&,const int64_t&,0>(_Other1,_Other2) noexcept' being compiled
          with
          [
              _Other1=const int64_t &,
              _Other2=const int64_t &
          ]
              H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-model.cpp(528,21):
              see the first reference to 'std::pair<int64_t,uint32_t>::pair' in 'llama_meta_device_get_split_state::<lambda_3>::operator ()'

  llama-quant.cpp
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\type_traits(1680,98): warning C4244: 'argument': conversion from 'unsigned __int64' to 'int', possible loss of data [H:\a
i-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\src\llama.vcxproj]
  (compiling source file '../../llama_cpp-src/src/llama-quant.cpp')
      C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\type_traits(1680,98):
      the template instantiation context (the oldest one first) is
          H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-quant.cpp(276,17):
          see reference to function template instantiation 'std::thread &std::vector<std::thread,std::allocator<std::thread>>::emplace_back<llama_tensor_dequantize_impl::<lambda_1>&,ggml_type&,uint8_t*,float*,si
  ze_t&>(llama_tensor_dequantize_impl::<lambda_1> &,ggml_type &,uint8_t *&&,float *&&,size_t &)' being compiled
              H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-src\src\llama-quant.cpp(276,29):
              see the first reference to 'std::vector<std::thread,std::allocator<std::thread>>::emplace_back' in 'llama_tensor_dequantize_impl'
          C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\vector(924,24):
          see reference to function template instantiation '_Ty &std::vector<_Ty,std::allocator<_Ty>>::_Emplace_one_at_back<llama_tensor_dequantize_impl::<lambda_1>&,ggml_type&,unsigned char*,float*,size_t&>(lla
  ma_tensor_dequantize_impl::<lambda_1> &,ggml_type &,unsigned char *&&,float *&&,size_t &)' being compiled
          with
          [
              _Ty=std::thread
          ]
          C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\vector(845,20):
          see reference to function template instantiation '_Ty &std::vector<_Ty,std::allocator<_Ty>>::_Emplace_back_with_unused_capacity<llama_tensor_dequantize_impl::<lambda_1>&,ggml_type&,unsigned char*,float
  *,size_t&>(llama_tensor_dequantize_impl::<lambda_1> &,ggml_type &,unsigned char *&&,float *&&,size_t &)' being compiled
          with
          [
              _Ty=std::thread
          ]
          C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\vector(863,27):
          see reference to function template instantiation 'void std::_Default_allocator_traits<_Alloc>::construct<_Ty,llama_tensor_dequantize_impl::<lambda_1>&,ggml_type&,unsigned char*,float*,size_t&>(_Alloc &
  ,_Objty *const ,llama_tensor_dequantize_impl::<lambda_1> &,ggml_type &,unsigned char *&&,float *&&,size_t &)' being compiled
          with
          [
              _Alloc=std::allocator<std::thread>,
              _Ty=std::thread,
              _Objty=std::thread
          ]
          C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\xmemory(730,14):
          see reference to function template instantiation '_Ty *std::construct_at<_Objty,llama_tensor_dequantize_impl::<lambda_1>&,ggml_type&,unsigned char*,float*,size_t&>(_Ty *const ,llama_tensor_dequantize_i
  mpl::<lambda_1> &,ggml_type &,unsigned char *&&,float *&&,size_t &) noexcept(false)' being compiled
          with
          [
              _Ty=std::thread,
              _Objty=std::thread
          ]
          C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\xutility(463,69):
          see reference to function template instantiation 'std::thread::thread<llama_tensor_dequantize_impl::<lambda_1>&,ggml_type&,_T,float*,size_t&,0>(_Fn,ggml_type &,_T &&,float *&&,size_t &)' being compiled
          with
          [
              _T=uint8_t *,
              _Fn=llama_tensor_dequantize_impl::<lambda_1> &
          ]
          C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\thread(93,9):
          see reference to function template instantiation 'void std::thread::_Start<llama_tensor_dequantize_impl::<lambda_1>&,ggml_type&,_Ty,float*,size_t&>(_Fn,ggml_type &,_Ty &&,float *&&,size_t &)' being com
  piled
          with
          [
              _Ty=uint8_t *,
              _Fn=llama_tensor_dequantize_impl::<lambda_1> &
          ]
          C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\thread(76,40):
          see reference to function template instantiation 'unsigned int (__cdecl *std::thread::_Get_invoke<std::thread::_Start::_Tuple,0,1,2,3,4>(std::integer_sequence<size_t,0,1,2,3,4>) noexcept)(void *) noexc
  ept' being compiled
          C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\thread(67,17):
          see reference to function template instantiation 'unsigned int std::thread::_Invoke<std::thread::_Start::_Tuple,0,1,2,3,4>(void *) noexcept' being compiled
          C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\thread(60,14):
          see reference to function template instantiation 'void std::invoke<llama_tensor_dequantize_impl::<lambda_1>,ggml_type,uint8_t*,_Ty*,unsigned __int64>(_Callable &&,_Ty1 &&,uint8_t *&&,_Ty *&&,unsigned _
  _int64 &&) noexcept(false)' being compiled
          with
          [
              _Ty=float,
              _Callable=llama_tensor_dequantize_impl::<lambda_1>,
              _Ty1=ggml_type
          ]

  llama-sampler.cpp
  llama-vocab.cpp
  unicode-data.cpp
  unicode.cpp
  afmoe.cpp
  apertus.cpp
  arcee.cpp
  arctic.cpp
  arwkv7.cpp
  baichuan.cpp
  bailingmoe.cpp
  bailingmoe2.cpp
  bert.cpp
  bitnet.cpp
  bloom.cpp
  chameleon.cpp
  Generating Code...
  Compiling...
  chatglm.cpp
  codeshell.cpp
  cogvlm.cpp
  cohere2.cpp
  cohere2moe.cpp
  command-r.cpp
  dbrx.cpp
  deci.cpp
  deepseek.cpp
  deepseek2.cpp
  deepseek2ocr.cpp
  deepseek32.cpp
  deepseek4.cpp
  delta-net-base.cpp
  dflash.cpp
  dots1.cpp
  dream.cpp
  eagle3.cpp
  ernie4-5-moe.cpp
  ernie4-5.cpp
  Generating Code...
  Compiling...
  eurobert.cpp
  exaone-moe.cpp
  exaone.cpp
  exaone4.cpp
  falcon-h1.cpp
  falcon.cpp
  gemma-embedding.cpp
  gemma.cpp
  gemma2.cpp
  gemma3.cpp
  gemma3n.cpp
  gemma4-assistant.cpp
  gemma4.cpp
  glm-dsa.cpp
  glm4-moe.cpp
  glm4.cpp
  gpt2.cpp
  gptneox.cpp
  granite-hybrid.cpp
  granite-moe.cpp
  Generating Code...
  Compiling...
  granite.cpp
  grok.cpp
  grovemoe.cpp
  hunyuan-dense.cpp
  hunyuan-moe.cpp
  hunyuan-vl.cpp
  internlm2.cpp
  jais.cpp
  jais2.cpp
  jamba.cpp
  jina-bert-v2.cpp
  jina-bert-v3.cpp
  kimi-linear.cpp
  lfm2.cpp
  lfm2moe.cpp
  llada-moe.cpp
  llada.cpp
  llama-embed.cpp
  llama4.cpp
  maincoder.cpp
  Generating Code...
  Compiling...
  mamba-base.cpp
  mamba.cpp
  mamba2.cpp
  mellum.cpp
  mimo2.cpp
  minicpm.cpp
  minicpm3.cpp
  minimax-m2.cpp
  mistral3.cpp
  mistral4.cpp
  modern-bert.cpp
  mpt.cpp
  nemotron-h-moe.cpp
  nemotron-h.cpp
  nemotron.cpp
  neo-bert.cpp
  nomic-bert-moe.cpp
  nomic-bert.cpp
  olmo.cpp
  olmo2.cpp
  Generating Code...
  Compiling...
  olmoe.cpp
  openai-moe.cpp
  openelm.cpp
  orion.cpp
  paddleocr.cpp
  pangu-embed.cpp
  phi2.cpp
  phi3.cpp
  phimoe.cpp
  plamo.cpp
  plamo2.cpp
  plamo3.cpp
  plm.cpp
  qwen.cpp
  qwen2.cpp
  qwen2moe.cpp
  qwen2vl.cpp
  qwen3.cpp
  qwen35.cpp
  qwen35moe.cpp
  Generating Code...
  Compiling...
  qwen3moe.cpp
  qwen3next.cpp
  qwen3vl.cpp
  qwen3vlmoe.cpp
  refact.cpp
  rnd1.cpp
  rwkv6-base.cpp
  rwkv6.cpp
  rwkv6qwen2.cpp
  rwkv7-base.cpp
  rwkv7.cpp
  seed-oss.cpp
  smallthinker.cpp
  smollm3.cpp
  stablelm.cpp
  starcoder.cpp
  starcoder2.cpp
  step35.cpp
  t5.cpp
  t5encoder.cpp
  Generating Code...
  Compiling...
  talkie.cpp
  wavtokenizer-dec.cpp
  xverse.cpp
  Generating Code...
  Building Custom Rule H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_cpp-src/common/CMakeLists.txt
  build-info.cpp
  llama-common-base.vcxproj -> H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\common\Release\llama-common-base.lib
  Building Custom Rule H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_cpp-src/tools/ui/CMakeLists.txt
  embed.cpp
  llama-ui-embed.vcxproj -> H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\tools\ui\Release\llama-ui-embed.exe
  H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_cpp-build/tools/ui/Release/llama-ui-embed.exe: message: deploying dependencies
  1>Provisioning UI assets
  -- UI: npm not found, skipping npm build
  -- UI: downloading from b9886: https://huggingface.co/buckets/ggml-org/llama-ui/resolve/b9886/dist.tar.gz
  -- UI: download dist.tar.gz from b9886 failed: "HTTP response code said error"
  -- UI: downloading from latest: https://huggingface.co/buckets/ggml-org/llama-ui/resolve/latest/dist.tar.gz
  -- UI: archive verified and extracted
  -- UI: HF download succeeded, stamp updated (latest)
  CMake Warning at H:/ai-on-a-stick/AI-On-A-Stick/build/windows-vs2022-x64/_deps/llama_cpp-src/scripts/ui-assets.cmake:260 (message):
    UI: LLAMA_UI_GZIP requested but gzip not found, embedding uncompressed
  Call Stack (most recent call first):
    H:/ai-on-a-stick/AI-On-A-Stick/build/windows-vs2022-x64/_deps/llama_cpp-src/scripts/ui-assets.cmake:367 (emit_files)


  embed: write output file H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_cpp-build/tools/ui/ui.h
  embed: write output file H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_cpp-build/tools/ui/ui.cpp
  Building Custom Rule H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_cpp-src/tools/ui/CMakeLists.txt
  Building Custom Rule H:/ai-on-a-stick/ai-on-a-stick/build/windows-vs2022-x64/_deps/llama_cpp-src/tools/ui/CMakeLists.txt
  ui.cpp
  llama-ui.vcxproj -> H:\ai-on-a-stick\ai-on-a-stick\build\windows-vs2022-x64\_deps\llama_cpp-build\tools\ui\Release\llama-ui.lib