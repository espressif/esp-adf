#!/usr/bin/env bash
#
# This script is used to check the sdkconfig.defaults / sdkconfig.defaults.xxx & apps.json.
# The first parameter is the name of the configuration file to check.
# The second parameter is the name of the development board to be checked

[ -z ${IDF_PATH} ] && die "IDF_PATH is not set"
[ -z ${ADF_PATH} ] && die "ADF_PATH is not set"

find ${ADF_PATH}"/examples" ! -path "*/managed_components/*" ! -path "*/components/*" -type f -name "Makefile" > examples.txt
sed -i "s/\/Makefile//g" examples.txt
cat examples.txt | grep -o '/examples/.*' > check_apps_json_and_sdk.txt
BOARD_NAME=(`cat $ADF_PATH/tools/ci/apps.json | grep -E "ESP+[0-9a-zA-Z_]+" -o | sort -u -r`)

check_sdkcfg() {
    CHIP_TYPES=(`echo $audio_boards | grep -E "esp+[0-9a-z]+" -o | sort -u`)

    if [ ! -e $ADF_PATH${line}"/sdkconfig.defaults" ]; then
        echo -e "\e[31m Error: sdkconfig.defaults is missing in \e[0m" $line
        SCRIPT_RESULT="FAIL"
    fi

    for chip in ${CHIP_TYPES[*]}
    do
        if [ ! -e $ADF_PATH${line}"/sdkconfig.defaults."$chip ]; then
            echo -e "\e[31m Error: sdkconfig.defaults." $chip "is missing in \e[0m" $line
            SCRIPT_RESULT="FAIL"
        fi
    done

    if [ -n "$SCRIPT_RESULT" ]; then
        echo -e "\033[33m Check sdkconfig.defaults failed \033[0m"
        exit 1
    fi
}

check_audio_board() {
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
        echo "The error occurs in the definition of" ${line} "in apps.json"
        echo "\e[31m Error: not found support board \e[0m"
        exit 1
    fi
}

check_audio_board_idf_ver() {
    result=${result%;*}
    chip_board_idf_ver_list=(`echo $result | tr ';' ' '` )
    for chip_board_idf_ver in ${chip_board_idf_ver_list[*]}
    do
        idf_ver=$(echo $chip_board_idf_ver | grep '\[v[0-9][0-9]' -o)
        if [ ! -n "$idf_ver" ];then
            echo "The error occurs in the definition of" ${line} "in apps.json"
            echo "\e[31m Error: not found" $chip_board_idf_ver "support idf version \e[0m"
            exit 1
        fi
    done
}

update_audio_hal() {
    if [ -e $ADF_PATH${line}"/"$1 ]; then
        old_config=$(sed -n '/_BOARD=y/p' $ADF_PATH${line}"/"$1)
        if [ ! -z ${old_config} ]; then
            new_config="CONFIG_"$2"_BOARD=y"
            sed -i "s%${old_config}%${new_config}%" $ADF_PATH${line}"/"$1
            echo "old config:"$old_config
            echo "new config:"$new_config
        else
            sed -i '$a\CONFIG_'$2'_BOARD=y' $ADF_PATH${line}"/"$1
            cat $ADF_PATH${line}"/"$1
        fi
    fi
}

while read line
do
    result=$(cat ${ADF_PATH}/tools/ci/apps.json | grep -E $line)
    if [ -n "$result" ];then
        audio_boards=${result#*:*:}

        # Check if the example's file(sdkconfg.defaults.xxx / sdkconfg.defaults) is missing
        check_sdkcfg
        # Does the example define supported board types
        check_audio_board
        # Check if idf version is defined
        check_audio_board_idf_ver

        # Update AUDIO_HAL
        update_audio_hal $1 $2

    else
        echo "\e[31m Error:" $line "exist but not found it in apps.json \e[0m"
        exit 1
    fi
done < check_apps_json_and_sdk.txt

echo "Check passed"
