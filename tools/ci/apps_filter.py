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


def get_adf_path():
    """
    Get ADF_PATH environment variable, raise exception if not set

    Returns:
        str: Value of ADF_PATH environment variable

    Raises:
        EnvironmentError: If ADF_PATH is not set
    """
    adf_path = os.getenv('ADF_PATH')
    if adf_path is None:
        raise EnvironmentError("Environment variable ADF_PATH not set")
    return adf_path


def parse_version(version_str):
    """
    Parse version string like 'v5.5.1' or 'v5.5' to tuple (5, 5, 1) for comparison.
    Shorter versions are padded with zeros (e.g., 'v5.5' becomes (5, 5, 0)).
    """
    version_str = version_str.lstrip('v')
    parts = version_str.split('.')
    # Pad with zeros to ensure at least 3 parts (major, minor, patch)
    while len(parts) < 3:
        parts.append('0')
    return tuple(int(part) for part in parts[:3])


def is_version_in_range(version_range, target_version):
    if '-' in version_range:
        start_version, end_version = version_range.split('-')
        start_ver_tuple = parse_version(start_version)
        end_ver_tuple = parse_version(end_version)
        target_ver_tuple = parse_version(target_version)
        return start_ver_tuple <= target_ver_tuple <= end_ver_tuple
    else:
        return version_range == target_version


def create_apps_txt(matching_app_dirs):
    if matching_app_dirs:
        with open('apps.txt', 'a', encoding='utf-8') as file:
            for app_dir in matching_app_dirs:
                file.write(f"{app_dir}\n")
        print(f"Matching application directories have been written to apps.txt", file=sys.stderr)
    else:
        print("No matching application directories found.", file=sys.stderr)


def filter_pytest_dirs_and_export(app_dirs):
    """
    Filter directories containing pytest_*.py files and export shell variables

    Args:
        app_dirs: List of application directories
    """
    import glob

    ADF_PATH = get_adf_path()

    pytest_dirs = []
    for app_dir in app_dirs:
        full_path = os.path.join(ADF_PATH, app_dir)
        # Find pytest_*.py files in the directory
        pytest_files = glob.glob(os.path.join(full_path, 'pytest_*.py'))
        if pytest_files:
            pytest_dirs.append(full_path)
            print(f"  Found pytest files in: {app_dir}", file=sys.stderr)

    if not pytest_dirs:
        print("No directories with pytest files found", file=sys.stderr)

    # Output shell export statements that can be evaluated
    if pytest_dirs:
        print(f"export TEST_DIR='{' '.join(pytest_dirs)}'")
    else:
        print("export TEST_DIR=''")


def create_audo_board_idf_json_file(board, board_dict):
    """
    Checks if the specified board exists in board_dict,
    if found, writes the content of board_dict to a JSON file.
    """
    if board not in board_dict:
        print(f"\033[31mERROR: Board '{board}' not found in DEFAULTS section of the YAML file\033[0m", file=sys.stderr)
        sys.exit(1)
    else:
        print(f"Board '{board}' found in DEFAULTS. Proceeding...", file=sys.stderr)
        ADF_PATH = get_adf_path()
        # Write board_dict content to JSON file
        output_json_path = os.path.join(ADF_PATH, 'tools', 'ci', 'audio_board_idf.json')
        with open(output_json_path, 'w', encoding='utf-8') as json_file:
            json.dump(board_dict, json_file, ensure_ascii=False, indent=4)

        print(f"{board} configuration saved to {output_json_path}", file=sys.stderr)


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
                        default=os.path.join(get_adf_path(), 'tools', 'ci', 'apps.yaml'),
                        help='Path to the configuration file (default: $ADF_PATH/tools/ci/apps.yaml)')
    parser.add_argument('--pytest-export',
                        action='store_true',
                        help='Filter pytest directories and export shell variables for eval')

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
                    # Replace variable placeholders in version string
                    version = replace_variable_placeholders(version, variable_dict)
                    if board == audio_board and isinstance(version, str) and is_version_in_range(version, idf_version_tag):
                        print(f"  Matching App Directory: {app_dir}", file=sys.stderr)
                        matching_app_dirs.append(app_dir)
            else:
                if target_board == audio_board:
                    default_value = default_board_idf_dict.get(audio_board, None)
                    if isinstance(default_value, str) and is_version_in_range(default_value, idf_version_tag):
                        print(f"  Matching App Directory: {app_dir}", file=sys.stderr)
                        matching_app_dirs.append(app_dir)

    create_apps_txt(matching_app_dirs)

    # If --pytest-export specified, filter pytest directories and export shell variables
    if args.pytest_export:
        filter_pytest_dirs_and_export(matching_app_dirs)


if __name__ == "__main__":
    app_main()
