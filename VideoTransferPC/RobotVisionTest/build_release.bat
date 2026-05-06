@echo off
setlocal enabledelayedexpansion

echo ===== Building RobotVisionTest (Release) =====

REM Change to the RobotVisionTest directory if script is run from elsewhere
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

REM Set FFmpeg and ASIO paths
set FFMPEG_ROOT=..\..\..\..\ffmpeg-7.1-full_build-shared
set ASIO_ROOT=..\..\..\..\asio-1.30.2\include

REM Configure with CMake
echo Configuring project with CMake...
cmake -G "Visual Studio 16 2019" -A x64 ^
      -DCMAKE_BUILD_TYPE=Release ^
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
echo Executable location: build_release\Release\RobotVisionConsole.exe
echo BUILD_PROCESS_COMPLETED_SUCCESSFULLY

cd ..
