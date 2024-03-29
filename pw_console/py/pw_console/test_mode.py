# Copyright 2022 The Pigweed Authors
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
"""pw_console test mode functions."""

import asyncio
import time
import re
import random
import logging
from threading import Thread

FAKE_DEVICE_LOGGER_NAME = 'pw_console_fake_device'

_ROOT_LOG = logging.getLogger('')
_FAKE_DEVICE_LOG = logging.getLogger(FAKE_DEVICE_LOGGER_NAME)


def start_fake_logger(lines, log_thread_entry, log_thread_loop):
    fake_log_messages = prepare_fake_logs(lines)

    test_log_thread = Thread(target=log_thread_entry, args=(), daemon=True)
    test_log_thread.start()

    background_log_task = asyncio.run_coroutine_threadsafe(
        # This function will be executed in a separate thread.
        log_forever(fake_log_messages),
        # Using this asyncio event loop.
        log_thread_loop,
    )  # type: ignore
    return background_log_task


def prepare_fake_logs(lines) -> list[tuple[str, dict]]:
    fake_logs: list[tuple[str, dict]] = []
    key_regex = re.compile(r':kbd:`(?P<key>[^`]+)`')
    for line in lines:
        if not line:
            continue

        keyboard_key = ''
        search = key_regex.search(line)
        if search:
            keyboard_key = search.group(1)

        fake_logs.append((line.lstrip(), {'keys': keyboard_key}))
    return fake_logs


async def log_forever(fake_log_messages: list[tuple[str, dict]]):
    """Test mode async log generator coroutine that runs forever."""
    _ROOT_LOG.info('Fake log device connected.')
    start_time = time.time()
    message_count = 0

    log_methods = [
        _FAKE_DEVICE_LOG.info,
        _FAKE_DEVICE_LOG.debug,
        _FAKE_DEVICE_LOG.warning,
        _FAKE_DEVICE_LOG.error,
        _FAKE_DEVICE_LOG.critical,
    ]
    log_method_rand_weights = [50, 20, 10, 10, 10]

    # Fake module column names.
    module_names = ['APP', 'RADIO', 'BAT', 'USB', 'CPU']
    while True:
        if message_count > 32:
            await asyncio.sleep(1)
        fake_log = random.choice(fake_log_messages)
        log_func = random.choices(log_methods, weights=log_method_rand_weights)

        module_name = module_names[message_count % len(module_names)]
        log_func[0](
            fake_log[0],
            extra=dict(
                extra_metadata_fields=dict(
                    module=module_name,
                    file='fake_app.cc',
                    timestamp=time.time() - start_time,
                    **fake_log[1],
                )
            ),
        )
        message_count += 1
        if message_count % 10 == 0:
            _ROOT_LOG.info('Device message count: %d', message_count)
