@echo off
setlocal
cd /d "%~dp0"
title Akshayanagara Lake F1 Simulator

if not exist "build\Release\LakeF1.exe" (
    echo Building the simulator for the first time...
    where cmake >nul 2>nul
    if errorlevel 1 (
        echo.
        echo CMake was not found.
        echo Install CMake and Visual Studio with Desktop C++ support, then run this file again.
        pause
        exit /b 1
    )
    cmake -S . -B build
    if errorlevel 1 (
        echo.
        echo CMake configuration failed. Check the error above.
        pause
        exit /b 1
    )
    cmake --build build --config Release
    if errorlevel 1 (
        echo.
        echo Build failed. Check the error above.
        pause
        exit /b 1
    )
)

echo Starting Akshayanagara Lake F1 Simulator...
start "Lake F1 Simulator" /wait "build\Release\LakeF1.exe"
if errorlevel 1 pause
endlocal
