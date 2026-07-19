@ECHO OFF
REM Launcher for backfill_translations.py, mirroring sync.bat so it reuses the same
REM Python setup (common_python.bat) and runs from the mpcresources directory.

SETLOCAL
PUSHD %~dp0

SET SILENT=%1

CALL "common_python.bat"
IF %ERRORLEVEL% NEQ 0 GOTO END

python.exe backfill_translations.py

:END
IF NOT DEFINED SILENT (
  PAUSE
)
IF %ERRORLEVEL% NEQ 0 (
  ENDLOCAL
  EXIT /B 1
) ELSE (
  ENDLOCAL
  EXIT /B
)
