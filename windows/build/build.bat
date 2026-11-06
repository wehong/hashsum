@echo off
rem ======================================================================
rem  hashsum.exe / hashsum-gui.exe - Windows build script
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
rem  Output:  windows\bin\windows-<arch>\hashsum.exe       (command line)
rem           windows\bin\windows-<arch>\hashsum-gui.exe   (GUI)
rem
rem  Both EXEs are linked WITHOUT the C runtime (CRT) and import only DLLs
rem  that ship with Windows, so they run on a clean Windows 7 without the
rem  VC++ redistributable or UCRT.
rem    hashsum.exe     : KERNEL32, SHELL32
rem    hashsum-gui.exe : KERNEL32, USER32, GDI32, SHELL32, COMDLG32, COMCTL32
rem
rem  (Messages are in English on purpose: cmd.exe reads .bat files in the
rem   console code page, so non-ASCII text could be garbled.)
rem ======================================================================
setlocal

set ARCH=x86
set TOOL=

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

rem windows\ folder (absolute path, without "..")
for %%I in ("%~dp0..") do set WINROOT=%%~fI
set OUTDIR=%WINROOT%\bin\windows-%ARCH%
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
rem Work in this folder: hashsum-gui.rc refers to the icon/manifest files by
rem name, relative to this folder. (popd in :done / :fail)
pushd "%~dp0"
if not defined ZIG set ZIG=zig
if "%ARCH%"=="x64" (set TARGET=x86_64-windows.win7-gnu) else (set TARGET=x86-windows.win7-gnu)
echo [zig] %TARGET%
rem Compile and link separately: a one-step build pulls in MinGW start-up
rem code that depends on the UCRT.
set CFLAGS=-Os -Wall -Wextra -fno-builtin -fno-stack-protector
set COMMON=-nostdlib -s -Wl,--entry=entry -Wl,--major-subsystem-version,6 -Wl,--minor-subsystem-version,1 -Wl,/Brepro
set CLI_LIBS=-Wl,--subsystem,console -lkernel32 -lshell32
set GUI_LIBS=-Wl,--subsystem,windows -luser32 -lgdi32 -lcomdlg32 -lcomctl32 -lshell32 -lkernel32
%ZIG% cc -target %TARGET% %CFLAGS% -c hashsum.c -o "%OUTDIR%\hashsum.o"
if errorlevel 1 goto fail
%ZIG% cc -target %TARGET% "%OUTDIR%\hashsum.o" %COMMON% %CLI_LIBS% -o "%OUTDIR%\hashsum.exe"
if errorlevel 1 goto fail
rem GUI: zig cc compiles the .rc (icons, manifest, version info) itself
%ZIG% cc -target %TARGET% %CFLAGS% -c hashsum-gui.c -o "%OUTDIR%\hashsum-gui.o"
if errorlevel 1 goto fail
%ZIG% cc -target %TARGET% "%OUTDIR%\hashsum-gui.o" hashsum-gui.rc %COMMON% %GUI_LIBS% -o "%OUTDIR%\hashsum-gui.exe"
if errorlevel 1 goto fail
del /q "%OUTDIR%\*.o" "%OUTDIR%\*.pdb" 2>nul
goto done

rem --------------------------------------------------------------- msvc
:build_msvc
pushd "%~dp0"
rem The target architecture is decided by the prompt you opened
rem ("x86 Native Tools" or "x64 Native Tools").
if /i "%VSCMD_ARG_TGT_ARCH%"=="x64" if "%ARCH%"=="x86" (
    echo Error: this prompt targets x64. Open "x86 Native Tools Command Prompt" for a 32-bit EXE.
    goto fail
)
if /i "%VSCMD_ARG_TGT_ARCH%"=="x86" if "%ARCH%"=="x64" (
    echo Error: this prompt targets x86. Open "x64 Native Tools Command Prompt" for a 64-bit EXE.
    goto fail
)
echo [msvc] %VSCMD_ARG_TGT_ARCH%
set CFLAGS=/nologo /O1 /GS- /Zl /utf-8 /W3
set COMMON=/nologo /nodefaultlib /entry:entry /dynamicbase /nxcompat /release /opt:ref /Brepro
cl %CFLAGS% /c hashsum.c /Fo"%OUTDIR%\hashsum.obj"
if errorlevel 1 goto fail
link %COMMON% /subsystem:console,6.01 "%OUTDIR%\hashsum.obj" kernel32.lib shell32.lib /out:"%OUTDIR%\hashsum.exe"
if errorlevel 1 goto fail
rc /nologo /fo "%OUTDIR%\hashsum-gui.res" hashsum-gui.rc
if errorlevel 1 goto fail
cl %CFLAGS% /c hashsum-gui.c /Fo"%OUTDIR%\hashsum-gui.obj"
if errorlevel 1 goto fail
link %COMMON% /subsystem:windows,6.01 "%OUTDIR%\hashsum-gui.obj" "%OUTDIR%\hashsum-gui.res" user32.lib gdi32.lib comdlg32.lib comctl32.lib shell32.lib kernel32.lib /out:"%OUTDIR%\hashsum-gui.exe"
if errorlevel 1 goto fail
del /q "%OUTDIR%\*.obj" "%OUTDIR%\*.res" 2>nul
goto done

rem -------------------------------------------------------------- mingw
:build_mingw
pushd "%~dp0"
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
    goto fail
)
if "%ARCH%"=="x86" if "%ENTRY%"=="entry" (
    echo Error: this gcc builds 64-bit EXEs. Use an i686 MinGW gcc for x86.
    goto fail
)
set CFLAGS=-Os -Wall -Wextra -fno-builtin -fno-stack-protector
rem --enable-stdcall-fixup: entry is __stdcall (_entry@0) on x86; silence ld warning
rem --no-insert-timestamp : do not write the build time into the EXE
set COMMON=-nostdlib -s -Wl,-e,%ENTRY% -Wl,--enable-stdcall-fixup -Wl,--no-insert-timestamp
gcc %CFLAGS% hashsum.c %COMMON% -Wl,--subsystem,console:6.1 -lkernel32 -lshell32 -o "%OUTDIR%\hashsum.exe"
if errorlevel 1 goto fail
rem GUI: windres (comes with MinGW gcc) compiles the .rc into an object file
windres -i hashsum-gui.rc -o "%OUTDIR%\hashsum-gui-res.o"
if errorlevel 1 goto fail
gcc %CFLAGS% hashsum-gui.c "%OUTDIR%\hashsum-gui-res.o" %COMMON% -Wl,--subsystem,windows:6.1 -luser32 -lgdi32 -lcomdlg32 -lcomctl32 -lshell32 -lkernel32 -o "%OUTDIR%\hashsum-gui.exe"
if errorlevel 1 goto fail
del /q "%OUTDIR%\*.o" 2>nul
goto done

:done
popd
echo.
echo Built: %OUTDIR%\hashsum.exe
echo        %OUTDIR%\hashsum-gui.exe
exit /b 0

:fail
popd
echo.
echo Build failed.
exit /b 1

rem ------------------------------------------------------------ helpers
rem call :onpath name.exe  -> errorlevel 0 if found on PATH
:onpath
if "%~$PATH:1"=="" exit /b 1
exit /b 0
