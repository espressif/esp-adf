#!/usr/bin/env python
#
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

"""Filter CI app paths with .build_test_rules.yml disable rules."""

from __future__ import annotations

import argparse
import ast
import re
import sys
from pathlib import Path
from typing import Any

import yaml


def _idf_version_parts(idf_version: str) -> dict[str, int]:
    match = re.search(r'(\d+)(?:\.(\d+))?(?:\.(\d+))?', idf_version or '')
    if not match:
        return {
            'IDF_VERSION_MAJOR': 0,
            'IDF_VERSION_MINOR': 0,
            'IDF_VERSION_PATCH': 0,
        }

    major, minor, patch = match.groups()
    return {
        'IDF_VERSION_MAJOR': int(major or 0),
        'IDF_VERSION_MINOR': int(minor or 0),
        'IDF_VERSION_PATCH': int(patch or 0),
    }


def _normalise_path(path: str, project_root: Path) -> str:
    path_obj = Path(path)
    if path_obj.is_absolute():
        try:
            path_obj = path_obj.resolve().relative_to(project_root.resolve())
        except ValueError:
            pass
    return path_obj.as_posix().strip('/')


def _path_matches_rule(app_path: str, rule_path: str) -> bool:
    return app_path == rule_path or app_path.startswith(f'{rule_path}/')


def _compare(left: Any, right: Any, op: ast.cmpop) -> bool:
    if isinstance(op, ast.Eq):
        return left == right
    if isinstance(op, ast.NotEq):
        return left != right
    if isinstance(op, ast.Lt):
        return left < right
    if isinstance(op, ast.LtE):
        return left <= right
    if isinstance(op, ast.Gt):
        return left > right
    if isinstance(op, ast.GtE):
        return left >= right
    raise ValueError(f'unsupported comparison operator: {op.__class__.__name__}')


def _eval_node(node: ast.AST, variables: dict[str, Any]) -> Any:
    if isinstance(node, ast.Expression):
        return _eval_node(node.body, variables)
    if isinstance(node, ast.BoolOp):
        values = [_eval_node(value, variables) for value in node.values]
        if isinstance(node.op, ast.And):
            return all(values)
        if isinstance(node.op, ast.Or):
            return any(values)
        raise ValueError(f'unsupported boolean operator: {node.op.__class__.__name__}')
    if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.Not):
        return not _eval_node(node.operand, variables)
    if isinstance(node, ast.Compare):
        left = _eval_node(node.left, variables)
        for op, comparator in zip(node.ops, node.comparators):
            right = _eval_node(comparator, variables)
            if not _compare(left, right, op):
                return False
            left = right
        return True
    if isinstance(node, ast.Name):
        if node.id not in variables:
            raise ValueError(f'unsupported variable: {node.id}')
        return variables[node.id]
    if isinstance(node, ast.Constant):
        return node.value
    raise ValueError(f'unsupported expression node: {node.__class__.__name__}')


def _eval_if(expr: str, variables: dict[str, Any]) -> bool:
    tree = ast.parse(expr, mode='eval')
    return bool(_eval_node(tree, variables))


def _load_rules(rules_file: Path) -> dict[str, Any]:
    if not rules_file.is_file():
        return {}

    with rules_file.open('r', encoding='utf-8') as fr:
        data = yaml.safe_load(fr) or {}

    if not isinstance(data, dict):
        raise ValueError(f'{rules_file} must contain a YAML mapping')

    return {
        str(path).strip('/'): rule
        for path, rule in data.items()
        if not str(path).startswith('.') and rule
    }


def _disable_keys_for_job(job: str) -> tuple[str, ...]:
    if job == 'build':
        return ('disable', 'disable_build')
    if job == 'test':
        return ('disable', 'disable_build', 'disable_test')
    return ('disable', 'disable_build', 'disable_test')


def _is_disabled(app_path: str, rules: dict[str, Any], variables: dict[str, Any], job: str) -> bool:
    disable_keys = _disable_keys_for_job(job)
    for rule_path, rule in sorted(rules.items(), key=lambda item: len(item[0])):
        if not _path_matches_rule(app_path, rule_path):
            continue

        if isinstance(rule, dict):
            for disable_key in disable_keys:
                disable_rules = rule.get(disable_key, [])
                for item in disable_rules:
                    if isinstance(item, dict) and 'if' in item and _eval_if(str(item['if']), variables):
                        return True

    return False


def main() -> int:
    parser = argparse.ArgumentParser(description='Filter app paths using .build_test_rules.yml')
    parser.add_argument('--rules-file', required=True, type=Path)
    parser.add_argument('--project-root', required=True, type=Path)
    parser.add_argument('--target', default='')
    parser.add_argument('--idf-version', default='')
    parser.add_argument('--config-name', default='default')
    parser.add_argument('--include-default', default='1')
    parser.add_argument('--job', choices=('build', 'test', 'all'), default='all')
    args = parser.parse_args()

    rules = _load_rules(args.rules_file)
    if not rules:
        sys.stdout.write(sys.stdin.read())
        return 0

    variables: dict[str, Any] = {
        'IDF_TARGET': args.target,
        'CONFIG_NAME': args.config_name,
        'INCLUDE_DEFAULT': args.include_default not in ('', '0', 'false', 'False'),
    }
    variables.update(_idf_version_parts(args.idf_version))

    for raw_path in sys.stdin:
        path = raw_path.strip()
        if not path:
            continue
        app_path = _normalise_path(path, args.project_root)
        if not _is_disabled(app_path, rules, variables, args.job):
            print(path)

    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception as e:
        print(f'[ERROR] failed to apply .build_test_rules.yml: {e}', file=sys.stderr)
        sys.exit(2)
