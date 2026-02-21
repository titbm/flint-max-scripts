@echo off
setlocal DisableDelayedExpansion

echo ============================================
echo  FLINT sorter compilation
echo  CFTSORT / BFTSORT / CFTSORT32 / BFTSORT32
echo ============================================

cd /d "%~dp0"
if not exist BUILD mkdir BUILD

rem =============================================
rem  32-bit: Open Watcom C++ (native)
rem =============================================

echo.
echo --- Win32 (Open Watcom C++) ---

for %%I in ("%~dp0..\..\Tools\WATCOM") do set "WATCOM=%%~sI"

if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set "WBIN=%WATCOM%\binnt64"
) else if defined PROCESSOR_ARCHITEW6432 (
    set "WBIN=%WATCOM%\binnt64"
) else (
    set "WBIN=%WATCOM%\binnt"
)

set "WPP=%WBIN%\wpp386.exe"
set "WLINK=%WBIN%\wlink.exe"

if not exist "%WPP%" (
    echo ERROR: Compiler not found: %WPP%
    goto SKIP_32
)

echo Compiling CFTSORT-32BIT.CPP...
"%WPP%" -3r -ox -i=%WATCOM%\h CFTSORT-32BIT.CPP
if errorlevel 1 (
    echo ERROR: CFTSORT-32BIT compilation failed
    goto CLEANUP_32
)

echo Linking CFTSORT32.EXE...
"%WLINK%" format windows nt runtime console=4.0 libpath %WATCOM%\lib386 libpath %WATCOM%\lib386\nt name BUILD\CFTSORT32.EXE file CFTSORT-32BIT.OBJ library kernel32 library user32 library clib3r library math3r
if errorlevel 1 (
    echo ERROR: CFTSORT32 linking failed
    goto CLEANUP_32
)

echo Compiling BFTSORT-32BIT.CPP...
"%WPP%" -3r -ox -i=%WATCOM%\h BFTSORT-32BIT.CPP
if errorlevel 1 (
    echo ERROR: BFTSORT-32BIT compilation failed
    goto CLEANUP_32
)

echo Linking BFTSORT32.EXE...
"%WLINK%" format windows nt runtime console=4.0 libpath %WATCOM%\lib386 libpath %WATCOM%\lib386\nt name BUILD\BFTSORT32.EXE file BFTSORT-32BIT.OBJ library kernel32 library user32 library clib3r library math3r
if errorlevel 1 (
    echo ERROR: BFTSORT32 linking failed
    goto CLEANUP_32
)

:CLEANUP_32
del *.OBJ 2>nul
del *.ERR 2>nul

:SKIP_32

rem =============================================
rem  16-bit: Borland C++ 4.5
rem =============================================

echo.
echo --- 16-bit DOS (Borland C++ 4.5) ---

set "BC45=%~dp0..\..\Tools\BC45"
set "DOSBOX=%~dp0..\..\Tools\DOSBox-X\dosbox-x.exe"
set "DBCONF=%~dp0..\..\Tools\DOSBox-X\dosbox-x.conf"
set "ROOT=%~dp0..\.."

if "%PROCESSOR_ARCHITECTURE%"=="AMD64" goto DOSBOX_BUILD
if defined PROCESSOR_ARCHITEW6432 goto DOSBOX_BUILD

:NATIVE_16
echo [32-bit OS: native compilation]
echo.

if not exist "%BC45%\BIN\BCC.EXE" (
    echo ERROR: BCC.EXE not found: %BC45%\BIN\BCC.EXE
    goto RESULTS
)

echo Compiling CFTSORT.CPP...
"%BC45%\BIN\BCC.EXE" -ml -3 -O1 -f -I"%BC45%\INCLUDE" -L"%BC45%\LIB" -eBUILD\CFTSORT.EXE CFTSORT.CPP
if errorlevel 1 echo ERROR: CFTSORT compilation failed

echo Compiling BFTSORT.CPP...
"%BC45%\BIN\BCC.EXE" -ml -3 -O1 -f -I"%BC45%\INCLUDE" -L"%BC45%\LIB" -eBUILD\BFTSORT.EXE BFTSORT.CPP
if errorlevel 1 echo ERROR: BFTSORT compilation failed

del *.OBJ 2>nul
goto RESULTS

:DOSBOX_BUILD
echo [64-bit OS: compilation via DOSBox-X]
echo.

if not exist "%DOSBOX%" (
    echo ERROR: DOSBox-X not found: %DOSBOX%
    goto RESULTS
)

pushd "%ROOT%"
"%DOSBOX%" -conf "%DBCONF%" ^
    -c "mount d ." ^
    -c "d:" ^
    -c "cd \Utils\SRC" ^
    -c "set PATH=D:\Tools\BC45\BIN" ^
    -c "if not exist BUILD mkdir BUILD" ^
    -c "BCC -ml -3 -O1 -f -ID:\Tools\BC45\INCLUDE -LD:\Tools\BC45\LIB -eBUILD\CFTSORT.EXE CFTSORT.CPP" ^
    -c "BCC -ml -3 -O1 -f -ID:\Tools\BC45\INCLUDE -LD:\Tools\BC45\LIB -eBUILD\BFTSORT.EXE BFTSORT.CPP" ^
    -c "del *.OBJ" ^
    -c "exit"
popd

:RESULTS
echo.
echo ============================================
echo  Results:
echo ============================================
if exist BUILD\CFTSORT32.EXE (echo   OK:   CFTSORT32.EXE) else (echo   FAIL: CFTSORT32.EXE)
if exist BUILD\BFTSORT32.EXE (echo   OK:   BFTSORT32.EXE) else (echo   FAIL: BFTSORT32.EXE)
if exist BUILD\CFTSORT.EXE   (echo   OK:   CFTSORT.EXE)   else (echo   FAIL: CFTSORT.EXE)
if exist BUILD\BFTSORT.EXE   (echo   OK:   BFTSORT.EXE)   else (echo   FAIL: BFTSORT.EXE)
echo ============================================
echo Done.
pause
endlocal
