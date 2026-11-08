@echo off
setlocal
set INPUT_TEST_ROOT=%cd%
call scripts\find-vswhere.bat
if errorlevel 1 exit /b 1
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvarsall.bat" amd64
if errorlevel 1 exit /b 1
if not exist build\input-tests mkdir build\input-tests
pushd build\input-tests
qmake "%INPUT_TEST_ROOT%\tests\capslockcapture_test.pro" CONFIG+=release
if errorlevel 1 exit /b 1
nmake
if errorlevel 1 exit /b 1
set "PATH=%INPUT_TEST_ROOT%\build\deploy-x64-release;%PATH%"
release\capslockcapture-test.exe
set INPUT_TEST_RESULT=%ERRORLEVEL%
popd
exit /b %INPUT_TEST_RESULT%
