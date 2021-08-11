# Copyright 2021 The Pigweed Authors
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
"""Python logging helper fuctions."""

import logging
import tempfile
from datetime import datetime
from typing import Iterator


def all_loggers() -> Iterator[logging.Logger]:
    """Iterates over all loggers known to Python logging."""
    manager = logging.getLogger().manager  # type: ignore[attr-defined]

    for logger_name in manager.loggerDict:  # pylint: disable=no-member
        yield logging.getLogger(logger_name)


def create_temp_log_file():
    """Create a unique tempfile for saving logs.

    Example format: /tmp/pw_console_2021-05-04_151807_8hem6iyq
    """

    # Grab the current system timestamp as a string.
    isotime = datetime.now().isoformat(sep='_', timespec='seconds')
    # Timestamp string should not have colons in it.
    isotime = isotime.replace(':', '')

    log_file_name = None
    with tempfile.NamedTemporaryFile(prefix=f'{__package__}_{isotime}_',
                                     delete=False) as log_file:
        log_file_name = log_file.name

    return log_file_name
