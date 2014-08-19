rem this script should be spawned from directory containing TCL sources.
rem built library will be installed to %INVICTUS_ROOT%/tcl

set INVICTUS_ROOT=%~dp0..\..\
echo Using %INVICTUS_ROOT% as Invictus root directory

call "%VS120COMNTOOLS%\..\..\VC\vcvarsall.bat" amd64

pushd win
nmake -f makefile.vc release || exit /b 1
nmake -f makefile.vc release install INSTALLDIR="%INVICTUS_ROOT%\tcl.x64" || exit /b 2
nmake -f makefile.vc OPTS=symbols || exit /b 3
nmake -f makefile.vc OPTS=symbols install INSTALLDIR="%INVICTUS_ROOT%\tcl.x64" || exit /b 4
popd

