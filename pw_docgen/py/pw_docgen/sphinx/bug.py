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
"""Custom role for creating bug links."""

from docutils import nodes
from docutils.parsers.rst import roles

from sphinx.application import Sphinx


def bug_role(
    role,  # pylint: disable=unused-argument
    rawtext,
    text,
    lineno,
    inliner,
    options=None,
    content=None,  # pylint: disable=unused-argument
):
    options = roles.normalized_role_options(options)
    try:
        bug = int(nodes.unescape(text))
        if bug < 1:
            raise ValueError
    except ValueError:
        msg = inliner.reporter.error(
            'Bug number must be a number greater than or equal to 1; '
            '"%s" is invalid.' % text,
            line=lineno,
        )
        prb = inliner.problematic(rawtext, rawtext, msg)
        return [prb], [msg]

    ref = 'https://pwbug.dev/{:d}'.format(bug)
    node = nodes.reference(rawtext, 'b/{:d}'.format(bug), refuri=ref, **options)
    return [node], []


def setup(app: Sphinx) -> dict[str, bool]:
    app.add_role('bug', bug_role)
    return {
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }
