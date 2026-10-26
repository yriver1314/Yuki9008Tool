@echo off
setlocal
cd /d "%~dp0"

where g++ >nul 2>nul
if errorlevel 1 (
    echo [ERROR] g++ not found. Install MinGW-w64 or add its bin directory to PATH.
    exit /b 1
)

where windres >nul 2>nul
if errorlevel 1 (
    echo [ERROR] windres not found. It ships with MinGW-w64.
    exit /b 1
)

echo [1/4] Compiling resources...
windres --codepage=65001 version.rc -O coff -o version.res
if errorlevel 1 (
    echo [ERROR] Resource compile failed.
    exit /b 1
)

echo [2/4] Compiling Yuki9008Tool...
g++ main.cpp app_state.cpp paint.cpp paint_foreground.cpp paint_cards.cpp paint_pages.cpp version.res ^
    -o Yuki9008Tool.exe -mwindows -std=c++17 -O2 ^
    -finput-charset=UTF-8 -fexec-charset=UTF-8 ^
    -lgdiplus -lgdi32 -luser32 -ldwmapi -luxtheme ^
    -lole32 -luuid -lshell32 -lsetupapi -lcomctl32 -lbcrypt -lwinhttp ^
    -static-libgcc -static-libstdc++
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo [3/4] Preparing dist...
if not exist dist mkdir dist
copy /Y Yuki9008Tool.exe dist\Yuki9008Tool.exe >nul
copy /Y logo.ico dist\logo.ico >nul
copy /Y README.md dist\README.md >nul
if exist preview.png copy /Y preview.png dist\preview.png >nul

for /f "delims=" %%I in ('where g++') do (
    set "GXX=%%I"
    goto :found
)
:found
for %%I in ("%GXX%") do set "MINGW=%%~dpI"
if exist "%MINGW%libwinpthread-1.dll" (
    copy /Y "%MINGW%libwinpthread-1.dll" dist\libwinpthread-1.dll >nul
    copy /Y "%MINGW%libwinpthread-1.dll" libwinpthread-1.dll >nul
) else (
    echo [WARN] libwinpthread-1.dll not found next to g++. Copy it manually if needed.
)

echo [4/4] Done.
echo   Source exe : %CD%\Yuki9008Tool.exe
echo   Dist exe   : %CD%\dist\Yuki9008Tool.exe
endlocal
