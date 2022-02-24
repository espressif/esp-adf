# based on http://protips.readthedocs.io/link-roles.html

from __future__ import print_function
from __future__ import unicode_literals
import re
import os
import subprocess
from collections import namedtuple

from docutils import nodes
from local_util import run_cmd_get_output
from sphinx.util import logging

# Creates a dict of all submodules with the format {submodule_path : (url relative to git root), commit)}
def get_submodules():
    git_root = subprocess.check_output(['git', 'rev-parse', '--show-toplevel']).strip().decode('utf-8')
    gitmodules_file = os.path.join(git_root, '.gitmodules')

    submodules = subprocess.check_output(['git', 'submodule', 'status'], cwd=git_root).strip().decode('utf-8').split('\n')

    if submodules[0] == '':
        return {}

    submodule_dict = {}
    Submodule = namedtuple('Submodule', 'url rev')

    for sub in submodules:
        sub_info = sub.lstrip().split(' ')

        # Get short hash, 7 digits
        rev = sub_info[0].lstrip('-')[0:7]
        path = sub_info[1].lstrip('./')

        config_key_arg = 'submodule.{}.url'.format(path)
        rel_url = subprocess.check_output(['git', 'config', '--file', gitmodules_file, '--get', config_key_arg]).decode('utf-8').lstrip('./').rstrip('\n')

        submodule_dict[path] = Submodule(rel_url, rev)

    return submodule_dict


def get_github_rev():
    path = run_cmd_get_output('git rev-parse --short HEAD')
    tag = run_cmd_get_output('git describe --exact-match')
    print('Git commit ID: ', path)
    if len(tag):
        print('Git tag: ', tag)
        path = tag
    return path


def setup(app):
    rev = get_github_rev()
    submods = get_submodules()

    # links to files or folders on the GitHub
    app.add_role('adf', github_link('tree', rev, submods, '/', app.config))
    app.add_role('adf_file', github_link('blob', rev, submods, '/', app.config))
    app.add_role('adf_raw', github_link('raw', rev, submods, '/', app.config))
    app.add_role('component', github_link('tree', rev, submods, '/components/', app.config))
    app.add_role('component_file', github_link('blob', rev, submods, '/components/', app.config))
    app.add_role('component_raw', github_link('raw', rev, submods, '/components/', app.config))
    app.add_role('example', github_link('tree', rev, submods, '/examples/', app.config))
    app.add_role('example_file', github_link('blob', rev, submods, '/examples/', app.config))
    app.add_role('example_raw', github_link('raw', rev, submods, '/examples/', app.config))

    # link to the current documentation file in specific language version
    on_rtd = os.environ.get('READTHEDOCS', None) == 'True'
    if on_rtd:
        # provide RTD specific commit identification to be included in the link
        tag_rev = 'latest'
        if (run_cmd_get_output('git rev-parse --short HEAD') != rev):
            tag_rev = rev
    else:
        # if not on the RTD then provide generic identification
        tag_rev = run_cmd_get_output('git describe --always')

    app.add_role('link_to_translation', crosslink('%s../../%s/{}/%s.html'.format(tag_rev)))


def url_join(*url_parts):
    """ Make a URL out of multiple components, assume first part is the https:// part and
    anything else is a path component """
    result = "/".join(url_parts)
    result = re.sub(r"([^:])//+", r"\1/", result)  # remove any // that isn't in the https:// part
    return result


def github_link(link_type, adf_rev, submods, root_path, app_config):
    def role(name, rawtext, text, lineno, inliner, options={}, content=[]):
        logger = logging.getLogger(__name__)
        ADF_REPO = "espressif/esp-adf"
        BASE_URL = "https://github.com"

        def warning(msg):
            logger.warning(msg, location=(inliner.document.settings.env.docname, lineno))

        # Redirects to submodule repo if path is a submodule, else default to IDF repo
        def redirect_submodule(path, submods, rev):
            for key, value in submods.items():
                # Add path separator to end of submodule path to ensure we are matching a directory
                if path.lstrip('/').startswith(os.path.join(key, '')):
                    # Remove domain
                    url = value.url.split('.com')[-1]
                    return url, value.rev, re.sub('^/{}/'.format(key), '', path)

            return ADF_REPO, rev, path

        m = re.search('(.*)\s*<(.*)>', text)  # noqa: W605 - regular expression
        if m:
            link_text = m.group(1)
            link = m.group(2)
        else:
            link_text = text
            link = text

        rel_path = root_path + link
        abs_path = os.path.join(os.getenv('ADF_PATH', ''), rel_path.lstrip('/'))

        repo_url, repo_rev, rel_path = redirect_submodule(rel_path, submods, adf_rev)
        url = url_join(BASE_URL, repo_url, link_type, repo_rev, rel_path)

        if 'READTHEDOCS' not in os.environ:
            # only check links when building locally
            is_dir = (link_type == 'tree')

            if not os.path.exists(abs_path):
                warning('ADF path %s does not appear to exist (absolute path %s)' % (rel_path, abs_path))
            elif is_dir and not os.path.isdir(abs_path):
                # note these "wrong type" warnings are not strictly needed  as GitHub will apply a redirect,
                # but the may become important in the future (plus make for cleaner links)
                warning('ADF path %s is not a directory but role :%s: is for linking to a directory, try :%s_file:' % (rel_path, name, name))
            elif not is_dir and os.path.isdir(abs_path):
                warning('ADF path %s is a directory but role :%s: is for linking to a file' % (rel_path, name))

        node = nodes.reference(rawtext, link_text, refuri=url, **options)
        return [node], []
    return role


def crosslink(pattern):
    def role(name, rawtext, text, lineno, inliner, options={}, content=[]):
        (language, link_text) = text.split(':')
        docname = inliner.document.settings.env.docname
        doc_path = inliner.document.settings.env.doc2path(docname, None, None)
        return_path = '../' * doc_path.count('/')
        url = pattern % (return_path, language, docname)
        node = nodes.reference(rawtext, link_text, refuri=url, **options)
        return [node], []
    return role
