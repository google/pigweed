# Copyright 2020 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Pigweed's Sphinx configuration."""

import sphinx_rtd_theme

# The suffix of source filenames.
source_suffix = ['.rst', '.md']

# The master toctree document.
master_doc = 'index'

# General information about the project.
project = 'Pigweed'
copyright = '2020 The Pigweed Authors'  # pylint: disable=redefined-builtin

# The version info for the project you're documenting, acts as replacement for
# |version| and |release|, also used in various other places throughout the
# built documents.
#
# The short X.Y version.
version = '0.1'
# The full version, including alpha/beta/rc tags.
release = '0.1.0'

# The name of the Pygments (syntax highlighting) style to use.
pygm = 'sphinx'

extensions = [
    'sphinx.ext.autodoc',  # Automatic documentation for Python code
    'sphinx.ext.napoleon',  # Parses Google-style docstrings
    'm2r',  # Converts Markdown to reStructuredText

    # Blockdiag suite of diagram generators.
    'sphinxcontrib.blockdiag',
    'sphinxcontrib.nwdiag',
    'sphinxcontrib.seqdiag',
    'sphinxcontrib.actdiag',
    'sphinxcontrib.rackdiag',
    'sphinxcontrib.packetdiag',
]

_DIAG_HTML_IMAGE_FORMAT = 'SVG'
blockdiag_html_image_format = _DIAG_HTML_IMAGE_FORMAT
nwdiag_html_image_format = _DIAG_HTML_IMAGE_FORMAT
seqdiag_html_image_format = _DIAG_HTML_IMAGE_FORMAT
actdiag_html_image_format = _DIAG_HTML_IMAGE_FORMAT
rackdiag_html_image_format = _DIAG_HTML_IMAGE_FORMAT
packetdiag_html_image_format = _DIAG_HTML_IMAGE_FORMAT

# Tell m2r to parse links to .md files and add them to the build.
m2r_parse_relative_links = True

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
html_theme = 'sphinx_rtd_theme'

# Add any paths that contain custom themes here, relative to this directory.
html_theme_path = [
    '_themes',
]

html_theme_path = [sphinx_rtd_theme.get_html_theme_path()]

# The name for this set of Sphinx documents.  If None, it defaults to
# "<project> v<release> documentation".
html_title = 'Pigweed'

# If true, SmartyPants will be used to convert quotes and dashes to
# typographically correct entities.
html_use_smartypants = True

# If false, no module index is generated.
html_domain_indices = True

# If false, no index is generated.
html_use_index = True

# If true, the index is split into individual pages for each letter.
html_split_index = False

# If true, links to the reST sources are added to the pages.
html_show_sourcelink = False

# If true, "Created using Sphinx" is shown in the HTML footer. Default is True.
html_show_sphinx = False

# Output file base name for HTML help builder.
htmlhelp_basename = 'Pigweeddoc'

# One entry per manual page. List of tuples
# (source start file, name, description, authors, manual section).
man_pages = [('index', 'pigweed', 'Pigweed', ['Google'], 1)]

# Grouping the document tree into Texinfo files. List of tuples
# (source start file, target name, title, author,
#  dir menu entry, description, category)
texinfo_documents = [
    ('index', 'Pigweed', 'Pigweed', 'Google', 'Pigweed', 'Firmware framework',
     'Miscellaneous'),
]

# Markdown files imported using m2r aren't marked as "referenced," so exclude
# them from the error reference checking.
exclude_patterns = ['README.md']
