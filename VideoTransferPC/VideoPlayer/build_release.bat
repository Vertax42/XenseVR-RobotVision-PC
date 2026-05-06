@echo off
setlocal enabledelayedexpansion

echo ===== Building VideoPlayer (Release) =====

REM Change to the VideoPlayer directory if script is run from elsewhere
set SCRIPT_DIR=%~dp0
cd %SCRIPT_DIR%

REM Create build directory if it doesn't exist
if not exist "build_release" (
    echo Creating build directory...
    mkdir build_release
)

cd build_release

REM Set Visual Studio path
set VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\devenv.exe

REM Set Qt directory
set QT_DIR=C:\Qt\6.6.3\msvc2019_64

REM Verify Qt directory exists
if not exist "!QT_DIR!\bin\qmake.exe" (
    echo Qt not found at !QT_DIR!
    echo Please check the Qt path in this script.
    exit /b 1
)

REM Add Qt to the PATH
set PATH=!QT_DIR!\bin;%PATH%

REM Configure with CMake
echo Configuring project with CMake...
cmake -G "Visual Studio 16 2019" -A x64 ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH=!QT_DIR! ^
      ..

if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed.
    exit /b 1
)

REM Build the project
echo Building project...
cmake --build . --config Release

if %ERRORLEVEL% neq 0 (
    echo Build failed.
    exit /b 1
)

echo ===== Build completed successfully =====
echo Executable location: build_release\Release\appVideoPlayer.exe
echo BUILD_PROCESS_COMPLETED_SUCCESSFULLY

cd ..