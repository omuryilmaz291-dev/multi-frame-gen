@echo off
setlocal
where cmake >nul 2>nul || (echo [HATA] CMake bulunamadi. Visual Studio'da "C++ ile masaustu gelistirme" ve "CMake" bilesenlerini kurun. & pause & exit /b 1)
cmake -S . -B build -A x64 || (echo [HATA] Yapilandirma basarisiz. & pause & exit /b 1)
cmake --build build --config Release || (echo [HATA] Derleme basarisiz. & pause & exit /b 1)
if exist dist rmdir /s /q dist
mkdir dist
copy /y build\Release\FrameInterp.exe dist\ >nul
echo.
echo [TAMAM] dist\FrameInterp.exe hazir (tek dosya, baska bir sey gerekmez).
pause
