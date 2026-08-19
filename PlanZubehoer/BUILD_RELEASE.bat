@echo off
setlocal
cd /d "%~dp0"

echo ============================================================
echo Plan Zubehoer - Vectorworks 2026 SDK Build
echo ============================================================

if not exist "..\..\VectorworksSDK\SDK2026\SDKLib\Include\OnlyWin\NNA_PluginBuild_RELEASE.props" (
    echo.
    echo FEHLER: Vectorworks SDK 2026 wurde nicht am erwarteten Ort gefunden.
    echo.
    echo Kopiere diesen Ordner nach:
    echo   SDKExamples\Examples2026\PlanZubehoer
    echo.
    echo Danach muss relativ dazu vorhanden sein:
    echo   SDKExamples\VectorworksSDK\SDK2026
    echo.
    exit /b 2
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo FEHLER: vswhere.exe wurde nicht gefunden. Visual Studio 2022 installieren.
    exit /b 3
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do set "VSROOT=%%i"

if not defined VSROOT (
    echo FEHLER: Keine passende Visual-Studio-Installation gefunden.
    exit /b 4
)

set "MSBUILD=%VSROOT%\MSBuild\Current\Bin\MSBuild.exe"
if not exist "%MSBUILD%" (
    echo FEHLER: MSBuild.exe wurde nicht gefunden.
    exit /b 5
)

"%MSBUILD%" PlanZubehoer2026.sln /m /t:Build /p:Configuration=Release /p:Platform=x64
if errorlevel 1 (
    echo.
    echo BUILD FEHLGESCHLAGEN.
    exit /b 10
)

echo.
echo BUILD ERFOLGREICH.
echo Erwartete Ausgabe:
echo   ..\..\Output\2026\_Output\Release\PlanZubehoer.vlb
echo   ..\..\Output\2026\_Output\Release\PlanZubehoer.vwr
echo.
endlocal
