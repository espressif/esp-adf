#!/bin/bash

# Prebuild configuration script for ESP-GMF example projects.
# Compatibility: bash 4.0+, zsh
# Usage:
#   source ./prebuild.sh    # Source to set environment in current shell

# -----------------------------------------------------------
# 1. Check IDF environment and setup if needed
# -----------------------------------------------------------
echo "Step 1: Checking IDF environment..."
echo ""

# Step 1.1: Check if IDF_PATH is set
if [ -z "${IDF_PATH:-}" ]; then
    echo "IDF_PATH not set. Please setup IDF environment first."
    echo "You can set it by running: export IDF_PATH=<path-to-esp-idf>"
    return 1
fi

idf_path="${IDF_PATH}"
echo "IDF_PATH is set: $idf_path"
echo ""

# Step 1.2: Check if IDF is properly installed by running idf.py --version
echo "Checking IDF installation by running: idf.py --version"
if idf.py --version > /dev/null 2>&1; then
    echo "IDF is properly installed"
else
    # Step 1.3: IDF not properly installed, attempt automatic installation
    echo "IDF installation incomplete. Attempting automatic setup..."
    echo ""
    
    install_script="$idf_path/install.sh"
    export_script="$idf_path/export.sh"
    
    # Check if install.sh exists
    if [ ! -f "$install_script" ]; then
        echo "install.sh not found at: $install_script"
        return 1
    fi
    
    # Run install.sh
    echo "Running: $install_script"
    bash "$install_script"
    echo "IDF installed successfully"
    echo ""
    
    # Source the export.sh to update environment
    echo "Sourcing: $export_script"
    source "$export_script"
    
    # Verify installation
    if ! idf.py --version > /dev/null 2>&1; then
        echo "Failed to verify IDF after installation. Please try manually:"
        echo "source $export_script"
        return 1
    fi
    
    echo "IDF environment setup successfully"
fi

echo ""

# -----------------------------------------------------------
# 2. Read and select target using idf.py --list-targets
# -----------------------------------------------------------
echo "Step 2: Selecting target..."
echo "Fetching available targets from IDF..."
echo ""

# Get list of available targets from idf.py
targets=$(idf.py --list-targets 2>&1 | grep -v "^WARNING" | sed -e '/^[[:space:]]*\//{d}' -e '/^[[:space:]]*$/{d}')

if [ $? -ne 0 ]; then
    echo "Failed to get available targets"
    echo "Error: $targets"
    return 1
fi

# Convert to array
available_targets=()
while IFS= read -r line; do
    # skip empty lines
    [ -z "$line" ] && continue
    available_targets+=("$line")
done <<< "$targets"

# Display available targets
echo "Available targets:"
i=1
for tgt in "${available_targets[@]}"; do
    echo "$i) $tgt"
    i=$((i + 1))
done
echo ""

# Get user selection
printf "Enter a number to select target: "
read target_choice

# Validate input
if ! [[ "$target_choice" =~ ^[0-9]+$ ]]; then
    echo "Invalid input. Please enter a valid number."
    return 1
fi

num_targets=${#available_targets[@]}

if [ "$target_choice" -lt 1 ] || [ "$target_choice" -gt "$num_targets" ]; then
    echo "Invalid input. Please enter a number between 1 and $num_targets"
    return 1
fi

# Get the selected target in a shell-agnostic way
TARGET=""
i=1
for tgt in "${available_targets[@]}"; do
    if [ "$i" -eq "$target_choice" ]; then
        TARGET="$tgt"
        break
    fi
    i=$((i + 1))
done

echo "Target selected: $TARGET"
echo "Executing: idf.py set-target $TARGET"
echo ""

# Run the command to set target
if ! idf.py set-target "$TARGET"; then
    echo "Failed to set target"
    return 1
fi

echo "Target $TARGET set successfully"
echo ""

# -----------------------------------------------------------
# 3. Set the board manager path
# -----------------------------------------------------------
echo "Step 3: Setting board manager path..."

# If running under zsh, use zsh's special expansion to get the sourced script path.
# We use eval with a single-quoted string so bash won't try to interpret zsh syntax.
if [ -n "${ZSH_VERSION:-}" ]; then
    eval '_script_path="${(%):-%x}"'
elif [ -n "${BASH_SOURCE:-}" ]; then
    _script_path="${BASH_SOURCE[0]}"
else
    _script_path="$0"
fi

if [ -z "$_script_path" ] || [ "$_script_path" = "$SHELL" ]; then
    SCRIPT_DIR="$(pwd)"
else
    SCRIPT_DIR="$(cd "$(dirname "$_script_path")" && pwd)"
fi

# Candidates for board manager path
candidates=(
    "$SCRIPT_DIR/managed_components/espressif__esp_board_manager"
    "$(cd "$SCRIPT_DIR/../../../" && pwd)/packages/esp_board_manager"
)

# Function to search for board manager path
search_board_manager_path() {
    for path in "${candidates[@]}"; do
        if [ -d "$path" ]; then
            # Deep check: look for gen_bmgr_config_codes.py
            required_file="$path/gen_bmgr_config_codes.py"
            if [ -f "$required_file" ]; then
                # Only echo the path (return value), use stderr for messages
                echo "$path"
                return 0
            else
                echo "Found esp_board_manager at $path, but gen_bmgr_config_codes.py not found. Skipping..." >&2
            fi
        fi
    done
    
    echo "esp_board_manager not found in the expected directories." >&2
    return 1
}

# Find the board manager path
NEW_IDF_EXTRA_ACTIONS_PATH=$(search_board_manager_path)
if [ $? -ne 0 ]; then
    return 1
fi

echo "Found esp_board_manager with gen_bmgr_config_codes.py in: $NEW_IDF_EXTRA_ACTIONS_PATH"

echo ""

# Check if IDF_EXTRA_ACTIONS_PATH is already set in the environment
if [ -n "${IDF_EXTRA_ACTIONS_PATH:-}" ]; then
    echo "IDF_EXTRA_ACTIONS_PATH already set: ${IDF_EXTRA_ACTIONS_PATH}"
    if [ "${IDF_EXTRA_ACTIONS_PATH}" != "$NEW_IDF_EXTRA_ACTIONS_PATH" ]; then
        echo "Overriding with new path: $NEW_IDF_EXTRA_ACTIONS_PATH"
    else
        echo "Already set to the correct path"
    fi
else
    echo "Setting IDF_EXTRA_ACTIONS_PATH to: $NEW_IDF_EXTRA_ACTIONS_PATH"
fi

# Set the environment variable
IDF_EXTRA_ACTIONS_PATH="$NEW_IDF_EXTRA_ACTIONS_PATH"
export IDF_EXTRA_ACTIONS_PATH
echo "IDF_EXTRA_ACTIONS_PATH=$IDF_EXTRA_ACTIONS_PATH"
echo ""

idf.py gen-bmgr-config -l
echo ""
echo "Prebuild configuration completed successfully!"
echo ""
echo "You can now select board from above or customed board"
echo ""
echo "Take ESP32-S3-Korvo2 V3.1 for example, run 'idf.py gen-bmgr-config -b esp32_s3_korvo2_v3'"
echo ""
echo "After selecting board, you can build the project by running 'idf.py build'"
