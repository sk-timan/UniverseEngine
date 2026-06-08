@echo off
setlocal

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

if not exist "%ROOT%\.vs" mkdir "%ROOT%\.vs"
> "%ROOT%\.vs\CMakeWorkspaceSettings.json" (
  echo {
  echo   "enableCMake": true,
  echo   "sourceDirectory": "${workspaceRoot}"
  echo }
)

set "CMAKE=C:\Program Files\Microsoft Visual Studio\18\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE%" (
  set "CMAKE=C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)

@echo ====== Configure CMake (x64-Debug) ======
"%CMAKE%" --preset x64-Debug -S "%ROOT%"
if errorlevel 1 (
  @echo [WARN] Preset configure failed, falling back to explicit build dir.
  "%CMAKE%" -S "%ROOT%" -B "%ROOT%\out\build\x64-Debug"
  if errorlevel 1 (
    @echo [ERROR] CMake configure failed.
    exit /b 1
  )
)
@echo [OK] CMake configure succeeded.

set "DEVENV=C:\Program Files\Microsoft Visual Studio\18\Professional\Common7\IDE\devenv.exe"
if not exist "%DEVENV%" (
  set "DEVENV=C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\devenv.exe"
)

@echo ====== Open Visual Studio (CMake folder mode) ======
start "" "%DEVENV%" /openfolder "%ROOT%"
