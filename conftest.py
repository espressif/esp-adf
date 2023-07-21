# SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import logging
import os
import pathlib
import re
import sys
from datetime import datetime
from typing import Callable, List, Optional, Tuple

import pytest
from _pytest.nodes import Item
from pytest import Config, FixtureRequest, Function, Session
from pytest_embedded.plugin import multi_dut_argument, multi_dut_fixture

IDF_VERSION = os.environ.get('IDF_VERSION')
PYTEST_ROOT_DIR = str(pathlib.Path(__file__).parent)
logging.info(f'Pytest root dir: {PYTEST_ROOT_DIR}')


SUPPORTED_TARGETS = ['esp32', 'esp32s2', 'esp32c3', 'esp32s3', 'esp32c2', 'esp32c6', 'esp32h2']
PREVIEW_TARGETS: List[str] = []  # this PREVIEW_TARGETS excludes 'linux' target
DEFAULT_SDKCONFIG = 'default'

TARGET_MARKERS = {
    'esp32': 'support esp32 target',
    'esp32s2': 'support esp32s2 target',
    'esp32s3': 'support esp32s3 target',
    'esp32c3': 'support esp32c3 target',
    'esp32c2': 'support esp32c2 target',
    'esp32c6': 'support esp32c6 target',
    'esp32h2': 'support esp32h2 target',
    'linux': 'support linux target',
}

SPECIAL_MARKERS = {
    'supported_targets': "support all officially announced supported targets ('esp32', 'esp32s2', 'esp32c3', 'esp32s3', 'esp32c2', 'esp32c6')",
    'preview_targets': "support all preview targets ('none')",
    'all_targets': 'support all targets, including supported ones and preview ones',
    'temp_skip_ci': 'temp skip tests for specified targets only in ci',
    'temp_skip': 'temp skip tests for specified targets both in ci and locally',
    'nightly_run': 'tests should be executed as part of the nightly trigger pipeline',
    'host_test': 'tests which should not be built at the build stage, and instead built in host_test stage',
    'qemu': 'build and test using qemu-system-xtensa, not real target',
}

ENV_MARKERS = {
    'ADF_EXAMPLE_GENERIC': 'Tests should be run on ADF_EXAMPLE_GENERIC runners.',
}

##################
# Help Functions #
##################
def format_case_id(target: Optional[str], config: Optional[str], case: str) -> str:
    return f'{target}.{config}.{case}'


def item_marker_names(item: Item) -> List[str]:
    return [marker.name for marker in item.iter_markers()]


def item_skip_targets(item: Item) -> List[str]:
    def _get_temp_markers_disabled_targets(marker_name: str) -> List[str]:
        temp_marker = item.get_closest_marker(marker_name)

        if not temp_marker:
            return []

        # temp markers should always use keyword arguments `targets` and `reason`
        if not temp_marker.kwargs.get('targets') or not temp_marker.kwargs.get('reason'):
            raise ValueError(
                f'`{marker_name}` should always use keyword arguments `targets` and `reason`. '
                f'For example: '
                f'`@pytest.mark.{marker_name}(targets=["esp32"], reason="IDF-xxxx, will fix it ASAP")`'
            )

        return to_list(temp_marker.kwargs['targets'])  # type: ignore

    temp_skip_ci_targets = _get_temp_markers_disabled_targets('temp_skip_ci')
    temp_skip_targets = _get_temp_markers_disabled_targets('temp_skip')

    # in CI we skip the union of `temp_skip` and `temp_skip_ci`
    if os.getenv('CI_JOB_ID'):
        skip_targets = list(set(temp_skip_ci_targets).union(set(temp_skip_targets)))
    else:  # we use `temp_skip` locally
        skip_targets = temp_skip_targets

    return skip_targets


@pytest.fixture(scope='session', autouse=True)
def idf_version() -> str:
    if os.environ.get('IDF_VERSION'):
        return os.environ.get('IDF_VERSION')
    idf_path = os.environ.get('IDF_PATH')
    if not idf_path:
        logging.warning('Failed to get IDF_VERSION!')
        return ''
    version_path = os.path.join(os.environ['IDF_PATH'], 'tools/cmake/version.cmake')
    regex = re.compile(r'^\s*set\s*\(\s*IDF_VERSION_([A-Z]{5})\s+(\d+)')
    ver = {}
    with open(version_path) as f:
        for line in f:
            m = regex.match(line)
            if m:
                ver[m.group(1)] = m.group(2)
    return '{}.{}'.format(int(ver['MAJOR']), int(ver['MINOR']))


@pytest.fixture(scope='session', autouse=True)
def session_tempdir() -> str:
    _tmpdir = os.path.join(
        os.path.dirname(__file__),
        'pytest_log',
        datetime.now().strftime('%Y-%m-%d_%H-%M-%S'),
    )
    os.makedirs(_tmpdir, exist_ok=True)
    return _tmpdir


@pytest.fixture
@multi_dut_argument
def config(request: FixtureRequest) -> str:
    config_marker = list(request.node.iter_markers(name='config'))
    return config_marker[0].args[0] if config_marker else 'default'


@pytest.fixture
@multi_dut_argument
def app_path(request: FixtureRequest, test_file_path: str) -> str:
    config_marker = list(request.node.iter_markers(name='app_path'))
    if config_marker:
        return config_marker[0].args[0]
    else:
        # compatible with old pytest-embedded parametrize --app_path
        return request.config.getoption('app_path', None) or os.path.dirname(test_file_path)


@pytest.fixture
def test_case_name(request: FixtureRequest, target: str, config: str) -> str:
    if not isinstance(target, str):
        target = '|'.join(sorted(list(set(target))))
    if not isinstance(config, str):
        config = '|'.join(sorted(list(config)))
    return f'{target}.{config}.{request.node.originalname}'


@pytest.fixture
@multi_dut_fixture
def build_dir(
    app_path: str,
    target: Optional[str],
    config: Optional[str],
    idf_version: str
) -> Optional[str]:
    """
    Check local build dir with the following priority:

    1. <app_path>/${IDF_VERSION}/build_<target>_<config>
    2. <app_path>/${IDF_VERSION}/build_<target>
    3. <app_path>/build_<target>_<config>
    4. <app_path>/build
    5. <app_path>

    Args:
        app_path: app path
        target: target
        config: config

    Returns:
        valid build directory
    """

    assert target
    assert config
    check_dirs = []
    if idf_version:
        check_dirs.append(os.path.join(idf_version, f'build_{target}_{config}'))
        check_dirs.append(os.path.join(idf_version, f'build_{target}'))
    check_dirs.append(f'build_{target}_{config}')
    check_dirs.append('build')
    check_dirs.append('.')
    for check_dir in check_dirs:
        binary_path = os.path.join(app_path, check_dir)
        if os.path.isdir(binary_path):
            logging.info(f'find valid binary path: {binary_path}')
            return check_dir

        logging.warning(
            f'checking binary path: {binary_path} ... missing ... try another place')

    logging.error(
        f'no build dir. Please build the binary "python tools/build_apps.py {app_path}" and run pytest again')
    sys.exit(1)


@pytest.fixture(autouse=True)
@multi_dut_fixture
def junit_properties(
    test_case_name: str, record_xml_attribute: Callable[[str, object], None]
) -> None:
    """
    This fixture is autoused and will modify the junit report test case name to <target>.<config>.<case_name>
    """
    record_xml_attribute('name', test_case_name)


##################
# Hook functions #
##################
_idf_pytest_embedded_key = pytest.StashKey['IdfPytestEmbedded']


def pytest_addoption(parser: pytest.Parser) -> None:
    base_group = parser.getgroup('idf')
    base_group.addoption(
        '--env',
        help='only run tests matching the environment NAME.',
    )


def pytest_configure(config: Config) -> None:
    # Require cli option "--target"
    help_commands = ['--help', '--fixtures', '--markers', '--version']
    for cmd in help_commands:
        if cmd in config.invocation_params.args:
            target = 'unneeded'
            break
        else:
            target = config.getoption('target')
    if not target:
        raise ValueError('Please specify one target marker via "--target [TARGET]"')

    config.stash[_idf_pytest_embedded_key] = IdfPytestEmbedded(
        target=target,
        env_name=config.getoption('env'),
    )
    config.pluginmanager.register(config.stash[_idf_pytest_embedded_key])

    for name, description in {**TARGET_MARKERS, **ENV_MARKERS, **SPECIAL_MARKERS}.items():
        config.addinivalue_line('markers', f'{name}: {description}')

def pytest_unconfigure(config: Config) -> None:
    _pytest_embedded = config.stash.get(_idf_pytest_embedded_key, None)
    if _pytest_embedded:
        del config.stash[_idf_pytest_embedded_key]
        config.pluginmanager.unregister(_pytest_embedded)


class IdfPytestEmbedded:
    def __init__(
        self,
        target: Optional[str] = None,
        env_name: Optional[str] = None,
    ):
        # CLI options to filter the test cases
        self.target = target
        self.env_name = env_name

        self._failed_cases: List[
            Tuple[str, bool, bool]
        ] = []  # (test_case_name, is_known_failure_cases, is_xfail)

    @pytest.hookimpl(tryfirst=True)
    def pytest_sessionstart(self, session: Session) -> None:
        if self.target:
            self.target = self.target.lower()
            session.config.option.target = self.target

    @pytest.hookimpl(tryfirst=True)
    def pytest_collection_modifyitems(self, items: List[Function]) -> None:
        # sort by file path and callspec.config
        # implement like this since this is a limitation of pytest, couldn't get fixture values while collecting
        # https://github.com/pytest-dev/pytest/discussions/9689
        # after sort the test apps, the test may use the app cache to reduce the flash times.
        def _get_param_config(_item: Function) -> str:
            if hasattr(_item, 'callspec'):
                return _item.callspec.params.get('config', DEFAULT_SDKCONFIG)  # type: ignore
            return DEFAULT_SDKCONFIG

        items.sort(key=lambda x: (os.path.dirname(x.path), _get_param_config(x)))

        # set default timeout 10 minutes for each case
        for item in items:
            if 'timeout' not in item.keywords:
                item.add_marker(pytest.mark.timeout(10 * 60))

        # add markers for special markers
        for item in items:
            if 'supported_targets' in item.keywords:
                for _target in SUPPORTED_TARGETS:
                    item.add_marker(_target)
            if 'preview_targets' in item.keywords:
                for _target in PREVIEW_TARGETS:
                    item.add_marker(_target)
            if 'all_targets' in item.keywords:
                for _target in [*SUPPORTED_TARGETS, *PREVIEW_TARGETS]:
                    item.add_marker(_target)

            # add 'xtal_40mhz' tag as a default tag for esp32c2 target
            # only add this marker for esp32c2 cases
            if (
                self.target == 'esp32c2'
                and 'esp32c2' in item_marker_names(item)
                and 'xtal_26mhz' not in item_marker_names(item)
            ):
                item.add_marker('xtal_40mhz')

        # filter all the test cases with "nightly_run" marker
        if os.getenv('INCLUDE_NIGHTLY_RUN') == '1':
            # Do not filter nightly_run cases
            pass
        elif os.getenv('NIGHTLY_RUN') == '1':
            items[:] = [item for item in items if 'nightly_run' in item_marker_names(item)]
        else:
            items[:] = [item for item in items if 'nightly_run' not in item_marker_names(item)]

        # filter all the test cases with target and skip_targets
        items[:] = [
            item
            for item in items
            if self.target in item_marker_names(item) and self.target not in item_skip_targets(item)
        ]
