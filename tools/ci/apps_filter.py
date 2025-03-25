# Note:
#   $1 esp32
#   $2 ESP_LYRAT_V4_3
#   $3 v4.4 or release/v4.4
#
# Usage:
#   source $ADF_PATH/tools/ci/apps_filter.py $IDF_TARGET $AUDIO_BOARD $IDF_VERSION_TAG apps.yaml

import os
import sys
import yaml
import json
import argparse
from enum import Enum


class ConfigKey(Enum):
    DEFAULTS = 'DEFAULTS'
    APPS     = 'APPS'
    APP_DIR  = 'app_dir'
    BOARD    = 'board'


def is_version_in_range(version_range, target_version):
    if '-' in version_range:
        start_version, end_version = version_range.split('-')
        start_version = float(start_version[1:])
        end_version = float(end_version[1:])
        target_version = float(target_version[1:])
        return start_version <= target_version <= end_version
    else:
        return version_range == target_version


def create_apps_txt(matching_app_dirs):
    if matching_app_dirs:
        with open('apps.txt', 'a', encoding='utf-8') as file:
            for app_dir in matching_app_dirs:
                file.write(f"{app_dir}\n")
        print(f"Matching application directories have been written to apps.txt")
    else:
        print("No matching application directories found.")


def create_audo_board_idf_json_file(board, board_dict):
    """
    Checks if the specified board exists in board_dict,
    if found, writes the content of board_dict to a JSON file.
    """
    if board not in board_dict:
        print(f"\033[31mERROR: Board '{board}' not found in DEFAULTS section of the YAML file\033[0m")
    else:
        print(f"Board '{board}' found in DEFAULTS. Proceeding...")
        ADF_PATH = os.getenv('ADF_PATH')
        if ADF_PATH is None:
            raise EnvironmentError("Environment variable ADF_PATH not set")
        # Write board_dict content to JSON file
        output_json_path = os.path.join(ADF_PATH, 'tools', 'ci', 'audio_board_idf.json')
        with open(output_json_path, 'w', encoding='utf-8') as json_file:
            json.dump(board_dict, json_file, ensure_ascii=False, indent=4)

        print(f"{board} configuration saved to {output_json_path}")


def replace_variable_placeholders(value, variable_dict):
    """
    Replaces variable placeholders in a string with actual values.

    Parameters:
        value (str): The string containing variable placeholders.
        variable_dict (dict): The dictionary with variables, where the key is the variable name and the value is the actual value.

    Returns:
        str: The string after replacement of the placeholders.
    """
    if isinstance(value, str):
        for var_key, var_value in variable_dict.items():
            if isinstance(var_value, str):
                value = value.replace(f"${{{var_key}}}", var_value)
    return value


def app_main():
    parser = argparse.ArgumentParser(description='Process some configuration parameters.')
    parser.add_argument('--target',
                        type=str,
                        default='esp32s3',
                        help='IDF target (default: esp32s3)')
    parser.add_argument('--board',
                        type=str,
                        default='ESP32_S3_KORVO2_V3',
                        help='Audio board (default: ESP32_S3_KORVO2_V3)')
    parser.add_argument('--idf_ver',
                        type=str,
                        default='v5.3',
                        help='IDF version tag (default: v5.3)')
    parser.add_argument('--config_file',
                        type=str,
                        default=os.path.join(os.getenv('ADF_PATH', '.'), 'tools', 'ci', 'apps.yaml'),
                        help='Path to the configuration file (default: ./tools/ci/apps.yaml)')

    args = parser.parse_args()

    target = args.target
    audio_board = args.board
    idf_version_tag = args.idf_ver.split('/')[-1]
    config_file_path = args.config_file
    matching_app_dirs = []
    default_board_idf_dict = {}

    with open(config_file_path, 'r', encoding='utf-8') as file:
        config = yaml.safe_load(file)

    # Get the variables section and build the variable dictionary
    variable_dict = {key: value for var in config.get('variables', []) for key, value in var.items()}

    # Get the DEFAULTS section and replace variable placeholders
    for default in config.get(ConfigKey.DEFAULTS.value, []):
        if isinstance(default, dict):
            for key, value in default.items():
                default_board_idf_dict[key] = replace_variable_placeholders(value, variable_dict)

    create_audo_board_idf_json_file(audio_board, default_board_idf_dict)

    # Get the APPS section and match the application directories
    for app in config.get(ConfigKey.APPS.value, []):
        app_dir = app.get(ConfigKey.APP_DIR.value, '')
        boards = app.get(ConfigKey.BOARD.value, {})

        if target in boards:
            target_boards = boards[target]
        else:
            continue

        for target_board in target_boards:
            if isinstance(target_board, dict):
                for board, version in target_board.items():
                    if board == audio_board and isinstance(version, str) and is_version_in_range(version, idf_version_tag):
                        print(f"  Matching App Directory: {app_dir}")
                        matching_app_dirs.append(app_dir)
            else:
                if target_board == audio_board:
                    default_value = default_board_idf_dict.get(audio_board, None)
                    if isinstance(default_value, str) and is_version_in_range(default_value, idf_version_tag):
                        print(f"  Matching App Directory: {app_dir}")
                        matching_app_dirs.append(app_dir)

    create_apps_txt(matching_app_dirs)


if __name__ == "__main__":
    app_main()
