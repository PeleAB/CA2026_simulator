@echo off
REM This script sets up the MSBuild environment for VS Code

echo Setting up Visual Studio Build Tools environment...
echo.

REM Try to find Visual Studio 2022
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
    echo Visual Studio 2022 Community environment loaded
    goto :success
)

if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
    echo Visual Studio 2022 Professional environment loaded
    goto :success
)

if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
    echo Visual Studio 2022 Enterprise environment loaded
    goto :success
)

if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
    echo Visual Studio 2022 Build Tools environment loaded
    goto :success
)

REM Try Visual Studio 2019
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat"
    echo Visual Studio 2019 Community environment loaded
    goto :success
)

echo ERROR: Could not find Visual Studio installation!
echo.
echo Please install one of:
echo   - Visual Studio 2022 Community (free)
echo   - Visual Studio Build Tools 2022
echo.
echo Download from: https://visualstudio.microsoft.com/downloads/
pause
exit /b 1

:success
echo.
echo Environment ready! You can now run:
echo   msbuild vs\CA2026.sln /p:Configuration=Debug /p:Platform=x64
echo.
echo Or in VS Code:
echo   Press Ctrl+Shift+B to build
echo   Press F5 to debug
echo.
cmd /k
