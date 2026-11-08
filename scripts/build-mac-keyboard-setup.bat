@echo off
setlocal
if not defined MAC_KEYBOARD_BUILD exit /b 1
set "BUILD_ROOT=%cd%\build"
set "BUILD_CONFIG=release"
set "BUILD_FOLDER=%BUILD_ROOT%\build-bundle-release"
set "INSTALLER_FOLDER=%BUILD_ROOT%\installer-x64-release"
call scripts\find-vswhere.bat
if errorlevel 1 exit /b 1
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvarsall.bat" amd64
if errorlevel 1 exit /b 1
msbuild -Restore wix\MoonlightSetup\MoonlightSetup.wixproj /p:Configuration=Release /p:Platform=x64 /p:MSBuildProjectExtensionsPath="%BUILD_FOLDER%"
if errorlevel 1 exit /b 1
copy "%INSTALLER_FOLDER%\MoonlightSetup.exe" "%INSTALLER_FOLDER%\Moonlight-MacKeyboard-v5-x64-Setup.exe"
exit /b %ERRORLEVEL%
