# SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

"""
This file is used in CI generate binary files for different kinds of apps
"""

import argparse
import sys
import os
import re
from pathlib import Path
from typing import List, Optional

from idf_build_apps import App, build_apps, find_apps, setup_logging, utils

PROJECT_ROOT = Path(__file__).parent.parent.absolute()
APPS_BUILD_PER_JOB = 30
IGNORE_WARNINGS = [
    r'1/2 app partitions are too small',
    r'This clock source will be affected by the DFS of the power management',
    r'The current IDF version does not support using the gptimer API',
    r'DeprecationWarning: pkg_resources is deprecated as an API',
    r'The smallest .+ partition is nearly full \(\d+% free space left\)!',
]

def compare_versions(v1, v2):
    if v1.startswith('v'):
        v1 = v1[1:]
    if v2.startswith('v'):
        v2 = v2[1:]
    v1_parts = v1.split('.')
    v2_parts = v2.split('.')

    if int(v1_parts[0]) > int(v2_parts[0]):
        return True
    elif int(v1_parts[0]) < int(v2_parts[0]):
        return False
    else:
        if int(v1_parts[1]) > int(v2_parts[1]):
            return True
        elif int(v1_parts[1]) < int(v2_parts[1]):
            return False
        else:
            return False

def _get_idf_version():
    if os.environ.get('IDF_VERSION'):
        return os.environ.get('IDF_VERSION')
    version_path = os.path.join(os.environ['IDF_PATH'], 'tools/cmake/version.cmake')
    regex = re.compile(r'^\s*set\s*\(\s*IDF_VERSION_([A-Z]{5})\s+(\d+)')
    ver = {}
    with open(version_path) as f:
        for line in f:
            m = regex.match(line)
            if m:
                ver[m.group(1)] = m.group(2)
    return 'v{}.{}'.format(int(ver['MAJOR']), int(ver['MINOR']))


def _idf_ver_to_suffix(idf_ver: str) -> str:
    """Convert IDF version string to file suffix (major.minor only).

    Examples:
        'v5.4'         -> 'idf-v5-4'
        'v5.4.3'       -> 'idf-v5-4'
        'release/v5.5' -> 'idf-v5-5'
        'release/v5.5.3' -> 'idf-v5-5'
    """
    ver = idf_ver.split('/')[-1]                   # strip 'release/' prefix if present
    parts = ver.lstrip('v').split('.')             # ['5', '4', '3']
    major_minor = 'v' + '.'.join(parts[:2])        # 'v5.4'
    return 'idf-' + major_minor.replace('.', '-')  # 'idf-v5-4'


def _get_sdkconfig_defaults_str(app_dir: str, target: str, idf_ver_suffix: str) -> Optional[str]:
    """
    Check if sdkconfig.defaults.<target>.<idf_ver_suffix> exists in app_dir.
    If so, return the full semicolon-separated defaults list:
      sdkconfig.defaults (if exists)
      sdkconfig.defaults.<target> (if exists)
      sdkconfig.defaults.<target>.<idf_ver_suffix>
    Returns None when no version-specific file exists (keep default behavior).
    """
    base = 'sdkconfig.defaults'
    target_file = f'{base}.{target}'
    version_file = f'{target_file}.{idf_ver_suffix}'

    if not os.path.isfile(os.path.join(app_dir, version_file)):
        return None

    result = []
    if os.path.isfile(os.path.join(app_dir, base)):
        result.append(base)
    if os.path.isfile(os.path.join(app_dir, target_file)):
        result.append(target_file)
    result.append(version_file)
    return ';'.join(result)


def _patch_sdkconfig_defaults(apps: List[App], idf_ver: str) -> None:
    """
    For each app, if a version-specific sdkconfig.defaults file exists in its
    app_dir, update sdkconfig_defaults_str and recompute the cached file list.
    """
    idf_ver_suffix = _idf_ver_to_suffix(idf_ver)
    for app in apps:
        defaults_str = _get_sdkconfig_defaults_str(app.app_dir, app.target, idf_ver_suffix)
        if defaults_str is None:
            continue
        app.sdkconfig_defaults_str = defaults_str
        new_files, new_defined_target = app._process_sdkconfig_files()
        app._sdkconfig_files = new_files
        app._sdkconfig_files_defined_target = new_defined_target


def get_cmake_apps(
    paths,
    target,
    config_rules_str,
    default_build_targets,
    idf_ver,
):
    if '/' in idf_ver:
        idf_ver_str = idf_ver.split('/')[1]
    else:
        idf_ver_str = idf_ver

    if compare_versions('v5.3', idf_ver_str):
        apps = find_apps(
            paths,
            recursive=True,
            target=target,
            build_dir=f'{idf_ver}/build_@t_@w',
            config_rules_str=config_rules_str,
            build_log_path='build_log.txt',
            size_json_path='size.json',
            check_warnings=False,
            preserve=True,
            default_build_targets=default_build_targets,
            manifest_files=[str(p) for p in Path(os.environ['ADF_PATH']).glob('**/.build-test-rules.yml')],
        )
    else:
        apps = find_apps(
            paths=[str(p) for p in paths],
            recursive=True,
            target=target,
            build_dir=f'{idf_ver}/build_@t_@w',
            config_rules_str=config_rules_str,
            build_log_filename='build_log.txt',
            size_json_filename='size.json',
            check_warnings=False,
            default_build_targets=default_build_targets,
            manifest_files=[str(p) for p in Path(os.environ['ADF_PATH']).glob('**/.build-test-rules.yml')],
        )

    _patch_sdkconfig_defaults(apps, idf_ver)
    return apps

def main(args):
    default_build_targets = args.default_build_targets.split(',') if args.default_build_targets else None
    idf_ver = _get_idf_version()
    apps = get_cmake_apps(args.paths, args.target, args.config, default_build_targets, idf_ver)
    if args.exclude_apps:
        apps_to_build = [app for app in apps if app.name not in args.exclude_apps]
    else:
        apps_to_build = apps[:]

    print(f'Found {len(apps_to_build)} apps after filtering')
    print(f'Suggest setting the parallel count to {len(apps_to_build) // APPS_BUILD_PER_JOB + 1} for this build job')

    ret_code = build_apps(
        apps_to_build,
        parallel_count=args.parallel_count,
        parallel_index=args.parallel_index,
        dry_run=False,
        collect_size_info=args.collect_size_info,
        keep_going=True,
        ignore_warning_strs=IGNORE_WARNINGS,
        copy_sdkconfig=True,
    )

    sys.exit(ret_code)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Build all the apps for different test types. Will auto remove those non-test apps binaries',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument('paths', nargs='*', help='Paths to the apps to build.')
    parser.add_argument(
        '-t', '--target',
        default='all',
        help='Build apps for given target. could pass "all" to get apps for all targets',
    )
    parser.add_argument(
        '--config',
        default=['sdkconfig.ci=default', 'sdkconfig.ci.*=', '=default'],
        action='append',
        help='Adds configurations (sdkconfig file names) to build. This can either be '
        'FILENAME[=NAME] or FILEPATTERN. FILENAME is the name of the sdkconfig file, '
        'relative to the project directory, to be used. Optional NAME can be specified, '
        'which can be used as a name of this configuration. FILEPATTERN is the name of '
        'the sdkconfig file, relative to the project directory, with at most one wildcard. '
        'The part captured by the wildcard is used as the name of the configuration.',
    )
    parser.add_argument(
        '--parallel-count', default=1, type=int, help='Number of parallel build jobs.'
    )
    parser.add_argument(
        '--parallel-index',
        default=1,
        type=int,
        help='Index (1-based) of the job, out of the number specified by --parallel-count.',
    )
    parser.add_argument(
        '--collect-size-info',
        type=argparse.FileType('w'),
        help='If specified, the test case name and size info json will be written to this file',
    )
    parser.add_argument(
        '--exclude-apps',
        nargs='*',
        help='Exclude build apps',
    )
    parser.add_argument(
        '--default-build-targets',
        default=None,
        help='default build targets used in manifest files',
    )
    parser.add_argument(
        '-v', '--verbose',
        action='count', default=0,
        help='Show verbose log message',
    )

    arguments = parser.parse_args()

    if not arguments.paths:
        arguments.paths = [PROJECT_ROOT]
    setup_logging(verbose=arguments.verbose)  # Info
    main(arguments)
