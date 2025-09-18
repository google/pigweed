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
"""Custom role for creating CodeSearch links."""

from docutils import nodes
from docutils.parsers.rst import roles

from sphinx.application import Sphinx
from sphinx.util.nodes import split_explicit_title


def cs_role(
    role,  # pylint: disable=unused-argument
    rawtext,
    text,
    lineno,
    inliner,
    options=None,
    content=None,  # pylint: disable=unused-argument
):
    has_explicit_title, title, target = split_explicit_title(text)
    options = roles.normalized_role_options(options)
    base = 'https://cs.opensource.google/pigweed/pigweed/+/'
    url = (
        '{}{}'.format(base, target)
        if has_explicit_title
        else '{}main:{}'.format(base, title)
    )
    node = nodes.reference(rawtext, title, refuri=url, **options)
    return [node], []


def setup(app: Sphinx) -> dict[str, bool]:
    app.add_role('cs', cs_role)
    return {
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }
