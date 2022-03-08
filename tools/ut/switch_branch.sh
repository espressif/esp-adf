#!/usr/bin/env bash

while read rows
do
    VAR=$rows
    if [[ $VAR =~ "BRANCH" ]]; then
        echo $VAR | grep -Eo "\<BRANCH.*:\s*\S+"
        if [ $? = "0" ];then
            VAR_BRANCH=$(echo $VAR | grep -Eo "[a-zA-Z0-9./\-\_-]*" | tail -1)
        else
            VAR_BRANCH=""
        fi
        echo "The target branch:"$VAR_BRANCH
    elif [[ $VAR =~ "PATH" ]]; then
        VAR_PATH=$(echo $VAR | grep -Eo "[a-zA-Z0-9./\-\_-]*" | tail -1)
        if [[ $VAR_PATH ]] && [[ $VAR_BRANCH ]]; then
            cd $VAR_PATH
            echo "This path has been switched to:"$(pwd)
            if [ -e tools/CreateSectionTable.pyc ]; then rm -f tools/CreateSectionTable.pyc; fi
            git fetch
            git checkout $VAR_BRANCH
            git pull origin $VAR_BRANCH
            git submodule update --init --recursive
            cd -
        fi
        echo "The target PATH:"$VAR_PATH
    fi
done < $ADF_PATH/tools/ut/ut_branch.conf