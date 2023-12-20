@echo off

:: Infer ADF_PATH from script location
set "ORIG_PATH=%cd%"
set ADF_PATH=%~dp0
set ADF_PATH=%ADF_PATH:~0,-1%

:: Check IDF_PATH
if not defined IDF_PATH (
   set IDF_PATH=%ADF_PATH%\esp-idf
)

echo ADF_PATH: %ADF_PATH%
echo IDF_PATH: %IDF_PATH%

pushd %IDF_PATH% > nul
git submodule update --init --recursive --depth 1
call install.bat
popd > nul
