# Copyright 2023 The Pigweed Authors
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
"""Sphinx directives for Pigweed SEEDs"""


import docutils
from docutils import nodes
import docutils.statemachine

# pylint: disable=consider-using-from-import
import docutils.parsers.rst.directives as directives  # type: ignore

# pylint: enable=consider-using-from-import
from sphinx.application import Sphinx as SphinxApplication
from sphinx.util.docutils import SphinxDirective

from sphinx_design.cards import CardDirective


def status_choice(arg) -> str:
    return directives.choice(
        arg,
        (
            'draft',
            'open_for_comments',
            'intent_approved',
            'last_call',
            'accepted',
            'rejected',
        ),
    )


def parse_status(arg) -> str:
    """Support variations on the status choices.

    For example, you can use capital letters and spaces.
    """

    return status_choice('_'.join([token.lower() for token in arg.split(' ')]))


def status_badge(seed_status: str, badge_status) -> str:
    """Given a SEED status, return the status badge for rendering."""

    return (
        ':bdg-primary:'
        if seed_status == badge_status
        else ':bdg-secondary-line:'
    )


def cl_link(cl_num):
    return (
        f'`pwrev/{cl_num} '
        '<https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/'
        f'{cl_num}>`_'
    )


class PigweedSeedDirective(SphinxDirective):
    """Directive registering & rendering SEED metadata."""

    required_arguments = 0
    final_argument_whitespace = True
    has_content = True
    option_spec = {
        'number': directives.positive_int,
        'name': directives.unchanged_required,
        'status': parse_status,
        'proposal_date': directives.unchanged_required,
        'cl': directives.positive_int_list,
        'authors': directives.unchanged_required,
        'facilitator': directives.unchanged_required,
    }

    def _try_get_option(self, option: str):
        """Try to get an option by name and raise on failure."""

        try:
            return self.options[option]
        except KeyError:
            raise self.error(f' :{option}: option is required')

    def run(self) -> list[nodes.Node]:
        seed_number = '{:04d}'.format(self._try_get_option('number'))
        seed_name = self._try_get_option('name')
        status = self._try_get_option('status')
        proposal_date = self._try_get_option('proposal_date')
        cl_nums = self._try_get_option('cl')
        authors = self._try_get_option('authors')
        facilitator = self._try_get_option('facilitator')

        title = (
            f':fas:`seedling` SEED-{seed_number}: :ref:'
            f'`{seed_name}<seed-{seed_number}>`\n'
        )

        authors_heading = 'Authors' if len(authors.split(',')) > 1 else 'Author'

        self.content = docutils.statemachine.StringList(
            [
                ':octicon:`comment-discussion` Status:',
                f'{status_badge(status, "open_for_comments")}'
                '`Open for Comments`',
                ':octicon:`chevron-right`',
                f'{status_badge(status, "intent_approved")}'
                '`Intent Approved`',
                ':octicon:`chevron-right`',
                f'{status_badge(status, "last_call")}`Last Call`',
                ':octicon:`chevron-right`',
                f'{status_badge(status, "accepted")}`Accepted`',
                ':octicon:`kebab-horizontal`',
                f'{status_badge(status, "rejected")}`Rejected`',
                '\n',
                f':octicon:`calendar` Proposal Date: {proposal_date}',
                '\n',
                ':octicon:`code-review` CL: ',
                ', '.join([cl_link(cl_num) for cl_num in cl_nums]),
                '\n',
                f':octicon:`person` {authors_heading}: {authors}',
                '\n',
                f':octicon:`person` Facilitator: {facilitator}',
            ]
        )

        card = CardDirective.create_card(
            inst=self,
            arguments=[title],
            options={},
        )

        return [card]


def setup(app: SphinxApplication):
    app.add_directive('seed', PigweedSeedDirective)

    return {
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }
