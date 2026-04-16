#!/usr/bin/env pwsh

<#
PowerShell prebuild script for ESP-GMF example projects.
Provides similar functionality to prebuild.sh: checks IDF_PATH, verifies idf.py, lists/selects targets,
and sets IDF_EXTRA_ACTIONS_PATH in the current PowerShell session.

Usage:
    .\prebuild.ps1
Version: 1.0.0
#>

# -----------------------------------------------------------
# 1. Check IDF environment and setup if needed
# -----------------------------------------------------------
Write-Host "Step 1: Checking IDF environment..."
Write-Host ""

# -----------------------------------------------------------
# IDF version check helper
# Allowed ranges: >=5.4.3 && <5.5.0  OR  >=5.5.2 && <6.0.0
function VerToNum([string]$v) {
    $parts = $v -split '\.'
    $ma = [int]$parts[0]; $mi = [int]$parts[1]; $pa = [int]$parts[2]
    return ($ma * 1000000) + ($mi * 1000) + $pa
}

function Check-IdfVersion {
    Write-Host "Checking IDF version (from header)..."

    if (-not $env:IDF_PATH) {
        Write-Host "ERROR: IDF_PATH not set; cannot locate esp_idf_version.h"
        return $false
    }
    $hdr = Join-Path $env:IDF_PATH 'components\esp_common\include\esp_idf_version.h'
    if (-not (Test-Path $hdr)) {
        Write-Host "ERROR: esp_idf_version.h not found at: $hdr"
        Write-Host "Please ensure ESP-IDF source exists at IDF_PATH and contains components/esp_common/include/esp_idf_version.h"
        return $false
    }

    try {
        $text = Get-Content -Raw -LiteralPath $hdr -ErrorAction Stop
    } catch {
        Write-Host "ERROR: failed to read esp_idf_version.h"
        return $false
    }

    $maj = ([regex]::Match($text, '#\s*define\s+ESP_IDF_VERSION_MAJOR\s+(\d+)')).Groups[1].Value
    $min = ([regex]::Match($text, '#\s*define\s+ESP_IDF_VERSION_MINOR\s+(\d+)')).Groups[1].Value
    $pat = ([regex]::Match($text, '#\s*define\s+ESP_IDF_VERSION_PATCH\s+(\d+)')).Groups[1].Value

    if (-not $maj -or -not $min -or -not $pat) {
        Write-Host "ERROR: esp_idf_version.h does not contain required macros (ESP_IDF_VERSION_MAJOR/MINOR/PATCH)"
        return $false
    }

    $idfVer = "$maj.$min.$pat"

    $vnum = VerToNum $idfVer
    $min1 = VerToNum '5.4.3'
    $max1 = VerToNum '5.5.0'
    $min2 = VerToNum '5.5.2'
    $max2 = VerToNum '6.0.0'

    # Special-case: accept 5.5.1 only if the IDF repo's most recent commit date >= 2025-12
    if ($idfVer -eq '5.5.1') {
        if (-not $env:IDF_PATH) {
            Write-Host "ERROR: IDF_PATH not set; cannot verify commit date for 5.5.1"
            return $false
        }
        $idfRepo = $env:IDF_PATH
        $isGit = $false
        try {
            $isGit = (& git -C $idfRepo rev-parse --is-inside-work-tree 2>$null) -eq 'true'
        } catch {
            $isGit = $false
        }
        if (-not $isGit -and -not (Test-Path (Join-Path $idfRepo '.git'))) {
            Write-Host "ERROR: IDF_PATH ($idfRepo) is not a git repository; cannot verify 5.5.1 commit date"
            return $false
        }
        try {
            $lastCommit = (& git -C $idfRepo log -1 --format=%ci 2>$null) -join "`n"
        } catch {
            $lastCommit = $null
        }
        if (-not $lastCommit) {
            Write-Host "ERROR: unable to read last commit date for IDF at $idfRepo"
            return $false
        }
        $commitDate = ($lastCommit -split '\s+')[0]
        $parts = $commitDate -split '-'
        $commitYear = [int]$parts[0]
        $commitMonth = [int]$parts[1]
        if ($commitYear -gt 2025 -or ($commitYear -eq 2025 -and $commitMonth -ge 12)) {
            Write-Host "IDF version 5.5.1 with commit date $commitDate is accepted"
            return $true
        } else {
            Write-Host "ERROR: IDF 5.5.1 commit date $commitDate is older than 2025-12; unsupported"
            return $false
        }
    }

    if ((($vnum -ge $min1) -and ($vnum -lt $max1)) -or (($vnum -ge $min2) -and ($vnum -lt $max2))) {
        Write-Host "IDF version $idfVer is supported"
        return $true
    } else {
        Write-Host "ERROR: IDF version $idfVer is unsupported. Supported versions: >=5.4.3 && <5.5.0  OR  >=5.5.2 && <6.0.0"
        Write-Host "Please switch IDF to a supported version."
        return $false
    }
}

if (-not $env:IDF_PATH) {
    Write-Host "IDF_PATH not set. Please setup IDF environment first."
    Write-Host "You can set it by running: `$env:IDF_PATH='<path-to-esp-idf>'; [Environment]::SetEnvironmentVariable('IDF_PATH', `$env:IDF_PATH, 'User')"
    return 1
}
$idf_path = $env:IDF_PATH
Write-Host "IDF_PATH is set: $idf_path"
Write-Host ""

# Step 1.2: Check if IDF is properly installed by running idf.py --version
Write-Host "Checking IDF installation by running: idf.py --version"
$idfOk = $false
try {
    & idf.py --version > $null 2>&1
    $idfOk = $true
} catch {
    $idfOk = $false
}

if (-not $idfOk) {
    Write-Host "IDF installation incomplete. Attempting automatic setup..."
    Write-Host ""

    $installPs1 = Join-Path $idf_path 'install.ps1'
    $exportPs1  = Join-Path $idf_path 'export.ps1'

    if (Test-Path $installPs1) {
        Write-Host "Running: $installPs1"
        & $installPs1
        Write-Host "IDF installed successfully"
        Write-Host ""
    } else {
        Write-Host "install.ps1 not found at: $idf_path"
        return 1
    }

    if (Test-Path $exportPs1) {
        Write-Host "Sourcing: $exportPs1"
        . $exportPs1
    } else {
        Write-Host "No export script found at: $idf_path"
        return 1
    }

    # Verify installation
    try {
        & idf.py --version > $null 2>&1
        $idfOk = $true
    } catch {
        $idfOk = $false
    }
    if (-not $idfOk) {
        Write-Host "Failed to verify IDF after installation. Please try manually:"
        return 1
    }

    Write-Host "IDF environment setup successfully"

    # After installing/exporting, verify the IDF version is supported
    if (-not (Check-IdfVersion)) {
        return 1
    }
} else {
    Write-Host "IDF is properly installed"

    # When idf.py was already available, verify the IDF version is supported
    if (-not (Check-IdfVersion)) {
        return 1
    }
}

Write-Host ""

# -----------------------------------------------------------
# 2. Read and select target using idf.py --list-targets
# -----------------------------------------------------------
Write-Host "Step 2: Selecting target..."
Write-Host "Fetching available targets from IDF..."
Write-Host ""

Write-Host "Remove existing components/gen_bmgr_codes directory"
Write-Host ""
& Remove-Item -Path components\gen_bmgr_codes -Recurse -Force -ErrorAction SilentlyContinue

# List targets and filter warnings/blank/path lines
$raw = & idf.py --list-targets 2>&1
$targets = @($raw | Where-Object { $_ -notmatch '^WARNING' -and $_ -notmatch '^\s*/' -and $_ -notmatch '^\s*$' })
if ($targets.Count -eq 0) {
    Write-Host "Failed to get available targets"
    return 1
}

Write-Host "Available targets:"
for ($i=0; $i -lt $targets.Count; $i++) {
    Write-Host "$($i+1)) $($targets[$i])"
}
Write-Host ""

$choice = Read-Host 'Enter a number to select target'
if ($choice -notmatch '^[0-9]+$') {
    Write-Host "Invalid input. Please enter a valid number."
    return 1
}
$idx = [int]$choice - 1
if ($idx -lt 0 -or $idx -ge $targets.Count) {
    Write-Host "Invalid input. Please enter a number between 1 and $($targets.Count)"
    return 1
}
$TARGET = $targets[$idx]
Write-Host "Target selected: $TARGET"
Write-Host "Executing: idf.py set-target $TARGET"
Write-Host ""

# Run the command to set target
& idf.py set-target $TARGET
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to set target"
    return 1
}
Write-Host "Target $TARGET set successfully"
Write-Host ""

# -----------------------------------------------------------
# 3. Set the board manager path
# -----------------------------------------------------------
Write-Host "Step 3: Setting board manager path..."

# Find board manager path (use current folder as base)
$currentDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$cand1 = Join-Path $currentDir 'managed_components\espressif__esp_board_manager'

# Second candidate: walk up from current dir to find a folder named 'esp-gmf',
# then append packages\esp_board_manager. If not found, fall back
# to the previous heuristic (three levels up).
$cand2 = $null
$curr = $currentDir
while ($curr -and ($curr -ne [IO.Path]::GetPathRoot($curr))) {
    if ((Split-Path $curr -Leaf) -eq 'esp-gmf') {
        $cand2 = Join-Path $curr 'packages\esp_board_manager'
        break
    }
    $curr = Split-Path $curr -Parent
}
if (-not $cand2) {
    $cand2 = Join-Path $currentDir '..\..\..\packages\esp_board_manager'
}

$newPath = $null
if (Test-Path (Join-Path $cand1 'gen_bmgr_config_codes.py')) {
    $newPath = (Resolve-Path $cand1).Path
} elseif ($cand2 -and (Test-Path (Join-Path $cand2 'gen_bmgr_config_codes.py'))) {
    $newPath = (Resolve-Path $cand2).Path
}

if (-not $newPath) {
    Write-Host "esp_board_manager not found in the expected directories."
    return 1
}

Write-Host "Found esp_board_manager with gen_bmgr_config_codes.py in: $newPath"
Write-Host ""

# Check if IDF_EXTRA_ACTIONS_PATH is already set in the environment
if ($env:IDF_EXTRA_ACTIONS_PATH) {
    Write-Host "IDF_EXTRA_ACTIONS_PATH already set: $($env:IDF_EXTRA_ACTIONS_PATH)"
    if ($env:IDF_EXTRA_ACTIONS_PATH -ne $newPath) {
        Write-Host "Overriding with new path: $newPath"
    } else {
        Write-Host "✔ Already set to the correct path"
    }
} else {
    Write-Host "Setting IDF_EXTRA_ACTIONS_PATH to: $newPath"
}

# Set environment variable in current session (if dot-sourced this affects caller)
$env:IDF_EXTRA_ACTIONS_PATH = $newPath
[Environment]::SetEnvironmentVariable("IDF_EXTRA_ACTIONS_PATH", $newPath)
Write-Host "IDF_EXTRA_ACTIONS_PATH=$newPath"
Write-Host ""

# -----------------------------------------------------------
# 4. Select board and generate configuration
# -----------------------------------------------------------
Write-Host "Step 4: Selecting board and generating configuration..."
Write-Host ""

# Get board list
& idf.py gen-bmgr-config -l
if ($LASTEXITCODE -ne 0) {
    Write-Host "'idf.py gen-bmgr-config -l' exited with code $LASTEXITCODE"
}
Write-Host ""
Write-Host "If you need modular customization of your own development board, please refer to: https://github.com/espressif/esp-gmf/blob/main/packages/esp_board_manager/docs/how_to_customize_board.md"

# Ask for board selection
Write-Host ""
$board_input = Read-Host "Enter board number or name"

# Set default if empty
if ([string]::IsNullOrWhiteSpace($board_input)) {
    Write-Host "Board_input cannot be empty"
    return 1
}

Write-Host "Board selected: $board_input"
Write-Host "Executing: idf.py gen-bmgr-config -b $board_input"
Write-Host ""

# Generate board configuration
& idf.py gen-bmgr-config -b $board_input
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to generate board configuration"
    return 1
}

Write-Host ""
Write-Host "You can config the project by running 'idf.py menuconfig', then build the project by running 'idf.py build'"
Write-Host ""
