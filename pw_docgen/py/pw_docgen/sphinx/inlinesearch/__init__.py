# Copyright 2024 The Pigweed Authors
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
"""Build index to be used in the sidebar search input"""

import os
from os.path import dirname, join, exists
import json
import sphinx.search
from sphinx.util.osutil import copyfile
from sphinx.jinja2glue import SphinxFileSystemLoader


class IndexBuilder(sphinx.search.IndexBuilder):
    def freeze(self):
        """Create a usable data structure for serializing."""
        data = super().freeze()
        try:
            # Sphinx >= 1.5 format
            # Due to changes from github.com/sphinx-doc/sphinx/pull/2454
            base_file_names = data["docnames"]
        except KeyError:
            # Sphinx < 1.5 format
            base_file_names = data["filenames"]

        store = {
            "docnames": base_file_names,
            "titles": data["titles"],
        }
        index_dest = join(self.env.app.outdir, "sidebarindex.js")
        f = open(index_dest, "w")
        f.write("var SidebarSearchIndex=" + json.dumps(store))
        f.close()
        return data


def builder_inited(app):
    # adding a new loader to the template system puts our searchbox.html
    # template in front of the others, it overrides whatever searchbox.html
    # the current theme is using.
    # it's still up to the theme to actually _use_ a file called searchbox.html
    # somewhere in its layout. but the base theme and pretty much everything
    # else that inherits from it uses this filename.
    app.builder.templates.loaders.insert(
        0, SphinxFileSystemLoader(dirname(__file__))
    )


def copy_static_files(app, _):
    # because we're using the extension system instead of the theme system,
    # it's our responsibility to copy over static files outselves.
    files = ["js/searchbox.js", "css/searchbox.css"]
    for f in files:
        src = join(dirname(__file__), f)
        dest = join(app.outdir, "_static", f)
        if not exists(dirname(dest)):
            os.makedirs(dirname(dest))
        copyfile(src, dest)


def setup(app):
    # adds <script> and <link> to each of the generated pages to load these
    # files.
    app.add_css_file("css/searchbox.css")
    app.add_js_file("js/searchbox.js")

    app.connect("builder-inited", builder_inited)
    app.connect("build-finished", copy_static_files)

    sphinx.search.IndexBuilder = IndexBuilder
    return {
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
