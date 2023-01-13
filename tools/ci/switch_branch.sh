#!/usr/bin/env bash

[ -z ${IDF_PATH} ] && die "IDF_PATH is not set"
[ -z ${ADF_PATH} ] && die "ADF_PATH is not set"

set -o errexit

if ! type jq >/dev/null 2>&1;then
    apt-get update
    apt-get install jq -y
else
    echo "The jq tool has been installed"
fi

echo -e "\e[32m ESP_ADF_MODULES: " ${ESP_ADF_MODULES} " \e[0m"
modules_array=(`echo $ESP_ADF_MODULES | jq -r keys[]`)
if [ -n "$modules_array" ]; then
    echo "Get modules: " ${modules_array[@]}
fi

function find_path()
{
    res=$(find $ADF_PATH/.. -name $1 -type d)
    for var in $res
    do
        cd $var
        result=$(find . -maxdepth 1 -name .git)
        if [ -n "$result" ]; then
            module_path=$var
            echo "Module Path: " $var
            cd -
            return 0
        else
            cd -
        fi
    done;
    echo -e "\e[31m ERROR: Module not found \e[0m"
    exit 1
    return 0
}

for module in ${modules_array[*]}
do
    find_path $module
    module_branch=$(echo $ESP_ADF_MODULES | jq -r '.["'$module'"]')
    echo -e "\e[32m Module: " $module_path " -> " $module_branch "\e[0m"
    if [ "$module_branch" = "default" ]; then
        echo -e "\e[32m Modules: Use default branch \e[0m"
        break
    else
        cd $module_path
        echo "This path has been switched to:" $module_path
        if [ -e tools/CreateSectionTable.pyc ]; then rm -f tools/CreateSectionTable.pyc; fi
        git fetch
        git checkout $module_branch
        git pull origin $module_branch
        git submodule update --init --recursive --depth 1
        git log -1
        cd -
    fi
done
