#!/bin/bash

if [ -z "$ADF_PATH" ]; then
    adf_path="."
    # shellcheck disable=SC2128,SC2169,SC2039,SC3054,SC3028 # ignore array expansion warning
    if [ -n "${BASH_SOURCE-}" ]; then
        # shellcheck disable=SC3028,SC3054 # unreachable with 'dash'
        adf_path=$(dirname "${BASH_SOURCE[0]}")
    elif [ -n "${ZSH_VERSION-}" ]; then
        # shellcheck disable=SC2296  # ignore parameter starts with '{' because it's zsh
        adf_path=$(dirname "${(%):-%x}")
    elif [ -n "${ADF_PATH-}" ]; then
        if [ -f "/.dockerenv" ]; then
            echo "Using the ADF_PATH found in the environment as docker environment detected."
            adf_path=$ADF_PATH
        fi
    fi
    export ADF_PATH=$adf_path
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
