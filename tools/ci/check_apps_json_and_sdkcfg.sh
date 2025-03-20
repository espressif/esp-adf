#!/usr/bin/env bash
#
# This script is used to check the sdkconfig.defaults / sdkconfig.defaults.xxx & apps.yaml.
# The first parameter is the name of the configuration file to check.
# The second parameter is the name of the development board to be checked

[ -z ${IDF_PATH} ] && die "IDF_PATH is not set"
[ -z ${ADF_PATH} ] && die "ADF_PATH is not set"

set +e

if ! type yq >/dev/null 2>&1;then
    wget https://github.com/mikefarah/yq/releases/download/v4.44.5/yq_linux_amd64 -O /usr/local/bin/yq && chmod +x /usr/local/bin/yq
    yq --version
else
    echo "The yq tool has been installed"
fi

cat apps.txt | grep -o 'examples/.*' > check_apps_json_and_sdk.txt
BOARD_NAME=(`cat $ADF_PATH/tools/ci/audio_board_idf.json | grep -E "ESP+[0-9a-zA-Z_]+" -o | sort -u -r`)
APPS_CHECK_RESULT="OK"

function check_sdkcfg() {
    CHIP_TYPES=(`echo $audio_boards | grep -E "esp+[0-9a-z]+" -o | sort -u`)
    if [ ! -f $ADF_PATH"/${line}/sdkconfig.defaults" ]; then
        echo -e "\e[31m ERROR: sdkconfig.defaults is missing in " $line " \e[0m"
        APPS_CHECK_RESULT="FAIL"
    fi

    for chip in ${CHIP_TYPES[*]}
    do
        if [ ! -f $ADF_PATH"/${line}/sdkconfig.defaults."$chip ]; then
            echo -e "\e[31m ERROR: sdkconfig.defaults." $chip "is missing in " $line " \e[0m"
            APPS_CHECK_RESULT="FAIL"
        fi
    done
}

function check_audio_board() {
    for board_type in ${BOARD_NAME[*]}
    do
        result=$(echo $audio_boards | grep "${board_type}")
        if [ -n "$result" ];then
            # At least one valid audio board is defined after this example
            # Audio board check passed
            break
        fi
    done

    if [ ! -n "$result" ];then
        echo "The error occurs in the definition of" ${line} "in apps.yaml"
        echo -e "\e[31m ERROR: not found support board \e[0m"
        APPS_CHECK_RESULT="FAIL"
    fi
}

function update_audio_hal() {
    if [ -f $ADF_PATH"/${line}/"$1 ]; then
        old_config=$(sed -n '/_BOARD=y/p' $ADF_PATH"/${line}/"$1)
        if [ ! -z ${old_config} ]; then
            new_config="CONFIG_"$2"_BOARD=y"
            sed -i "s%${old_config}%${new_config}%" $ADF_PATH"/${line}/"$1
            # It is easy to see the change of audio_hal
            # echo $ADF_PATH"/${line}/"$1 "AUDIO_BOARD:"$old_config " --> " $new_config
        else
            sed -i '$a\CONFIG_'$2'_BOARD=y' $ADF_PATH"/${line}/"$1
        fi
    else
        echo -e "\e[33m WARNING: " $ADF_PATH"/${line}/"$1 " skip \e[0m"
    fi
}

while read line
do
    if yq eval ".APPS[] | select(.app_dir == \"$line\")" ${ADF_PATH}/tools/ci/apps.yaml > /dev/null; then
        audio_boards=$(yq eval ".APPS[] | select(.app_dir == \"$line\") | .board" ${ADF_PATH}/tools/ci/apps.yaml)

        # Check if the example's file(sdkconfg.defaults.xxx / sdkconfg.defaults) is missing
        check_sdkcfg
        # Does the example define supported board types
        check_audio_board
        # Update AUDIO_BOARD
        update_audio_hal $1 $2
    else
        echo -e "\e[31m ERROR:" $line "exist but not found it in apps.json \e[0m"
        APPS_CHECK_RESULT="FAIL"
    fi
done < check_apps_json_and_sdk.txt

if [ "$APPS_CHECK_RESULT" = "OK" ];then
    echo "Check passed"
elif [ "$APPS_CHECK_RESULT" = "FAIL" ];then
    echo "Check failure"
    exit 1
fi
set -e
