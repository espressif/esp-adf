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
try {
    Write-Host "Calling ESP-IDF export.ps1..." -ForegroundColor Yellow
    Write-Host "===" -ForegroundColor Yellow
    . $exportPs1Path
} catch {
    Write-Host "Failed to execute ESP-IDF export.ps1." -ForegroundColor Red
    ErrorHandling
}

# Check the exit code of export.ps1
if ($LASTEXITCODE -ne 0) {
    Write-Host "The script encountered an error in ESP-IDF export.ps1." -ForegroundColor Red
    Write-Host "ESP-IDF export.ps1 exit code: $LASTEXITCODE" -ForegroundColor Red
    ErrorHandling
} else {
    Write-Host "ESP-IDF export.ps1 executed successfully!" -ForegroundColor Green
}

# Run the Python script to apply patches
$applyPatchCmd = Join-Path $ADF_PATH "tools\adf_install_patches.py"
try {
    NewLine
    Write-Host "Calling 'python.exe $applyPatchCmd'" -ForegroundColor Yellow
    Write-Host "===" -ForegroundColor Yellow
    python.exe "$applyPatchCmd" apply-patch
} catch {
    Write-Host "Failed to execute adf_install_patches.py." -ForegroundColor Red
    ErrorHandling
}

# Check the exit code of the Python script
if ($LASTEXITCODE -ne 0) {
    Write-Host "The script encountered an error while applying adf patches." -ForegroundColor Red
    Write-Host "adf_install_patches.py exit code: $LASTEXITCODE" -ForegroundColor Red
    ErrorHandling
} else {
    NewLine
    Write-Host "Patch installed successfully!" -ForegroundColor Green
}

NewLine
ShowExampleCommands
NewLine

# If the script reaches this point, it has completed successfully
Write-Host "Exported successfully!" -ForegroundColor Green
exit 0
