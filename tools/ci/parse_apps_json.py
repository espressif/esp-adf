#!/usr/bin/env python
#-- coding:UTF-8 --
# Copyright 2022 Espressif Systems (Shanghai) PTE LTD
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Script function
# Select all routines supported by the current development board under the current IDF version from list.json
import re
import os
import argparse

IDF_SUPPORT_CHIP = {'722043f734':['v33'],
                    'release/v3.3':['v33'],
                    'release/v4.1':['v41'],
                    'release/v4.2':['v42'],
                    'release/v4.3':['v43'],
                    'v4.3.4':['v43'],
                    'release/v4.4':['v44']}

ADF_PATH = os.getenv('ADF_PATH')
if not ADF_PATH:
    print('Please set ADF_PATH before running this script')
    raise SystemExit(-1)

BUILD_PATH = os.getenv('BUILD_PATH')
if not BUILD_PATH:
    print('Please set BUILD_PATH before running this script')
    raise SystemExit(-1)

APPS_JSON_PATH = ADF_PATH + '/tools/ci/apps_v4_4.json'
LIST_JSON_PATH = BUILD_PATH + '/list.json'
LIST_JSON_BK_PATH = BUILD_PATH + '/list_backup.json'

# Filter examples
# Remove examples that are not supported by this development version under the current version.
def filter_examples_in_list_json(reserve_example):
    if os.path.exists(LIST_JSON_PATH):
        os.rename(LIST_JSON_PATH, LIST_JSON_BK_PATH)
        list_json = open(LIST_JSON_PATH, 'a')
        list_backup = open(LIST_JSON_BK_PATH, 'r', encoding='utf8', errors='ignore').readlines()

        for i in range(len(reserve_example)):
            single_example_str = '\{\"build_system\"\: \"cmake\"\, \"app_dir\"\: \"'+str(reserve_example[i])+'.*?\}'
            single_example_json = re.findall(single_example_str, str(list_backup))
            print(single_example_json)
            list_json.write(str(single_example_json[0])+'\n')

# Parse apps_v4_4.json
# Get all the examples supported by the current development board under the idf_branch version from 'apps.josn'.
def parse_apps_json(idf_branch, board):
    apps_json = open(APPS_JSON_PATH, 'r', encoding='utf8', errors='ignore').readlines()
    list_json = open(LIST_JSON_PATH, 'r', encoding='utf8', errors='ignore').readlines()
    list_examples = re.findall('"app_dir": "(.*?)", "work_dir"',str(list_json))
    print(list_examples)
    support_examples = list()
    for example in list_examples:
        single_example = re.findall('/examples/(.*)',str(example))
        board_type = re.findall(str(single_example[0]) +'(.*?)}',str(apps_json))
        idf_version = re.findall(board + ':\[(.*?)];',str(board_type))
        if not idf_version:
            print("need rm:" + example)
        if idf_branch in str(idf_version):
            support_examples.append(example)
        elif '-' in str(idf_version):
            idf_version_range = re.findall('v[0-9][0-9]',str(idf_version))
            if idf_version_range is not None:
                if idf_branch > idf_version_range[0] and idf_branch < idf_version_range[1]:
                    support_examples.append(example)
    print(support_examples)
    return support_examples

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--board", "-B",
                    help="Set audio hal",
                    default='ESP_LYRAT_V4_3')

    parser.add_argument("--branch", "-b",
                    help="Set branch",
                    default='release/v4.4')

    args = parser.parse_args()

    examples_list = list()
    examples_list = parse_apps_json(str(IDF_SUPPORT_CHIP[args.branch][0]), args.board)
    filter_examples_in_list_json(examples_list)
