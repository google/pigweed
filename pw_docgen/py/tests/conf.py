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
"""Sphinx configuration for the GN docs build tests."""
project = "GN docs build tests"
author = "The Pigweed Authors"
copyright = f"2025, {author}"
release = "0.0.0"
exclude_patterns = []
extensions = [
    "breathe",
]
pygments_style = "sphinx"
html_permalinks = False
breathe_projects = {}
breathe_projects[project] = "../doxygen/xml"
breathe_default_project = project
