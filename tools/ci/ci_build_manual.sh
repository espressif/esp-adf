#!/usr/bin/env bash
#
# This script is used to trigger CI tests for other module branches.
# usage:
#   Parameter option '-t': ADF Branch to be tested.
#   Parameter option '-m': The module name and target commit id to be switched on the CI.
#   Parameter option '-l': Labels of test items to be performed. The default is example_test and unit_test. This setup options allow is ignored.
# eg: `$ADF_PATH/tools/ci/adf_csb.sh -t ci/update_submodules_address -m adf-libs-ut:831305ba6a7f28e326a46467356c46bcd974da36 -m esp-adf-libs:e55b4e0c22548983a4d3f1f990702db34f86fb5a`.

[ -z ${ADF_PATH} ] && die "ADF_PATH is not set"

while getopts "t:m:l:h" optname
do
    case "$optname" in
        "t")
          TEST_ADF_BRANCH=$OPTARG;;
        "m")
          TEST_MODULE_BRANCH+=($OPTARG);;
        "l")
          CI_LABELS=$OPTARG;;
        "h")
          echo "Parameter option '-t': ADF Branch to be tested.(eg: -t master)"
          echo "Parameter option '-m': The module name and target commit id to be switched on the CI.(eg: -m esp-idf:release/v4.4)"
          echo "Parameter option '-l': Labels of test items to be performed. The default is example_test and unit_test. This setup options allow is ignored."
          ;;
        ":")
          echo "No argument value for option $OPTARG";;
        "?")
          echo "Unknow option $OPTARG";;
        "*")
          echo "Error";;
    esac
done

CI_LABELS=${CI_LABELS:-example_test,unit_test}
CI_AUDIO_BOT_TOKEN=${CI_AUDIO_BOT_TOKEN:-4af3ca9871d7a0b1824c9355820dc8}

if [ -z ${TEST_ADF_BRANCH} ]; then
    echo -e "\e[31m ERROR: ADF Branch not set. \e[0m"
    exit 1
elif [ -z ${TEST_MODULE_BRANCH} ]; then
    echo -e "\e[31m ERROR: Module name and Target commit id not set. \e[0m"
    exit 1
fi

for (( i = 0; i < ${#TEST_MODULE_BRANCH[@]}; i ++ )) do
    MODULE_BRANCH_JSON=$MODULE_BRANCH_JSON'"'${TEST_MODULE_BRANCH[i]}'"'
    if [ ! $i -eq $(( ${#TEST_MODULE_BRANCH[@]} - 1 )) ]; then
        MODULE_BRANCH_JSON=$MODULE_BRANCH_JSON","
    fi
done

MODULE_BRANCH_JSON=$(echo '{'$MODULE_BRANCH_JSON'}' | sed -e 's/:/\":"/g')

curl -X POST -F token=${CI_AUDIO_BOT_TOKEN} -F "ref=${TEST_ADF_BRANCH}" -F "variables[ESP_ADF_MODULES]=${MODULE_BRANCH_JSON}" -F "variables[CI_PIPELINE_SOURCE]=merge_request_event" \
-F "variables[CI_MERGE_REQUEST_LABELS]=${CI_LABELS}" https://gitlab.espressif.cn:6688/api/v4/projects/630/trigger/pipeline
