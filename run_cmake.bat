setlocal
call "%~dp0\setenv.bat"
cd %BITSHARES_ROOT%
cmake-gui -DINCLUDE_QT_WALLET=TRUE -G "Visual Studio 12"
