#!/usr/bin/env pwsh
## Usage
# ./export.ps1

## Function Definitions

function ErrorHandling {
    Write-Host "The script encountered an error." -ForegroundColor Red
    exit 1
}

function NewLine {
    Write-Host ""
}

function ShowExampleCommands {
    # Display help and example commands
    Write-Output ""
    Write-Output "The following command can be executed now to view detailed usage:"
    Write-Output ""
    Write-Output "   idf.py --help"
    Write-Output ""
    Write-Output "Compilation example (The commands highlighted in yellow below are optional: Configure the chip and project settings separately):"
    Write-Output ""
    Write-Output "   cd $ADF_PATH\examples\cli"
    Write-Output "   idf.py set-target esp32"
    Write-Output "   idf.py menuconfig"
    Write-Output "   idf.py build"
}

function ExecuteCommand {
    param (
        [string]$command,
        [string]$errorMessage
    )

    try {
        Write-Host "Executing: $command" -ForegroundColor Yellow
        Write-Host "===" -ForegroundColor Yellow
        Invoke-Expression $command
    } catch {
        Write-Host "$errorMessage" -ForegroundColor Red
        ErrorHandling
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Host "$errorMessage, exit code: $LASTEXITCODE" -ForegroundColor Red
        ErrorHandling
    }
}

## Export.ps1 Starts
NewLine

# Set ADF_PATH to the script's directory
$ADF_PATH = Split-Path -Path $MyInvocation.MyCommand.Definition -Parent
$ADF_PATH = $ADF_PATH.TrimEnd('\')

# export $env:ADF_PATH to current terminal
$env:ADF_PATH = "$ADF_PATH"

# export $ADF_PATH to current terminal
Write-Host "Export '`$ADF_PATH: $ADF_PATH' to current terminal" -ForegroundColor Yellow
Set-Variable -Name ADF_PATH -Value $env:ADF_PATH -Scope Global

# Check if IDF_PATH is already defined
if (-not $env:IDF_PATH) {
    # export $env:IDF_PATH to current terminal
    $env:IDF_PATH = Join-Path $ADF_PATH "esp-idf"
    # export $IDF_PATH to current terminal
    Write-Host "Export '`$IDF_PATH: $env:IDF_PATH' to current terminal" -ForegroundColor Yellow
    Set-Variable -Name IDF_PATH -Value $env:IDF_PATH -Scope Global
}

# Display ADF_PATH and IDF_PATH
NewLine
Write-Host "ADF_PATH: $env:ADF_PATH" -ForegroundColor Blue
Write-Host "IDF_PATH: $env:IDF_PATH" -ForegroundColor Blue
NewLine

# Check if export.ps1 exists before calling it
$exportPs1Path = Join-Path $env:IDF_PATH "export.ps1"
if (-Not (Test-Path $exportPs1Path)) {
    Write-Host "ESP-IDF export.ps1 does not exist at path: $exportPs1Path" -ForegroundColor Red
    ErrorHandling
}

# Call ESP-IDF export.ps1
$command = ". $exportPs1Path"
$errorMessage = "The script encountered an error in ESP-IDF export.ps1."
ExecuteCommand $command $errorMessage

# Run the Python script to apply patches
$applyPatchCmd = Join-Path $ADF_PATH "tools\adf_install_patches.py"
$command = "python.exe $applyPatchCmd apply-patch"
$errorMessage = "Failed to execute Python script: $applyPatchCmd"
ExecuteCommand $command $errorMessage

NewLine
ShowExampleCommands
NewLine

# If the script reaches this point, it has completed successfully
Write-Host "Exported successfully!" -ForegroundColor Green
exit 0
