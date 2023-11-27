#!/usr/bin/env bash
set -o errexit

[ -z ${IDF_PATH} ] && die "IDF_PATH is not set"
[ -z ${ADF_PATH} ] && die "ADF_PATH is not set"

if ! type jq >/dev/null 2>&1;then
    apt-get update
    apt-get install jq -y
else
    echo "The jq tool has been installed"
fi

echo -e "\e[32m ESP_ADF_MODULES:" ${ESP_ADF_MODULES} "\e[0m"
modules_array=(`echo $ESP_ADF_MODULES | jq -r keys[]`)
if [ -n "$modules_array" ]; then
    echo "Get modules:" ${modules_array[@]}
fi

function find_path()
{
    local module_name=$1
    local res=$(find $ADF_PATH/.. -name $module_name -type d)
    module_path=""
    for var in $res
    do
        cd $var
        result=$(find . -maxdepth 1 -name .git)
        if [ -n "$result" ]; then
            module_path=$var
            echo "Module Path:" $var
            cd -
            return 0
        else
            cd -
        fi
    done;
    echo -e "\e[31m ERROR:" $module_name "not found \e[0m"
    return 0
}

for module in ${modules_array[*]}
do
    find_path $module
    module_branch=$(echo $ESP_ADF_MODULES | jq -r '.["'$module'"]')
    if [ "$module_branch" = "default" ]; then
        echo -e "\e[32m Modules: Use default branch \e[0m"
        break
    elif [ -z "$module_path" ]; then
        echo -e "\e[33m Skip\e[0m"
    else
        echo -e "\e[32m Module:" $module_path "->" $module_branch "\e[0m"
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
