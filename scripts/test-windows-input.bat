@echo off
setlocal
set INPUT_TEST_ROOT=%cd%
call scripts\find-vswhere.bat
if errorlevel 1 exit /b 1
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvarsall.bat" amd64
if errorlevel 1 exit /b 1
if not exist build\input-tests mkdir build\input-tests
pushd build\input-tests
cl /nologo /std:c++17 /EHsc /W4 /I"%INPUT_TEST_ROOT%\app" /I"%INPUT_TEST_ROOT%\libs\windows\include\x64\SDL2" /I"%INPUT_TEST_ROOT%\libs\windows\include\x64" /I"%INPUT_TEST_ROOT%\libs\windows\include" /Fe:capslockcapture-test.exe "%INPUT_TEST_ROOT%\tests\capslockcapture_test.cpp" "%INPUT_TEST_ROOT%\tests\rawkeyboardscope_test.cpp" "%INPUT_TEST_ROOT%\app\streaming\input\nativekeyboardcapture.cpp" /link /LIBPATH:"%INPUT_TEST_ROOT%\libs\windows\lib\x64" SDL2.lib user32.lib
if errorlevel 1 exit /b 1
set "PATH=%INPUT_TEST_ROOT%\libs\windows\lib\x64;%PATH%"
capslockcapture-test.exe
set INPUT_TEST_RESULT=%ERRORLEVEL%
popd
exit /b %INPUT_TEST_RESULT%
