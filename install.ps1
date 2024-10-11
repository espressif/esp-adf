#!/usr/bin/env pwsh

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
        exit 1
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Host "$errorMessage, exit code: $LASTEXITCODE" -ForegroundColor Red
        exit 1
    }
}

$ORIG_PATH = Get-Location

$ADF_PATH = Split-Path -Path $MyInvocation.MyCommand.Definition -Parent
$ADF_PATH = $ADF_PATH.TrimEnd([IO.Path]::DirectorySeparatorChar)

if (-not $env:IDF_PATH) {
    $env:IDF_PATH = [IO.Path]::Combine($ADF_PATH, "esp-idf")
}

Write-Host "ADF_PATH: $ADF_PATH"
Write-Host "IDF_PATH: $env:IDF_PATH"

Push-Location $env:IDF_PATH

$command = "git submodule update --init --recursive --depth 1"
$errorMessage = "IDF submodules update failed."
ExecuteCommand $command $errorMessage

$installScriptPath = [IO.Path]::Combine($IDF_PATH, "install.ps1")
if (Test-Path $installScriptPath) {
    Write-Host "Calling install.ps1..."
    $command = '& $installScriptPath'
    $errorMessage = "Install idf tools failed."
    ExecuteCommand $command $errorMessage
} else {
    Write-Host "install.ps1 not found at path: $installScriptPath" -ForegroundColor Red
    exit 1
}

Pop-Location
