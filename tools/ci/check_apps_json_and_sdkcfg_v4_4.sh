#!/usr/bin/env bash
#
# This script is used to check the sdkconfig.defaults / sdkconfig.defaults.xxx & apps_v4_4.json.
# The first parameter is the name of the configuration file to check.
# The second parameter is the name of the development board to be checked

[ -z ${IDF_PATH} ] && die "IDF_PATH is not set"
[ -z ${ADF_PATH} ] && die "ADF_PATH is not set"
[ -z ${BUILD_PATH} ] && die "BUILD_PATH is not set"

set +e

export LIST_JSON_PATH=${BUILD_PATH}/list.json

if ! type jq >/dev/null 2>&1;then
    apt-get update
    apt-get install jq -y
else
    echo "The jq tool has been installed"
fi

jq -r .app_dir ${LIST_JSON_PATH} > examples.txt
cat examples.txt | grep -o '/examples/.*' > check_apps_json_and_sdk.txt
BOARD_NAME=(`cat $ADF_PATH/tools/ci/apps_v4_4.json | grep -E "ESP+[0-9a-zA-Z_]+" -o | sort -u -r`)
APPS_CHECK_RESULT="OK"

function check_sdkcfg() {
    CHIP_TYPES=(`echo $audio_boards | grep -E "esp+[0-9a-z]+" -o | sort -u`)
    if [ ! -f $ADF_PATH${line}"/sdkconfig.defaults" ]; then
        echo -e "\e[31m ERROR: sdkconfig.defaults is missing in " $line " \e[0m"
        APPS_CHECK_RESULT="FAIL"
    fi

    for chip in ${CHIP_TYPES[*]}
    do
        if [ ! -f $ADF_PATH${line}"/sdkconfig.defaults."$chip ]; then
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
        echo "The error occurs in the definition of" ${line} "in apps_v4_4.json"
        echo -e "\e[31m ERROR: not found support board \e[0m"
        APPS_CHECK_RESULT="FAIL"
    fi
}

function check_audio_board_idf_ver() {
    result=${result%;*}
    chip_board_idf_ver_list=(`echo $result | tr ';' ' '` )
    for chip_board_idf_ver in ${chip_board_idf_ver_list[*]}
    do
        idf_ver=$(echo $chip_board_idf_ver | grep '\[v[0-9][0-9]' -o)
        if [ ! -n "$idf_ver" ];then
            echo "The error occurs in the definition of" ${line} "in apps_v4_4.json"
            echo -e "\e[31m ERROR: not found" $chip_board_idf_ver "support idf version \e[0m"
            APPS_CHECK_RESULT="FAIL"
        fi
    done
}

function update_audio_hal() {
    if [ -f $ADF_PATH${line}"/"$1 ]; then
        old_config=$(sed -n '/_BOARD=y/p' $ADF_PATH${line}"/"$1)
        if [ ! -z ${old_config} ]; then
            new_config="CONFIG_"$2"_BOARD=y"
            sed -i "s%${old_config}%${new_config}%" $ADF_PATH${line}"/"$1
            # It is easy to see the change of audio_hal
            # echo $ADF_PATH${line}"/"$1 "AUDIO_BOARD:"$old_config " --> " $new_config
        else
            sed -i '$a\CONFIG_'$2'_BOARD=y' $ADF_PATH${line}"/"$1
            cat $ADF_PATH${line}"/"$1
        fi
    else
        echo -e "\e[32m " $ADF_PATH${line}"/"$1 " skip \e[0m"
    fi
}

while read line
do
    result=$(cat ${ADF_PATH}/tools/ci/apps_v4_4.json | grep -E $line)
    if [ -n "$result" ];then
        audio_boards=${result#*:*:}

        # Check if the example's file(sdkconfg.defaults.xxx / sdkconfg.defaults) is missing
        check_sdkcfg
        # Does the example define supported board types
        check_audio_board
        # Check if idf version is defined
        check_audio_board_idf_ver

        # Update AUDIO_BOARD
        update_audio_hal $1 $2

    else
        echo -e "\e[31m ERROR:" $line "exist but not found it in apps_v4_4.json \e[0m"
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
