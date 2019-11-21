# Copyright 2019 The Pigweed Authors
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

import logging

from pw_cmd.color import Color as _Color

_LOG = logging.getLogger(__name__)

class LogDemoCommand:
    """A simple demo command to show how logs look at various levels"""
    def name(self):
        return 'logdemo'

    def help(self):
        return 'Show how logs look at different levels'

    def register(self, parser):
        pass

    def run(self):
        # Force the log level to make sure all logs are shown.
        _LOG.setLevel(logging.DEBUG)

        # Log one message for every log level.
        _LOG.critical('Something terrible has happened!')
        _LOG.error('There was an error on our last operation')
        _LOG.warning('Looks like something is amiss; consider investigating')
        _LOG.info('The operation went as expected')
        _LOG.debug('Adding 1 to i')


def install():
    """Configure the system logger for the default pw command log format."""
    # This applies a gray background to the time to make the log lines distinct
    # from other input, in a way that's easier to see than plain colored text.
    logging.basicConfig(
            format=_Color.black_on_white('%(asctime)s') +
                   ' %(levelname)s %(message)s',
            datefmt='%Y%m%d %I:%M:%S %p',
            level=logging.INFO)

    # Shorten all the log levels to 3 characters for column-aligned logs.
    # Color the logs using ANSI codes.
    # yapf: disable
    logging.addLevelName(logging.CRITICAL, _Color.bold_red('CRT'))
    logging.addLevelName(logging.ERROR,    _Color.red     ('ERR'))
    logging.addLevelName(logging.WARNING,  _Color.yellow  ('WRN'))
    logging.addLevelName(logging.INFO,     _Color.magenta ('INF'))
    logging.addLevelName(logging.DEBUG,    _Color.blue    ('DBG'))
    # yapf: enable

