@ECHO OFF

DEL /Q failed_tests.log debug_tests.log

DEL /Q %~dp0\regression_tests\blockchain_export_fork_graph\*.gv
DEL /Q %~dp0\regression_tests\wallet_asset\*.json
DEL /Q %~dp0\regression_tests\wallet_backup\wallet_backup.json

SET /A count_tests=0
SET /A count_failed=0

SETLOCAL EnableDelayedExpansion

FOR /F %%T IN ('DIR /B /AD regression_tests\*^| findstr /vb _') DO (
  ECHO RUN: regression_tests\%%T ...
  wallet_tests -t regression_tests_without_network/%%T >> debug_tests.log 2>&1
  SET /A count_tests+=1
  IF !ERRORLEVEL! neq 0 (
    CALL :TestFailed %%T !ERRORLEVEL!
  ) ELSE (
	CALL :TestPassed %%T
  )
)
ECHO Total tests: %count_tests%
ECHO Failed tests: %count_failed%
GOTO :EOF

:TestFailed
ECHO %1 >> failed_tests.log
ECHO FAILED: regression_tests\%1 with ERRORLEVEL %2
SET /A count_failed+=1
GOTO :EOF

:TestPassed
ECHO PASS: regression_tests\%1
GOTO :EOF
