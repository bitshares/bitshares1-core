@echo off
set BITSHARES_ROOT=%~dp0..\
echo Using %BITSHARES_ROOT% as Invictus root directory

set OPENSSL_ROOT=%BITSHARES_ROOT%\OpenSSL
set OPENSSL_ROOT_DIR=%OPENSSL_ROOT%
set OPENSSL_INCLUDE_DIR=%OPENSSL_ROOT%\include
if "%DBROOTDIR%" == "" set DBROOTDIR=%BITSHARES_ROOT%\BerkeleyDB

rem set BOOST_ROOT only if it is not yet configured
if "%BOOST_ROOT%" == "" set BOOST_ROOT=%BITSHARES_ROOT%\boost

set PATH=%BITSHARES_ROOT%\bin;%BITSHARES_ROOT%\Cmake\bin;%BITSHARES_ROOT%\boost\stage\lib;%PATH%

echo Setting up VS2012 environment...
call "%VS110COMNTOOLS%\..\..\VC\vcvarsall.bat"

