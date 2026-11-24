
@echo off
chcp 65001 >nul
title NEBULA V2 - BUILD SYSTEM

cd /d "%~dp0\.."

echo.
echo ==========================================
echo          NEBULA V2 BUILD SYSTEM
echo ==========================================
echo.

where cmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake is not installed.
    echo Install CMake 3.22 or newer.
    pause
    exit /b 1
)

where py >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python is not installed.
    pause
    exit /b 1
)

if not exist "CMakeLists.txt" (
    echo ERROR: CMakeLists.txt is missing.
    pause
    exit /b 1
)

if not exist "Source\PluginProcessor.cpp" (
    echo ERROR: Plugin source files are missing.
    pause
    exit /b 1
)

echo [1/4] Checking Python...
py -3 --version
if errorlevel 1 goto ERROR

echo.
echo [2/4] Configuring JUCE and NEBULA...
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DNEBULA_FETCH_JUCE=ON -DNEBULA_BUILD_TESTS=ON
if errorlevel 1 goto ERROR

echo.
echo [3/4] Building FX, SYNTH and DRUM...
cmake --build build --config Release --parallel 2
if errorlevel 1 goto ERROR

echo.
echo [4/4] Running tests...
ctest --test-dir build -C Release --output-on-failure
if errorlevel 1 goto ERROR

echo.
echo ==========================================
echo           BUILD SUCCESSFUL
echo ==========================================
echo.
echo Check the build folder for VST3 and Standalone.
echo No automatic system installation was performed.
echo.
pause
exit /b 0

:ERROR
echo.
echo ==========================================
echo              BUILD FAILED
echo ==========================================
echo Check the error messages above.
pause
exit /b 1
