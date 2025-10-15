@echo off
REM Simple launcher to run the C++ CLI without PowerShell
setlocal
REM Try release first, then debug, then top-level build
set "ROOT=%~dp0..\cpp"
set "CAND1=%ROOT%\build\Release\al-muslim.exe"
set "CAND2=%ROOT%\build\al-muslim.exe"
set "CAND3=%ROOT%\build-mingw\al-muslim.exe"

if exist "%CAND1%" (
  "%CAND1%" %*
) else if exist "%CAND2%" (
  "%CAND2%" %*
) else if exist "%CAND3%" (
  "%CAND3%" %*
) else (
  echo Could not find al-muslim.exe. Please build it first.
  echo Expected paths:
  echo   %CAND1%
  echo   %CAND2%
  echo   %CAND3%
  pause
  exit /b 1
)

endlocal