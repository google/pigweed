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
"""Docs widget that provides up-to-date info about the next Pigweed Live."""


import datetime
import sys

from docutils import nodes
from docutils.parsers.rst import Directive
from sphinx.application import Sphinx

try:
    import pytz  # type: ignore

    PYTZ_AVAILABLE = True
except ImportError:
    PYTZ_AVAILABLE = False


class PigweedLiveDirective(Directive):
    """Generates the up-to-date Pigweed Live info."""

    datetime_format = '%Y-%m-%d %H:%M:%S'
    # TODO: b/303859828 - Update this data sometime between 2024-09-23
    # and 2024-10-07.
    meetings = [
        '2024-07-01 13:00:00',
        '2024-07-29 13:00:00',
        '2024-08-26 13:00:00',
        '2024-09-23 13:00:00',
        '2024-10-21 13:00:00',
        '2024-12-16 13:00:00',
    ]
    timezone = pytz.timezone('US/Pacific')

    def run(self) -> list[nodes.Node]:
        return [self._make_paragraph()]

    def _make_paragraph(self) -> nodes.Node:
        next_meeting = self._find_next_meeting()
        paragraph = nodes.paragraph()
        paragraph += nodes.Text('Our next Pigweed Live is ')
        meeting_text = nodes.strong()
        meeting_text += nodes.Text(next_meeting)
        paragraph += meeting_text
        paragraph += nodes.Text(
            (
                ". Please join us to discuss what's new in Pigweed and "
                "anything else Pigweed-related that's on your mind. "
            )
        )
        refuri = 'https://groups.google.com/g/pigweed'
        link = nodes.reference(refuri=refuri)
        link += nodes.Text('Join our mailing list')
        paragraph += link
        paragraph += nodes.Text(" to receive an invite to the next meeting.")
        return paragraph

    def _find_next_meeting(self) -> str:
        current_datetime = self.timezone.localize(datetime.datetime.now())
        next_meeting = None
        for meeting in self.meetings:
            unlocalized_datetime = datetime.datetime.strptime(
                meeting, self.datetime_format
            )
            meeting_datetime = self.timezone.localize(unlocalized_datetime)
            if current_datetime > meeting_datetime:
                continue
            next_meeting = meeting_datetime
            break
        if next_meeting is None:
            sys.exit(
                'ERROR: Pigweed Live meeting data needs to be updated. '
                'Update the `meetings` list in `PigweedLiveDirective`. '
                'See b/303859828.'
            )
        else:
            date = next_meeting.strftime('%a %b %d, %Y')
            hour = next_meeting.strftime('%I%p').lstrip('0')
            timezone = 'PDT' if next_meeting.dst() else 'PST'
            return f'{date} {hour} ({timezone})'


def setup(app: Sphinx) -> dict[str, bool]:
    """Initialize the directive."""
    if PYTZ_AVAILABLE:
        app.add_directive('pigweed-live', PigweedLiveDirective)
    return {
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }
