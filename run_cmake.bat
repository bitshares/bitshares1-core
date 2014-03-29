setlocal
call setenv.bat
cd %BITSHARES_ROOT%
cmake-gui -G "Visual Studio 11"
