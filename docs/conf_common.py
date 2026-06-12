import re

from esp_docs.conf_docs import *  # noqa: F403,F401

extensions += [
    'sphinx_copybutton',
    # Render Mermaid diagrams client-side (no mmdc required).
    'sphinxcontrib.mermaid',
    'esp_docs.esp_extensions.dummy_build_system',
    # Package the built HTML into a downloadable .zip so the
    # sphinx_idf_theme version selector can offer "Download HTML".
    'esp_docs.esp_extensions.add_html_zip',
    'esp_docs.esp_extensions.run_doxygen',
]

languages = ['en', 'zh_CN']

# link roles config
project_homepage = 'https://github.com/espressif/esp-adf'
github_repo = 'espressif/esp-adf'

# context used by sphinx_idf_theme
html_context['github_user'] = 'espressif'
html_context['github_repo'] = 'esp-adf'

html_static_path = ['../_static']

# Extra options required by sphinx_idf_theme
project_slug = 'esp-adf'

# Contains info used for constructing target and version selector
versions_url = './_static/docs_version.js'

# Enable API hover tooltips on :c:/:cpp: cross-reference links.
esp_hover_api_enable = True

# Mermaid: render diagrams client-side via the Mermaid JS library bundled by
# sphinxcontrib-mermaid. This avoids any dependency on the 'mmdc' CLI tool,
# which is not available in the CI environment. The 'raw' format keeps the
# directive content as-is in HTML so the browser-side mermaid.js can render it.
mermaid_output_format = 'raw'
mermaid_height = 'auto'

# Redirect old URLs after the docs restructure (see page_redirects.txt).
with open('../page_redirects.txt') as f:
    lines = [re.sub(' +', ' ', line.strip()) for line in f.readlines()
             if line.strip() and not line.startswith('#')]
    for line in lines:
        if len(line.split(' ')) != 2:
            raise RuntimeError('Invalid line in page_redirects.txt: %s' % line)
    html_redirect_pages = [tuple(line.split(' ')) for line in lines]

# http://stackoverflow.com/questions/12772927/specifying-an-online-image-in-sphinx-restructuredtext-format
suppress_warnings = ['image.nonlocal_uri']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
exclude_patterns = ['_build', 'README.md']

# The suffix of source filenames.
source_suffix = ['.rst', '.md']

# The master toctree document.
master_doc = 'index'

linkcheck_exclude_documents = ['index']

from esp_docs.conf_docs import setup as esp_docs_setup
from esp_docs.esp_extensions.link_roles import github_link, get_github_rev, get_submodules

def setup(app):
    esp_docs_setup(app)
    app.add_css_file('mermaid_overrides.css')
    rev = get_github_rev()
    submods = get_submodules()
    # Correctly point the example role to adf_examples using standard link checks
    app.add_role('example', github_link('tree', rev, submods, '/adf_examples/', app.config), override=True)
    app.add_role('example_file', github_link('blob', rev, submods, '/adf_examples/', app.config), override=True)
    app.add_role('example_raw', github_link('raw', rev, submods, '/adf_examples/', app.config), override=True)
