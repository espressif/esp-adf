#!/usr/bin/env python
# coding=utf-8

import os
import re
import sys
import argparse
import subprocess

ADF_PATH = os.environ.get("ADF_PATH")
IDF_PATH = os.environ.get("IDF_PATH")
if ADF_PATH is None:
    ADF_PATH = os.path.realpath(os.path.join(os.path.dirname(__file__), '..'))
if IDF_PATH is None:
    IDF_PATH = os.path.realpath(os.path.join(ADF_PATH, 'esp-idf'))
    print(f"Use default IDF_PATH: {IDF_PATH}")

sys.path.append(os.path.join(IDF_PATH, "tools"))
from idf_py_actions.tools import idf_version

# Naming rules for idf patch:
# - If the patch applies to the entire IDF minor version (vx.x), it is named idf_vx.x_xxxxx.patch
# - If the patch applies to a specific IDF patch version (vx.x.x), it is named idf_vx.x.x_xxxxx.patch
IDF_PATCH_PREFIX = "idf_"
IDF_PATCHES_PATH = os.path.join(ADF_PATH, "idf_patches")


def apply_patch(patches_dir_path, patch_prefix, version):
    """
    Patch application rules: Prioritize the application of patches with a prefix close to `f"{patch_prefix}{version}"` in patches_dir_path.

    Args:
        patches_dir_path (str): Folder path to store patch files.
        patch_prefix (str): The patch file prefix to retrieve.
        version (str): The patch version to retrieve.
    """
    patch_version = ''
    match = re.match(r'(.[0-9]+\.[0-9]+)\.(\d+)', version)

    if match:
        # If the IDF version is in vx.x.x format, the patch file named with vx.x.x and closest to the current version will be searched first.
        major_minor = match.group(1)
        patch_num = int(match.group(2))
        for patch in range(patch_num, -1, -1):
            patch_files = [f for f in os.listdir(patches_dir_path) if f.startswith(f"{patch_prefix}{major_minor}.{patch}_")]
            if patch_files:
                patch_version = f"{major_minor}.{patch}"
                break

        # If the corresponding vx.x.x patch file is not found, prepare to search for a patch file named vx.x
        if patch_version == '':
            parts = version.rsplit('.', 1)
            major_minor = parts[0]
            patch_version = str(major_minor)
    else:
        # Note that the IDF version is in vx.x format.
        # Then apply the patch file in vx.x.0 format first. If not found, apply the patch file named vx.x.
        patch_files = [f for f in os.listdir(patches_dir_path) if f.startswith(f"{patch_prefix}{version}.0_")]
        if patch_files:
            patch_version = f"{version}.0"
        else:
            patch_version = str(version)

    patch_files = [f for f in os.listdir(patches_dir_path) if f.startswith(f"{patch_prefix}{patch_version}_")]
    if patch_files:
        for patch_file in patch_files:
            patch_path = os.path.join(patches_dir_path, patch_file)
            print(f"Applying patch {patch_path}")
            subprocess.run(["git", "apply", f"{patch_path}"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    else:
        print("No need to apply patches")


def main(argv):
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest='action')
    subparsers.add_parser('apply-patch', help='Apply the corresponding version of the patch in the idf_patches folder')
    args = parser.parse_args(argv)

    if args.action == "apply-patch":
        orig_dir = os.getcwd()
        os.chdir(IDF_PATH)
        idf_version_output = str(idf_version())
        idf_ver = re.search(r"v[0-9]+\.[0-9]+[.0-9]*", idf_version_output).group()
        if (idf_ver):
            apply_patch(IDF_PATCHES_PATH, IDF_PATCH_PREFIX, idf_ver)
        os.chdir(orig_dir)


if __name__ == '__main__':
    main(sys.argv[1:])
