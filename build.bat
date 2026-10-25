@echo off
echo ==============================================
echo   Compiling DREADFALL 3D (Boomer Shooter) ...
echo ==============================================
g++ -O2 -mwindows src\main.cpp -lopengl32 -lglu32 -lgdi32 -luser32 -lwinmm -o Dreadfall.exe
if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] Dreadfall.exe compiled successfully!
) else (
    echo.
    echo [ERROR] Compilation failed!
)
pause
