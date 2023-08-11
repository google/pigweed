#!/usr/bin/env python3
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
"""device module unit tests"""

from contextlib import contextmanager
import logging
import queue
import threading
import time
import unittest

from pw_hdlc.rpc import RpcClient, HdlcRpcClient


class QueueFile:
    """A fake file object backed by a queue for testing."""

    EOF = object()

    def __init__(self):
        # Operator puts; consumer gets
        self._q = queue.Queue()

        # Consumer side access only!
        self._readbuf = b''
        self._eof = False

    ###############
    # Consumer side

    def __enter__(self):
        return self

    def __exit__(self, *exc_info):
        self.close()

    def _read_from_buf(self, size: int) -> bytes:
        data = self._readbuf[:size]
        self._readbuf = self._readbuf[size:]
        return data

    def read(self, size: int = 1) -> bytes:
        """Reads data from the queue"""
        # First try to get buffered data
        data = self._read_from_buf(size)
        assert len(data) <= size
        size -= len(data)

        # if size == 0:
        if data:
            return data

        # No more data in the buffer
        assert not self._readbuf

        if self._eof:
            return data  # may be empty

        # Not enough in the buffer; block on the queue
        item = self._q.get()

        # NOTE: We can't call Queue.task_done() here because the reader hasn't
        # actually *acted* on the read item yet.

        # Queued data
        if isinstance(item, bytes):
            self._readbuf = item
            return self._read_from_buf(size)

        # Queued exception
        if isinstance(item, Exception):
            raise item

        # Report EOF
        if item is self.EOF:
            self._eof = True
            return data  # may be empty

        raise Exception('unexpected item type')

    def write(self, data: bytes) -> None:
        pass

    #####################
    # Weird middle ground

    # It is a violation of most file-like object APIs for one thread to call
    # close() while another thread is calling read(). The behavior is
    # undefined.
    #
    # - On Linux, close() may wake up a select(), leaving the caller with a bad
    #   file descriptor (which could get reused!)
    # - Or the read() could continue to block indefinitely.
    #
    # We choose to cause a subsequent/parallel read to receive an exception.
    def close(self) -> None:
        self.cause_read_exc(Exception('closed'))

    ###############
    # Operator side

    def put_read_data(self, data: bytes) -> None:
        self._q.put(data)

    def cause_read_exc(self, exc: Exception) -> None:
        self._q.put(exc)

    def set_read_eof(self) -> None:
        self._q.put(self.EOF)

    def wait_for_drain(self, timeout=None) -> None:
        """Wait for the queue to drain (be fully consumed).

        Args:
          timeout: The maximum time (in seconds) to wait, or wait forever
            if None.

        Raises:
          TimeoutError: If timeout is given and has elapsed.
        """
        # It would be great to use Queue.join() here, but that requires the
        # consumer to call Queue.task_done(), and we can't do that because
        # the consumer of read() doesn't know anything about it.
        # Instead, we poll.  ¯\_(ツ)_/¯
        start_time = time.time()
        while not self._q.empty():
            if timeout is not None:
                elapsed = time.time() - start_time
                if elapsed > timeout:
                    raise TimeoutError(f"Queue not empty after {elapsed} sec")
            time.sleep(0.1)


class QueueFileTest(unittest.TestCase):
    """Test the QueueFile class"""

    def test_read_data(self) -> None:
        file = QueueFile()
        file.put_read_data(b'hello')
        self.assertEqual(file.read(5), b'hello')

    def test_read_data_multi_read(self) -> None:
        file = QueueFile()
        file.put_read_data(b'helloworld')
        self.assertEqual(file.read(5), b'hello')
        self.assertEqual(file.read(5), b'world')

    def test_read_data_multi_put(self) -> None:
        file = QueueFile()
        file.put_read_data(b'hello')
        file.put_read_data(b'world')
        self.assertEqual(file.read(5), b'hello')
        self.assertEqual(file.read(5), b'world')

    def test_read_eof(self) -> None:
        file = QueueFile()
        file.set_read_eof()
        result = file.read(5)
        self.assertEqual(result, b'')

    def test_read_exception(self) -> None:
        file = QueueFile()
        message = 'test exception'
        file.cause_read_exc(ValueError(message))
        with self.assertRaisesRegex(ValueError, message):
            file.read(5)

    def test_wait_for_drain_works(self) -> None:
        file = QueueFile()
        file.put_read_data(b'hello')
        file.read()
        try:
            # Timeout is arbitrary; will return immediately.
            file.wait_for_drain(0.1)
        except TimeoutError:
            self.fail("wait_for_drain raised TimeoutError")

    def test_wait_for_drain_raises(self) -> None:
        file = QueueFile()
        file.put_read_data(b'hello')
        # don't read
        with self.assertRaises(TimeoutError):
            # Timeout is arbitrary; it will raise no matter what.
            file.wait_for_drain(0.1)


class Sentinel:
    def __repr__(self):
        return 'Sentinel'


def _get_client(file) -> RpcClient:
    return HdlcRpcClient(
        read=file.read,
        paths_or_modules=[],
        channels=[],
    )


def _wait_for_reader_thread_exit(client: RpcClient) -> None:
    # TODO(b/294858483): Joining the thread should be done by RpcClient itself.
    thread = client._reader._reader_thread  # pylint: disable=protected-access

    # This should take <10ms but we'll wait up to 1000x longer.
    thread.join(10.0)


# This should take <10ms but we'll wait up to 1000x longer.
_QUEUE_DRAIN_TIMEOUT = 10.0


class HdlcRpcClientTest(unittest.TestCase):
    """Tests the pw_hdlc.rpc.HdlcRpcClient class."""

    # NOTE: There is no test here for stream EOF because Serial.read()
    # can return an empty result if configured with timeout != None.
    # The reader thread will continue in this case.

    def test_clean_close_after_stream_close(self) -> None:
        """Assert RpcClient closes cleanly when stream closes."""
        # See b/293595266.
        file = QueueFile()

        with self.assert_no_hdlc_rpc_error_logs():
            with file:
                with _get_client(file) as rpc_client:
                    # We want to make sure the reader thread is blocked on
                    # read() and doesn't exit immediately.
                    file.put_read_data(b'')
                    file.wait_for_drain(_QUEUE_DRAIN_TIMEOUT)

                # RpcClient.__exit__ calls stop() on the reader thread, but
                # it is blocked on file.read().

            # QueueFile.close() is called, triggering an exception in the
            # blocking read() (by implementation choice). The reader should
            # handle it by *not* logging it and exiting immediately.
            _wait_for_reader_thread_exit(rpc_client)

        self.assert_no_background_threads_running()

    def test_device_handles_read_exception(self) -> None:
        """Assert RpcClient closes cleanly when read raises an exception."""
        # See b/293595266.
        file = QueueFile()

        logger = logging.getLogger('pw_hdlc.rpc')
        test_exc = Exception('boom')
        with self.assertLogs(logger, level=logging.ERROR) as ctx:
            with _get_client(file) as rpc_client:
                # Cause read() to raise an exception. The reader should
                # handle it by logging it and exiting immediately.
                file.cause_read_exc(test_exc)
                file.wait_for_drain(_QUEUE_DRAIN_TIMEOUT)
                _wait_for_reader_thread_exit(rpc_client)

        # Assert one exception was raised
        self.assertEqual(len(ctx.records), 1)
        rec = ctx.records[0]
        self.assertIsNotNone(rec.exc_info)
        assert rec.exc_info is not None  # for mypy
        self.assertEqual(rec.exc_info[1], test_exc)

        self.assert_no_background_threads_running()

    @contextmanager
    def assert_no_hdlc_rpc_error_logs(self):
        logger = logging.getLogger('pw_hdlc.rpc')
        sentinel = Sentinel()
        with self.assertLogs(logger, level=logging.ERROR) as ctx:
            # TODO(b/294861320) use assertNoLogs() in Python 3.10+
            # We actually want to assert there are no errors, but
            # TestCase.assertNoLogs() is not available until Python 3.10.
            # So we log one error to keep the test from failing and manually
            # inspect the list of captured records.
            logger.error(sentinel)

            yield ctx

        self.assertEqual([record.msg for record in ctx.records], [sentinel])

    def assert_no_background_threads_running(self):
        self.assertEqual(threading.enumerate(), [threading.current_thread()])


if __name__ == '__main__':
    unittest.main()
