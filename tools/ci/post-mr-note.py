#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import re
import sys
from pathlib import Path

import gitlab
import urllib3

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)


PREVIEW_RE = re.compile(r'^\[document preview\]\[([^\]]+)\]\s*(\S+)')


def parse_args():
    parser = argparse.ArgumentParser(
        description='Post documentation preview links to an existing GitLab MR.'
    )
    parser.add_argument('authkey', help='GitLab project or personal access token')
    parser.add_argument('project', help='GitLab project path, for example adf/esp-adf-internal')
    parser.add_argument('mr_iid', help='Merge request IID')
    parser.add_argument(
        '--url',
        default=os.environ.get('GITLAB_URL_BASE'),
        help='GitLab instance URL',
    )
    parser.add_argument(
        '--doc-url-file',
        default='logs/doc-url.txt',
        help='Path to the deploy-docs output captured by the deploy job',
    )
    return parser.parse_args()


def collect_preview_links(path):
    links = {}
    for line in Path(path).read_text(encoding='utf-8').splitlines():
        match = PREVIEW_RE.match(line.strip())
        if not match:
            continue
        lang_chip, url = match.groups()
        lang = 'zh_CN' if lang_chip.startswith('zh_CN') else 'en'
        links[lang] = url
    return links


def prepare_note(links):
    parts = []
    if 'zh_CN' in links:
        parts.append(f"[ESP-ADF CN]({links['zh_CN']})")
    if 'en' in links:
        parts.append(f"[ESP-ADF EN]({links['en']})")
    return 'Documentation preview:\n\n' + ' / '.join(parts) if parts else ''


def main():
    args = parse_args()
    if not args.url:
        print('GitLab URL is required. Set GITLAB_URL_BASE or pass --url.', file=sys.stderr)
        return 1

    links = collect_preview_links(args.doc_url_file)
    note = prepare_note(links)
    if not note:
        print(f'No documentation preview links found in {args.doc_url_file}')
        return 1

    server = gitlab.Gitlab(args.url, private_token=args.authkey, api_version=4, ssl_verify=False)
    project = server.projects.get(args.project)
    mr = project.mergerequests.get(args.mr_iid)
    mr.notes.create({'body': note})
    print(note)
    return 0


if __name__ == '__main__':
    sys.exit(main())
