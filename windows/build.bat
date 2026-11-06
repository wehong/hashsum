@echo off
rem ======================================================================
rem  hashsum.exe - Windows build script
rem
rem  Usage:  build.bat [x86 | x64] [zig | msvc | mingw]
rem
rem    x86   32-bit EXE (default) - runs on 32/64-bit Windows 7 and later
rem    x64   64-bit EXE
rem
rem    Compiler is auto-detected in this order if not specified:
rem      1. zig    https://ziglang.org (unzip and add to PATH)
rem                or set ZIG, e.g.  set ZIG=python -m ziglang
rem      2. msvc   run from "x86/x64 Native Tools Command Prompt for VS"
rem      3. mingw  gcc from MinGW-w64 (MSYS2 etc.)
rem
rem  Output:  build\windows-<arch>\hashsum.exe  (in the repository root)
rem
rem  The EXE is linked WITHOUT the C runtime (CRT): it imports only
rem  KERNEL32.dll and SHELL32.dll, so it runs on a clean Windows 7
rem  without the VC++ redistributable or UCRT.
rem
rem  (Messages are in English on purpose: cmd.exe reads .bat files in the
rem   console code page, so non-ASCII text could be garbled.)
rem ======================================================================
setlocal

set ARCH=x86
set TOOL=
set SRC=%~dp0hashsum.c

:parse
if "%~1"=="" goto parsed
if /i "%~1"=="x86"   set ARCH=x86& shift & goto parse
if /i "%~1"=="x64"   set ARCH=x64& shift & goto parse
if /i "%~1"=="zig"   set TOOL=zig& shift & goto parse
if /i "%~1"=="msvc"  set TOOL=msvc& shift & goto parse
if /i "%~1"=="mingw" set TOOL=mingw& shift & goto parse
echo Unknown argument: %~1
echo Usage: build.bat [x86 ^| x64] [zig ^| msvc ^| mingw]
exit /b 2
:parsed

rem Repository root (absolute path, without "..")
for %%I in ("%~dp0..") do set ROOT=%%~fI
set OUTDIR=%ROOT%\build\windows-%ARCH%
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

if not "%TOOL%"=="" goto build_%TOOL%

rem Look up compilers on PATH with cmd's own %%~$PATH: (no external tools)
if defined ZIG goto build_zig
call :onpath zig.exe
if not errorlevel 1 goto build_zig
call :onpath cl.exe
if not errorlevel 1 goto build_msvc
call :onpath gcc.exe
if not errorlevel 1 goto build_mingw

echo No compiler found. Install one of the following:
echo   - Zig   : https://ziglang.org/download/  (unzip, add folder to PATH)
echo             or: pip install ziglang  then  set ZIG=python -m ziglang
echo   - MSVC  : Visual Studio Build Tools, then use "Native Tools Command Prompt"
echo   - MinGW : https://www.msys2.org/
exit /b 1

rem ---------------------------------------------------------------- zig
:build_zig
if not defined ZIG set ZIG=zig
if "%ARCH%"=="x64" (set TARGET=x86_64-windows.win7-gnu) else (set TARGET=x86-windows.win7-gnu)
echo [zig] %TARGET%
rem Compile and link separately: a one-step build pulls in MinGW start-up
rem code that depends on the UCRT.
set CFLAGS=-Os -Wall -Wextra -fno-builtin -fno-stack-protector
set LDFLAGS=-nostdlib -s -Wl,--entry=entry -Wl,--subsystem,console -Wl,--major-subsystem-version,6 -Wl,--minor-subsystem-version,1 -lkernel32 -lshell32
%ZIG% cc -target %TARGET% %CFLAGS% -c "%SRC%" -o "%OUTDIR%\hashsum.o"
if errorlevel 1 goto fail
%ZIG% cc -target %TARGET% "%OUTDIR%\hashsum.o" %LDFLAGS% -o "%OUTDIR%\hashsum.exe"
if errorlevel 1 goto fail
del /q "%OUTDIR%\hashsum.o" "%OUTDIR%\hashsum.pdb" 2>nul
goto done

rem --------------------------------------------------------------- msvc
:build_msvc
rem The target architecture is decided by the prompt you opened
rem ("x86 Native Tools" or "x64 Native Tools").
if /i "%VSCMD_ARG_TGT_ARCH%"=="x64" if "%ARCH%"=="x86" (
    echo Error: this prompt targets x64. Open "x86 Native Tools Command Prompt" for a 32-bit EXE.
    exit /b 1
)
if /i "%VSCMD_ARG_TGT_ARCH%"=="x86" if "%ARCH%"=="x64" (
    echo Error: this prompt targets x86. Open "x64 Native Tools Command Prompt" for a 64-bit EXE.
    exit /b 1
)
echo [msvc] %VSCMD_ARG_TGT_ARCH%
cl /nologo /O1 /GS- /Zl /utf-8 /W3 /c "%SRC%" /Fo"%OUTDIR%\hashsum.obj"
if errorlevel 1 goto fail
set LDFLAGS=/nologo /nodefaultlib /entry:entry /subsystem:console,6.01 /dynamicbase /nxcompat /release /opt:ref
link %LDFLAGS% "%OUTDIR%\hashsum.obj" kernel32.lib shell32.lib /out:"%OUTDIR%\hashsum.exe"
if errorlevel 1 goto fail
del /q "%OUTDIR%\hashsum.obj" 2>nul
goto done

rem -------------------------------------------------------------- mingw
:build_mingw
rem 32-bit symbols carry a leading underscore (_entry), 64-bit do not.
rem GNU ld does not fail on a wrong entry name - it silently uses the start
rem of .text - so the name must match the compiler's target.
set MACH=
for /f %%m in ('gcc -dumpmachine') do set MACH=%%m
set ENTRY=_entry
if /i "%MACH:~0,6%"=="x86_64" set ENTRY=entry
echo [mingw] %MACH%
if "%ARCH%"=="x64" if "%ENTRY%"=="_entry" (
    echo Error: this gcc builds 32-bit EXEs. Use an x86_64 MinGW gcc for x64.
    exit /b 1
)
if "%ARCH%"=="x86" if "%ENTRY%"=="entry" (
    echo Error: this gcc builds 64-bit EXEs. Use an i686 MinGW gcc for x86.
    exit /b 1
)
set CFLAGS=-Os -Wall -Wextra -fno-builtin -fno-stack-protector
rem --enable-stdcall-fixup: entry is __stdcall (_entry@0) on x86; silence ld warning
set LDFLAGS=-nostdlib -s -Wl,-e,%ENTRY% -Wl,--enable-stdcall-fixup -Wl,--subsystem,console:6.1 -lkernel32 -lshell32
gcc %CFLAGS% "%SRC%" %LDFLAGS% -o "%OUTDIR%\hashsum.exe"
if errorlevel 1 goto fail
goto done

:done
echo.
echo Built: %OUTDIR%\hashsum.exe
echo Test : python "%ROOT%\tests\test_hashsum.py" "%OUTDIR%\hashsum.exe"
exit /b 0

:fail
echo.
echo Build failed.
exit /b 1

rem ------------------------------------------------------------ helpers
rem call :onpath name.exe  -> errorlevel 0 if found on PATH
:onpath
if "%~$PATH:1"=="" exit /b 1
exit /b 0
