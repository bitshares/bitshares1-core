@ECHO OFF

DEL /Q failed_tests.log debug_tests.log

SETLOCAL EnableDelayedExpansion

FOR /F %%T IN ('DIR /B /AD regression_tests\*^| findstr /vb _') DO (
  ECHO RUN: regression_tests\%%T ...
  wallet_tests -t regression_tests_without_network/%%T >> debug_tests.log 2>&1
  IF !ERRORLEVEL! neq 0 (
    CALL :TestFailed %%T !ERRORLEVEL!
  ) ELSE (
	CALL :TestPassed %%T
  )
)
GOTO :EOF

:TestFailed
ECHO %1 >> failed_tests.log
ECHO FAILED: regression_tests\%1 with ERRORLEVEL %2
GOTO :EOF

:TestPassed
ECHO PASS: regression_tests\%1
GOTO :EOF
