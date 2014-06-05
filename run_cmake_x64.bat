setlocal
call "%~dp0\setenv_x64.bat"
cd %BITSHARES_ROOT%
cmake-gui -G "Visual Studio 12 Win64"
