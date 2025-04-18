# Copyright 2025 The Pigweed Authors
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
"""Tests for ensuring that pw_docgen correctly builds Sphinx sites."""

import argparse
import os
import pathlib
import sys
import unittest

import bs4

is_gn_build = False
try:
    from python.runfiles import runfiles  # type: ignore
except ImportError:
    is_gn_build = True

expected_files = [
    ".buildinfo",
    "_sources/index.rst.txt",
    "_sources/pw_docgen/py/tests/lib.rst.txt",
    "_static/alabaster.css",
    "_static/basic.css",
    "_static/custom.css",
    "_static/doctools.js",
    "_static/documentation_options.js",
    "_static/file.png",
    "_static/language_data.js",
    "_static/minus.png",
    "_static/plus.png",
    "_static/pygments.css",
    "_static/searchtools.js",
    "_static/sphinx_highlight.js",
    "genindex.html",
    "objects.inv",
    "pw_docgen/py/tests/lib.html",
    "search.html",
    "searchindex.js",
    "index.html",
]


class TestGnDocsBuild(unittest.TestCase):
    """Test that GN built the Sphinx site correctly."""

    docs_dir: pathlib.Path

    def test_file_count(self):
        """Test that the GN docs produced the expected number of files."""
        actual = 0
        for root, _, files in os.walk(self.docs_dir):
            actual += len(files)
        expected = len(expected_files)
        self.assertEqual(actual, expected)

    def test_file_paths(self):
        """Test that the GN docs build produced the expected files paths."""
        print(self.docs_dir)
        for file in expected_files:
            self.assertTrue(os.path.exists(f"{self.docs_dir}/{file}"))

    def test_index_title(self):
        """Test that the title in index.html matches the title in index.rst."""
        with open(f"{self.docs_dir}/index.html", "r") as f:
            html = f.read()
        soup = bs4.BeautifulSoup(html, "html.parser")
        actual = soup.find("h1").text
        expected = "GN docs build tests"
        self.assertEqual(actual, expected)

    def test_doxygen_integration(self):
        """Test that Doxygen-generated content is inserted into the site correctly."""
        with open(f"{self.docs_dir}/pw_docgen/py/tests/lib.html", "r") as f:
            html = f.read()
        sentinel = "x91lfm4lg78"
        self.assertTrue(sentinel in html)


def main() -> None:
    """Parse CLI args and then run the unit tests."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--docs-dir",
        type=pathlib.Path,
        required=True,
        help="Path to the generated docs website.",
    )
    parser.add_argument(
        "argv",
        nargs=argparse.REMAINDER,
        help='Arguments after "--" are passed to unittest.',
    )
    args = parser.parse_args()
    TestGnDocsBuild.docs_dir = args.docs_dir
    argv = sys.argv[:1] + args.argv[1:]
    unittest.main(argv=argv)


if __name__ == "__main__":
    if is_gn_build:
        main()
