rem this script should be spawned from directory containing TCL sources.
rem built library will be installed to %INVICTUS_ROOT%/tcl
rem potential problems to solve: manual elimination of -debug:full option from files: makefile.vc (replace to -debug)
rem manual elimination of -WX switches from makefile.vc since in debug build warnings are generated.

setlocal

set TCL_VERSION=8.6.1

set INVICTUS_ROOT=%~dp0..\..
echo Using %INVICTUS_ROOT% as Invictus root directory

call "%VS120COMNTOOLS%\..\..\VC\vcvarsall.bat"

rem goto buildTK
echo "Building TCL library"

set CL=/MP
set LINK=/DEBUG

rem goto buildTK

pushd win
echo Building release version

nmake -f makefile.vc clean
nmake -f makefile.vc release || exit /b 1
nmake -f makefile.vc release install INSTALLDIR="%INVICTUS_ROOT%\tcl" || exit /b 2

echo Building DEBUG version

nmake -f makefile.vc clean OPTS=symbols 
nmake -f makefile.vc OPTS=symbols || exit /b 3
nmake -f makefile.vc install OPTS=symbols INSTALLDIR="%INVICTUS_ROOT%\tcl" || exit /b 4

popd

:buildTK
echo "Building TK library"
set TCLDIR=%CD%
set INSTALLDIR=%INVICTUS_ROOT%\tcl
pushd ..\tk%TCL_VERSION%\win
rem call buildall.vc.bat || exit 10

nmake -f makefile.vc TCLDIR=%TCLDIR% || exit /b 5
nmake -f makefile.vc TCLDIR=%TCLDIR% install INSTALLDIR="%INVICTUS_ROOT%\tcl" || exit /b 6
nmake -f makefile.vc OPTS=symbols TCLDIR=%TCLDIR% || exit /b 7
nmake -f makefile.vc OPTS=symbols install TCLDIR=%TCLDIR% INSTALLDIR="%INVICTUS_ROOT%\tcl" || exit /b 8
popd

rem Copy generated binaries to the names been accepted by CMake (it doesn't recognize names suffixed by 't')
pushd %INVICTUS_ROOT%\tcl\bin
copy /Y tclsh86t.exe tclsh.exe
copy /Y tclsh86t.exe tclsh86.exe
copy /Y wish86t.exe wish86.exe
copy /Y wish86t.exe wish.exe
popd

pushd %INVICTUS_ROOT%\tcl\lib
copy /Y tcl86t.lib tcl86.lib
copy /Y tcl86tg.lib tcl86g.lib
copy /Y tk86t.lib tk86.lib
copy /Y tk86tg.lib tk86g.lib
pushd

:end
endlocal

