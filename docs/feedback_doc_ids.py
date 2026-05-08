# -*- coding: utf-8 -*-
"""Resolve per-page feedback ``doc_id`` from ``.lbcf.yml``; otherwise defaults to esp-adf's default doc IDs (5146 / 5147)."""

from __future__ import annotations

import re
from pathlib import Path
from typing import Any, Dict, List, Mapping, MutableMapping, Optional, Tuple
from urllib.parse import urlparse

# Repository root (parent of ``docs/``)
_DOCS_DIR = Path(__file__).resolve().parent
_REPO_ROOT = _DOCS_DIR.parent
_LBCF_PATH = _REPO_ROOT / '.lbcf.yml'

_DEFAULT_BY_LANG = {'en': '5146', 'zh_CN': '5147'}
_IDF_TARGET_SEGMENTS = {
    'esp32',
    'esp32c2',
    'esp32c3',
    'esp32c5',
    'esp32c6',
    'esp32h2',
    'esp32h4',
    'esp32p4',
    'esp32s2',
    'esp32s3',
    'esp32s31',
}

_feedback_maps_cache: Optional[Dict[str, Dict[str, str]]] = None


def _document_link_to_pagename(link: str) -> Optional[str]:
    """Convert published docs URL to Sphinx pagename."""
    path = urlparse(link).path
    m = re.search(r'/(?:latest|stable|release-v[^/]+)/(.+)$', path)
    if m is None:
        return None

    tail = m.group(1).strip('/')
    if not tail:
        return None
    if tail.endswith('.html'):
        tail = tail[:-5]
    parts = [segment for segment in tail.split('/') if segment]
    if not parts:
        return None

    # Keep full path for ADF links. For IDF-style links, drop chip target prefix.
    if parts[0] in _IDF_TARGET_SEGMENTS and len(parts) > 1:
        parts = parts[1:]
    return '/'.join(parts)


def _load_lbcf_maps() -> Dict[str, Dict[str, str]]:
    maps: Dict[str, MutableMapping[str, str]] = {'en': {}, 'zh_CN': {}}
    try:
        import yaml  # noqa: WPS433 (runtime import: optional until requirements installed)
    except ImportError:
        return {k: dict(v) for k, v in maps.items()}

    if not _LBCF_PATH.is_file():
        return {k: dict(v) for k, v in maps.items()}

    with open(_LBCF_PATH, encoding='utf-8') as fh:
        data = yaml.safe_load(fh)
    for doc in data.get('documents') or []:
        link = doc.get('document_link')
        if not link:
            continue
        lang = doc.get('language', 'en')
        sphinx_lang = 'zh_CN' if lang == 'cn' else 'en'
        pagename = _document_link_to_pagename(link)
        if not pagename:
            continue
        doc_id = str(doc['doc_id'])
        maps[sphinx_lang][pagename] = doc_id

    return {k: dict(v) for k, v in maps.items()}


def get_feedback_doc_id_maps() -> Dict[str, Dict[str, str]]:
    global _feedback_maps_cache
    if _feedback_maps_cache is None:
        _feedback_maps_cache = _load_lbcf_maps()
    return _feedback_maps_cache


def _version_tuple(version_suffix: str) -> Tuple[int, ...]:
    """Turn ``1.2`` / ``1_2`` into a tuple for ordering (best-effort)."""
    cleaned = version_suffix.replace('_', '.').strip('.')
    if not cleaned:
        return (0,)
    parts: List[int] = []
    for segment in cleaned.split('.'):
        if segment.isdigit():
            parts.append(int(segment))
        else:
            digits = ''.join(c for c in segment if c.isdigit())
            parts.append(int(digits) if digits else 0)
    return tuple(parts)


def _doc_id_for_board_prefix(
    table: Mapping[str, str], prefix: str
) -> Optional[Tuple[str, str]]:
    """If *prefix* is a board folder, return doc_id from its main LBCF page(s)."""
    for suffix, tag in (
        ('index', 'ancestor_index'),
        ('user_guide', 'ancestor_user_guide'),
    ):
        key = f'{prefix}/{suffix}'
        if key in table:
            return table[key], tag

    pfx = prefix + '/'
    versioned: List[Tuple[Tuple[int, ...], str, str]] = []
    for key, did in table.items():
        if not key.startswith(pfx):
            continue
        tail = key[len(pfx) :]
        if tail.startswith('user_guide_v') and len(tail) > len('user_guide_v'):
            ver = tail[len('user_guide_v') :]
            if re.fullmatch(r'[\d.]+', ver):
                versioned.append((_version_tuple(ver), key, did))
        elif tail.startswith('user_guide-v') and len(tail) > len('user_guide-v'):
            ver = tail[len('user_guide-v') :]
            if re.fullmatch(r'[\d.]+', ver):
                versioned.append((_version_tuple(ver), key, did))
    if versioned:
        versioned.sort(key=lambda x: x[0])
        return versioned[-1][2], 'ancestor_user_guide_versioned'
    return None


def normalize_sphinx_language(language: Optional[str]) -> str:
    """Map ``app.config.language`` (and variants) to map keys ``en`` / ``zh_CN``."""
    if not language:
        return 'en'
    lang = language.replace('-', '_')
    if lang.lower() in ('zh_cn', 'zh_tw'):
        return 'zh_CN'
    if lang == 'zh_CN':
        return 'zh_CN'
    return 'en'


def resolve_feedback_doc_id_detailed(
    maps: Mapping[str, Mapping[str, str]],
    language: str,
    pagename: str,
) -> Tuple[str, str]:
    """Return ``(doc_id, source)`` where *source* describes how *doc_id* was chosen."""
    lang = normalize_sphinx_language(language)
    table = maps.get(lang) or {}
    doc_id = table.get(pagename)
    if doc_id is not None:
        return doc_id, 'direct'
    # ADF migrated some board docs from get-started/* to design-guide/dev-boards/*.
    m = re.fullmatch(r'design-guide/dev-boards/(get-started-[^/]+|user-guide-[^/]+)', pagename)
    if m:
        legacy = f'get-started/{m.group(1)}'
        doc_id = table.get(legacy)
        if doc_id is not None:
            return doc_id, 'fallback_design_guide_to_get_started'
    # LBCF often lists either index.html or user_guide.html; Sphinx pagenames differ.
    if pagename.endswith('/index'):
        parent = pagename[: -len('/index')]
        alt = f'{parent}/user_guide'
        doc_id = table.get(alt)
        if doc_id is not None:
            return doc_id, 'fallback_index_to_user_guide'
    elif pagename.endswith('/user_guide'):
        parent = pagename[: -len('/user_guide')]
        alt = f'{parent}/index'
        doc_id = table.get(alt)
        if doc_id is not None:
            return doc_id, 'fallback_user_guide_to_index'
    # LBCF uses user_guide.html but the repo has user_guide_v1.0.rst only.
    vm = re.fullmatch(r'(.+)/user_guide(_v[\d.]+|-v[\d.]+)$', pagename)
    if vm:
        plain = f'{vm.group(1)}/user_guide'
        doc_id = table.get(plain)
        if doc_id is not None:
            return doc_id, 'fallback_versioned_to_plain_user_guide'
    # Sub-pages (reference/, kit add-ons) and board index when LBCF only lists user_guide_v*.
    cur = pagename
    while '/' in cur:
        cur = cur.rsplit('/', 1)[0]
        got = _doc_id_for_board_prefix(table, cur)
        if got is not None:
            doc_id, tag = got
            return doc_id, f'fallback_board_ancestor:{tag}'
    return _DEFAULT_BY_LANG[lang], 'default'


def resolve_feedback_doc_id(
    maps: Mapping[str, Mapping[str, str]],
    language: str,
    pagename: str,
) -> str:
    """Return CDP ``doc_id`` for feedback URL for this page."""
    doc_id, _ = resolve_feedback_doc_id_detailed(maps, language, pagename)
    return doc_id


def load_lbcf_document_rows() -> List[dict]:
    """Each dict: document_name, doc_id, lbcf_language, sphinx_lang, pagename, document_link."""
    rows: List[dict] = []
    try:
        import yaml  # noqa: WPS433
    except ImportError:
        return rows
    if not _LBCF_PATH.is_file():
        return rows
    with open(_LBCF_PATH, encoding='utf-8') as fh:
        data = yaml.safe_load(fh)
    for doc in data.get('documents') or []:
        link = doc.get('document_link')
        if not link:
            continue
        lang = doc.get('language', 'en')
        sphinx_lang = 'zh_CN' if lang == 'cn' else 'en'
        pagename = _document_link_to_pagename(link)
        if not pagename:
            continue
        rows.append(
            {
                'document_name': doc.get('document_name', ''),
                'doc_id': str(doc['doc_id']),
                'lbcf_language': lang,
                'sphinx_lang': sphinx_lang,
                'pagename': pagename,
                'document_link': link,
            }
        )
    return rows


def clear_feedback_doc_id_cache() -> None:
    """Test helper: force reload of ``.lbcf.yml`` on next build."""
    global _feedback_maps_cache
    _feedback_maps_cache = None


_registered_app_ids: set[int] = set()


def setup(app: Any) -> dict[str, bool]:
    def on_config_inited(app: Any, _config: Any) -> None:
        aid = id(app)
        if aid in _registered_app_ids:
            return
        _registered_app_ids.add(aid)

        feedback_maps = get_feedback_doc_id_maps()

        def _html_page_feedback_doc_id(
            app: Any,
            pagename: str,
            _templatename: str,
            context: dict[str, Any],
            _doctree: Any,
        ) -> None:
            lang = app.config.language or 'en'
            context['feedback_docid'] = resolve_feedback_doc_id(
                feedback_maps, lang, pagename
            )

        app.connect('html-page-context', _html_page_feedback_doc_id)

    app.connect('config-inited', on_config_inited)

    return {'parallel_read_safe': True, 'parallel_write_safe': True}
