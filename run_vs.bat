@echo off
call %~dp0\setenv.bat

where WDExpress.exe
if %ERRORLEVEL% == 0 (
    start WDExpress.exe 
) else (
    echo WDExpress.exe wasn't found

    where devenv.com
    if %ERRORLEVEL% == 0 (
        start devenv.com
    ) else (
        echo devenv.com wasn't found
    )
)
