#!/usr/bin/env bash
#
# This script is used to check the sdkconfig.defaults / sdkconfig.defaults.xxx & apps.json.
# The first parameter is the name of the configuration file to check.
# The second parameter is the name of the development board to be checked

[ -z ${IDF_PATH} ] && die "IDF_PATH is not set"
[ -z ${ADF_PATH} ] && die "ADF_PATH is not set"
[ -z ${ADF_UT} ] && die "ADF_UT is not set"

if [ -d ${IDF_PATH}/tools/unit-test-app/build ]; then
    rm ${IDF_PATH}/tools/unit-test-app/build -r
fi
if [ -f ${IDF_PATH}/tools/unit-test-app/sdkconfig ]; then
    rm ${IDF_PATH}/tools/unit-test-app/sdkconfig
fi
if [ -f ${IDF_PATH}/tools/unit-test-app/sdkconfig.old ]; then
    rm ${IDF_PATH}/tools/unit-test-app/sdkconfig.old
fi
if [ -f ${IDF_PATH}/tools/unit-test-app/sdkconfig.defaults ]; then
    rm ${IDF_PATH}/tools/unit-test-app/sdkconfig.defaults
    cp ${ADF_UT}/sdkconfig_v44.defaults ${IDF_PATH}/tools/unit-test-app/sdkconfig.defaults
fi

if [ `grep -c "/esp_audio" "${ADF_PATH}/project.mk"` -eq '0' ]; then
    sed -i '/EXTRA_COMPONENT_DIRS += \$(ADF_PATH)\/components\//a\EXTRA_COMPONENT_DIRS += $(ADF_UT)/esp_audio' ${ADF_PATH}/project.mk
fi

sed -i "s%include \$(IDF_PATH)\/make\/project.mk%include \$(ADF_PATH)\/project.mk%g" ${IDF_PATH}/tools/unit-test-app/Makefile

echo "The unit test environment is set up."
echo "Please run the following commands to compile and download (Notes: If the case list is lost when the firmware is running, \
please run command \"make flash monitor -j8 TEST_COMPONENTS=\"fatfs\"\" before running the following commands):"
echo "make flash monitor -j8 TEST_COMPONENTS=\"esp_audio\""
