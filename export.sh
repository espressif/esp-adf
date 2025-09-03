#!/bin/bash

if [ -z "$ADF_PATH" ]; then
    SCRIPT_PATH="${BASH_SOURCE[0]:-$0}"
    while [ -h "$SCRIPT_PATH" ]; do
        LINK_TARGET="$(readlink "$SCRIPT_PATH")"
        if [ "${LINK_TARGET:0:1}" = "/" ]; then
            SCRIPT_PATH="$LINK_TARGET"
        else
            SCRIPT_PATH="$(dirname "$SCRIPT_PATH")/$LINK_TARGET"
        fi
    done
    SCRIPT_DIR="$(cd "$(dirname "$SCRIPT_PATH")" && pwd)"
    export ADF_PATH="$SCRIPT_DIR"
fi

if [ -z "$IDF_PATH" ]; then
    export IDF_PATH=$ADF_PATH/esp-idf
fi

echo "ADF_PATH: $ADF_PATH"
echo "IDF_PATH: $IDF_PATH"

source ${IDF_PATH}/export.sh || return $?
source "${IDF_PATH}/tools/detect_python.sh"
"${ESP_PYTHON}" "${ADF_PATH}/tools/adf_install_patches.py" apply-patch

echo -e "\nThe following command can be executed now to view detailed usage:"
echo -e ""
echo -e "  idf.py --help"
echo -e "\nCompilation example (The commands highlighted in yellow below are optional: Configure the chip and project settings separately)"
echo -e ""
echo -e "  cd $ADF_PATH/examples/cli"
echo -e "  \033[33midf.py set-target esp32\033[0m"
echo -e "  \033[33midf.py menuconfig\033[0m"
echo -e "  idf.py build"
echo -e ""
