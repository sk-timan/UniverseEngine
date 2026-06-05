@echo off
if exist "build" (
    rmdir /s /q "build"
)

mkdir "build"

@echo ====== Generate VS 2022 solution ======
set "CMAKE_LOG=build\cmake-generate.log"
@cmake -G "Visual Studio 17" -A x64 -Bbuild --log-level=ERROR --no-warn-unused-cli 1>"%CMAKE_LOG%" 2>&1
if errorlevel 1 (
    @echo [ERROR] CMake generation failed. Full output:
    @type "%CMAKE_LOG%"
    @exit /b 1
)
@echo [OK] CMake generation succeeded. Detailed log: "%CMAKE_LOG%"

@echo ====== Start Visual Studio ======
@start build\OpenSpecTest.sln
