@echo off

set "ORIG_PATH=%cd%"
set ADF_PATH=%~dp0
set ADF_PATH=%ADF_PATH:~0,-1%

:: Check IDF_PATH
if not defined IDF_PATH (
   set IDF_PATH=%ADF_PATH%\esp-idf
)

echo ADF_PATH: %ADF_PATH%
echo IDF_PATH: %IDF_PATH%

call %IDF_PATH%\export.bat
if %errorlevel% neq 0 (goto :ErrorHandling)
python.exe "%ADF_PATH%\tools\adf_install_patches.py" apply-patch
if %errorlevel% neq 0 (goto :ErrorHandling)

echo.
echo The following command can be executed now to view detailed usage:
echo.
echo   idf.py --help
echo.
echo Compilation example (The commands highlighted in yellow below are optional: Configure the chip and project settings separately)
echo.
echo   cd %ADF_PATH%\examples\cli
echo   [33midf.py set-target esp32[0m
echo   [33midf.py menuconfig[0m
echo   idf.py build
echo.

:ErrorHandling
echo The script encountered an error.
goto :eof
