setlocal
call "%~dp0\setenv.bat"
cd %BITSHARES_ROOT%
cmake-gui -G "Visual Studio 12"
