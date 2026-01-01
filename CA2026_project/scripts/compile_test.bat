@echo off
if not defined VSCMD_VER call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d %~dp0\..
if not exist build mkdir build
cl.exe /nologo /Zi /Fe:build\CA2026_test.exe src\main.c src\core.c src\cache.c src\bus.c src\init.c src\instruction.c src\stubs.c /I src /D_CRT_SECURE_NO_WARNINGS
echo Build complete. Executable in build\CA2026_test.exe
