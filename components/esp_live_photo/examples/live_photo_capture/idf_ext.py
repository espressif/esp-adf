import os
import sys
import shutil
import importlib.util
import tempfile
import yaml
from pathlib import Path

try:
    from idf_component_manager.dependencies import download_project_dependencies
    from idf_component_tools.manager import ManifestManager
    from idf_component_tools.utils import ProjectRequirements
except ImportError:
    download_project_dependencies = ManifestManager = ProjectRequirements = None

def _bmgr_config_callback(target_name: str, ctx, args, **kwargs) -> None:
    raise Exception('should not be called')

_FAKE_ACTIONS = {
    'actions': {
        'gen-bmgr-config': {
            'callback': _bmgr_config_callback,
            'options': [],
            'short_help': 'Generate ESP Board Manager configuration files',
        },
    }
}

def _load_module(file_path, module_name=None):
    module_name = module_name or file_path.stem
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    if spec is None or spec.loader is None:
        raise ImportError(f'Failed to create spec for {file_path}')
    mod = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = mod
    spec.loader.exec_module(mod)
    return mod

from typing import Tuple, Union, Optional, Dict

def _get_bmgr_info(project_path: str) -> Tuple[Optional[str], Optional[Dict]]:
    """Extract information about esp_board_manager dependency from manifest."""
    MANIFEST_NAMES = ['idf_component.yml', 'idf_component.yaml']
    BMGR_KEYS = ['espressif/esp_board_manager', 'esp_board_manager']
    proj = Path(project_path)
    main_dir = proj / 'main'

    manifest_path = next((p for f in MANIFEST_NAMES if (p := main_dir / f).exists()), None)
    if not manifest_path:
        return None, None

    try:
        with open(manifest_path, 'r', encoding='utf-8') as f:
            deps = (yaml.safe_load(f) or {}).get('dependencies', {})
        bmgr_key = next((k for k in BMGR_KEYS if k in deps), None)
        if bmgr_key:
            return bmgr_key, deps[bmgr_key]
    except Exception as e:
        print(f'Error reading manifest: {e}')

    return None, None

def _download_bmgr_component(project_path: str) -> None:
    """Extract the esp_board_manager dependency from the manifest file in the project's main directory and download it."""
    if not all([download_project_dependencies, ManifestManager, ProjectRequirements]):
        print('Warning: idf-component-manager is not available, cannot download component')
        return

    proj = Path(project_path)
    managed_path = proj / 'managed_components'
    lock_path = proj / 'dependencies.lock'

    bmgr_key, bmgr_dep = _get_bmgr_info(project_path)
    if not bmgr_key:
        print(f'Warning: esp_board_manager dependency not found in manifest')
        return

    managed_path.mkdir(parents=True, exist_ok=True)

    try:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_manifest = Path(temp_dir) / 'idf_component.yml'
            with open(temp_manifest, 'w', encoding='utf-8') as f:
                yaml.dump({'dependencies': {bmgr_key: bmgr_dep}}, f)

            manifest = ManifestManager(str(temp_dir), 'temp').load()
            download_project_dependencies(
                ProjectRequirements([manifest]),
                str(lock_path),
                str(managed_path),
            )
            print(f'Successfully downloaded esp_board_manager component to {managed_path}')
    except Exception as e:
        print(f'Error downloading esp_board_manager component: {e}')
        raise

def action_extensions(base_actions: dict, project_path: str) -> dict:
    bmgr_path = Path(project_path) / 'managed_components' / 'espressif__esp_board_manager'

    bmgr_key, bmgr_dep = _get_bmgr_info(project_path)
    if bmgr_dep and 'override_path' in bmgr_dep:
        bmgr_path = Path(project_path) / 'main' / bmgr_dep['override_path']
        bmgr_path = bmgr_path.resolve()

    if len(sys.argv) > 2 and sys.argv[1] == 'gen-bmgr-config':
        is_updating = os.getenv('_IDF_EXT_UPDATING_DEPS') == '1'
        shutil.rmtree(Path(project_path) / 'components' / 'gen_bmgr_codes', ignore_errors=True)
        if not bmgr_path.exists() and not is_updating:
            # Only download if it's not overridden or the overridden path doesn't exist
            if bmgr_dep and 'override_path' not in bmgr_dep:
                os.environ['_IDF_EXT_UPDATING_DEPS'] = '1'
                os.environ.setdefault('IDF_TARGET', 'esp32')
                try:
                    _download_bmgr_component(project_path)
                finally:
                    os.environ.pop('_IDF_EXT_UPDATING_DEPS', None)
            else:
                print(f'Warning: esp_board_manager path {bmgr_path} does not exist')

    if not bmgr_path.exists():
        return _FAKE_ACTIONS
    else:
        bmgr_config = _load_module(bmgr_path / 'idf_ext.py')
        return bmgr_config.action_extensions(base_actions, project_path)
