#!/bin/bash

basedir=$(dirname "$0")
ADF_PATH=$(cd "${basedir}"; pwd)
export ADF_PATH

if [ -z "$IDF_PATH" ]; then
    export IDF_PATH=$ADF_PATH/esp-idf
fi

echo "ADF_PATH: $ADF_PATH"
echo "IDF_PATH: $IDF_PATH"

pushd $IDF_PATH > /dev/null
git submodule update --init --recursive --depth 1
./install.sh || return $?
popd > /dev/null
