@echo off
set BITSHARES_ROOT=%~dp0..\
echo Using %BITSHARES_ROOT% as Bitshare root directory

set OPENSSL_ROOT=%BITSHARES_ROOT%\OpenSSL
set OPENSSL_ROOT_DIR=%OPENSSL_ROOT%
set OPENSSL_INCLUDE_DIR=%OPENSSL_ROOT%\include
set DBROOTDIR=%BITSHARES_ROOT%\BerkeleyDB

set BOOST_ROOT=%BITSHARES_ROOT%\boost_1.55

set PATH=%BITSHARES_ROOT%\bin;%BITSHARES_ROOT%\Cmake\bin;%BITSHARES_ROOT%\boost\stage\lib;%PATH%

echo Setting up VS2013 environment...
call "%VS120COMNTOOLS%\..\..\VC\vcvarsall.bat"

