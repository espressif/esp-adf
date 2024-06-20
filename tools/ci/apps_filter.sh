#!/bin/bash
# cat $ADF_PATH/tools/ci/apps.json | jq length
# cat $ADF_PATH/tools/ci/apps.json | jq ".[0].board.esp32.ESP_LYRAT_V4_3"
# echo "v24.6.2222" | grep -o "[0-9].*"
#
# Note:
#   $1 esp32
#   $2 ESP_LYRAT_V4_3
#   $3 v4.4 or release/v4.4
#   $4 apps.json
#   $5 audio_board_idf.json
#
# Usage:
#   source $ADF_PATH/tools/ci/apps_filter.sh $IDF_TARGET $AUDIO_BOARD $IDF_VERSION_TAG $ADF_PATH/tools/ci/apps.json $ADF_PATH/tools/ci/audio_board_idf.json

if ! type gettext >/dev/null 2>&1;then
    apt-get update
    apt-get install gettext -y
else
    echo "The gettext tool has been installed"
fi

idf_target=$1
audio_hal=$2
idf_version_tag=$3
apps_json_path=$4
audio_board_idf_json_path=$5

len=(`cat $apps_json_path | jq length`)

app_num=0
app_path=0
EXAMPLES=''

function idf_version_control() {
    arr=($@)
    for ((i = 0; i < ${#arr[@]}; i ++)) do
        idf=(${arr//-/ })
        for (( i = 0; i < ${#idf[@]}; i ++)) do
            if [ "${idf[i]:1}" = "${idf_version_tag:1}" ]; then
                app_path=$(cat $apps_json_path | jq -r ".[$num].app_dir")
                echo $app_path >> apps.txt
                EXAMPLES+=$app_path" "
                echo $app_path
                break
            fi
            idf[i]=$(echo "${idf[i]}" | grep -o "[0-9].*")
        done;

        if [ "$app_path" = "" ]; then
            version=$(echo $idf_version_tag | grep -o "[0-9].*")
            if [ $(echo $version" > "${idf[0]} | bc) = 1 ] && [ $(echo $version" < "${idf[1]} | bc) = 1 ]; then
                app_path=$(cat $apps_json_path | jq -r ".[$num].app_dir")
                echo $app_path >> apps.txt
                EXAMPLES+=$app_path" "
                echo $app_path
            fi
        else
           break
        fi
    done
}

for num in $(seq 0 $len); do
    app_path=""
    chip=$(cat $apps_json_path | jq ".[$num].board.$idf_target")
    if [ "$chip" = "null" ]; then
        echo "skip"
    else
        if [[ "$chip" =~ "$audio_hal" ]]; then
            board=$(envsubst < $audio_board_idf_json_path | jq -r ".$audio_hal")
            if [ "$board" != "null" ]; then
                idf_vers=$(echo $board | tr ',' ' ')
                idf_version_control ${idf_vers[@]}
            else
                echo "skip"
            fi
        else
            echo "skip"
        fi
    fi
done
