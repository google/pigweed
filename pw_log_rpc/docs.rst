.. _module-pw_log_rpc:

==========
pw_log_rpc
==========
An RPC-based logging solution for Pigweed with log filtering and log drops
reporting -- coming soon!

.. warning::
  This module is under construction and might change in the future.

How to Use
==========
1. Set up RPC
-------------
Set up RPC for your target device. Basic deployments run RPC over a UART, with
HDLC on top for framing. See :ref:`module-pw_rpc` for details on how to enable
``pw_rpc``.

2. Set up tokenized logging (optional)
--------------------------------------
Set up the :ref:`module-pw_log_tokenized` log backend.

3. Connect the tokenized logging handler to the MultiSink
---------------------------------------------------------
Create a :ref:`MultiSink <module-pw_multisink>` instance to buffer log entries.
Then, make the log backend handler,
``pw_tokenizer_HandleEncodedMessageWithPayload``, encode log entries in the
``log::LogEntry`` format, and add them to the ``MultiSink``.

4. Create log drains and filters
--------------------------------
Create an ``RpcLogDrainMap`` with one ``RpcLogDrain`` for each RPC channel used
to stream logs. Optionally, create a ``FilterMap`` with ``Filter`` objects with
different IDs. Provide these map to the ``LogService`` and register the latter
with the application's RPC service. The ``RpcLogDrainMap`` provides a convenient
way to access and maintain each ``RpcLogDrain``. Attach each ``RpcLogDrain`` to
the ``MultiSink``. Optionally, set the ``RpcLogDrain`` callback to decide if a
log should be kept or dropped. This callback can be ``Filter::ShouldDropLog``.

5. Flush the log drains in the background
-----------------------------------------
Depending on the product's requirements, create a thread to flush all
``RpcLogDrain``\s or one thread per drain. The thread(s) must continuously call
``RpcLogDrain::Flush()`` to pull entries from the ``MultiSink`` and send them to
the log listeners.

Logging over RPC diagrams
=========================

Sample RPC logs request
-----------------------
The log listener, e.g. a computer, requests logs via RPC. The log service
receives the request and sets up the corresponding ``RpcLogDrain`` to start the
log stream.

.. mermaid::

  graph TD
    computer[Computer]-->pw_rpc;
    pw_rpc-->log_service[LogService];
    log_service-->rpc_log_drain_pc[RpcLogDrain<br>streams to<br>computer];;

Sample logging over RPC
------------------------
Logs are streamed via RPC to a computer, and to another log listener. There can
also be internal log readers, i.e. ``MultiSink::Drain``\s, attached to the
``MultiSink``, such as a writer to persistent memory, for example.

.. mermaid::

  graph TD
    source1[Source 1]-->log_api[pw_log API];
    source2[Source 2]-->log_api;
    log_api-->log_backend[Log backend];
    log_backend-->multisink[MultiSink];
    multisink-->drain[MultiSink::Drain];
    multisink-->rpc_log_drain_pc[RpcLogDrain<br>streams to<br>computer];
    multisink-->rpc_log_drain_other[RpcLogDrain<br>streams to<br>other log listener];
    drain-->other_consumer[Other log consumer<br>e.g. persistent memory];
    rpc_log_drain_pc-->pw_rpc;
    rpc_log_drain_other-->pw_rpc;
    pw_rpc-->computer[Computer];
    pw_rpc-->other_listener[Other log<br>listener];

Components Overview
===================
RPC log service
---------------
The ``LogService`` class is an RPC service that provides a way to request a log
stream sent via RPC and configure log filters. Thus, it helps avoid using a
different protocol for logs and RPCs over the same interface(s).
It requires a ``RpcLogDrainMap`` to assign stream writers and delegate the
log stream flushing to the user's preferred method, as well as a ``FilterMap``
to retrieve and modify filters.

RpcLogDrain
-----------
An ``RpcLogDrain`` reads from the ``MultiSink`` instance that buffers logs, then
packs, and sends the retrieved log entries to the log listener. One
``RpcLogDrain`` is needed for each log listener. An ``RpcLogDrain`` needs a
thread to continuously call ``Flush()`` to maintain the log stream. A thread can
maintain multiple log streams, but it must not be the same thread used by the
RPC server, to avoid blocking it.

Each ``RpcLogDrain`` is identified by a known RPC channel ID and requires a
``rpc::RawServerWriter`` to write the packed multiple log entries. This writer
is assigned by the ``LogService::Listen`` RPC.

``RpcLogDrain``\s can also be provided an open RPC writer, to constantly stream
logs without the need to request them. This is useful in cases where the
connection to the client is dropped silently because the log stream can continue
when reconnected without the client requesting logs again if the error handling
is set to ``kIgnoreWriterErrors`` otherwise the writer will be closed.

An ``RpcLogDrain`` must be attached to a ``MultiSink`` containing multiple
``log::LogEntry``\s. When ``Flush`` is called, the drain acquires the
``rpc::RawServerWriter`` 's write buffer, grabs one ``log::LogEntry`` from the
multisink, encodes it into a ``log::LogEntries`` stream, and repeats the process
until the write buffer is full. Then the drain calls
``rpc::RawServerWriter::Write`` to flush the write buffer and repeats the
process until all the entries in the ``MultiSink`` are read or an error is
found.

The user must provide a buffer large enough for the largest entry in the
``MultiSink`` while also accounting for the interface's Maximum Transmission
Unit (MTU). If the ``RpcLogDrain`` finds a drop message count as it reads the
``MultiSink`` it will insert a message in the stream with the drop message
count in the log proto dropped optional field. The receiving end can display the
count with the logs if desired.

RpcLogDrainMap
--------------
Provides a convenient way to access all or a single ``RpcLogDrain`` by its RPC
channel ID.

RpcLogDrainThread
-----------------
The module includes a sample thread that flushes each drain sequentially. Future
work might replace this with enqueueing the flush work on a work queue. The user
can also choose to have different threads flushing individual ``RpcLogDrain``\s
with different priorities.

Calling ``OpenUnrequestedLogStream()`` is a convenient way to set up a log
stream that is started without the need to receive an RCP request for logs.

Filter::Rule
------------
Contains a set of values that are compared against a log when set. All
conditions must be met for the rule to be met.

- ``action``: drops or keeps the log if the other conditions match.
  The rule is ignored when inactive.

- ``any_flags_set``: the condition is met if this value is 0 or the log has any
  of these flags set.

- ``level_greater_than_or_equal``: the condition is met when the log level is
  greater than or equal to this value.

- ``module_equals``: the condition is met if this byte array is empty, or the
  log module equals the contents of this byte array.

Filter
------
``Filter`` encapsulates a collection of zero or more ``Filter::Rule``\s and has
an ID used to modify or retrieve its contents.

FilterMap
---------
Provides a convenient way to retrieve register filters by ID.

Logging example
===============
The following code shows a sample setup to defer the log handling to the
``RpcLogDrainThread`` to avoid having the log streaming block at the log
callsite.

main.cc
-------
.. code-block:: cpp

  #include "foo/foo_log.h"
  #include "pw_log/log.h"
  #include "pw_thread/detached_thread.h"
  #include "pw_thread_stl/options.h"

  namespace {

  void RegisterServices() {
    pw::rpc::system_server::Server().RegisterService(foo_log::log_service);
  }
  }  // namespace

  int main() {
    PW_LOG_INFO("Deferred logging over RPC example");
    pw::rpc::system_server::Init();
    RegisterServices();
    pw::thread::DetachedThread(pw::thread::stl::Options(), foo_log::log_thread);
    pw::rpc::system_server::Start();
    return 0;
  }

foo_log.cc
----------
Example of a log backend implementation, where logs enter the ``MultiSink`` and
log drains are set up.

.. code-block:: cpp

  #include "foo/foo_log.h"

  #include <array>
  #include <cstdint>

  #include "pw_chrono/system_clock.h"
  #include "pw_log/proto_utils.h"
  #include "pw_log_rpc/log_service.h"
  #include "pw_log_rpc/rpc_log_drain.h"
  #include "pw_log_rpc/rpc_log_drain_map.h"
  #include "pw_log_rpc/rpc_log_drain_thread.h"
  #include "pw_rpc_system_server/rpc_server.h"
  #include "pw_sync/interrupt_spin_lock.h"
  #include "pw_sync/lock_annotations.h"
  #include "pw_sync/mutex.h"
  #include "pw_tokenizer/tokenize_to_global_handler_with_payload.h"

  namespace foo_log {
  namespace {
  constexpr size_t kLogBufferSize = 5000;
  // Tokenized logs are typically 12-24 bytes.
  constexpr size_t kMaxMessageSize = 32;
  // kMaxLogEntrySize should be less than the MTU of the RPC channel output used
  // by the provided server writer.
  constexpr size_t kMaxLogEntrySize =
      pw::log_rpc::RpcLogDrain::kMinEntrySizeWithoutPayload + kMaxMessageSize;
  std::array<std::byte, kLogBufferSize> multisink_buffer;

  // To save RAM, share the mutex, since drains will be managed sequentially.
  pw::sync::Mutex shared_mutex;
  std::array<std::byte, kMaxEntrySize> client1_buffer
      PW_GUARDED_BY(shared_mutex);
  std::array<std::byte, kMaxEntrySize> client2_buffer
      PW_GUARDED_BY(shared_mutex);
  std::array<pw::log_rpc::RpcLogDrain, 2> drains = {
      pw::log_rpc::RpcLogDrain(
          1,
          client1_buffer,
          shared_mutex,
          RpcLogDrain::LogDrainErrorHandling::kIgnoreWriterErrors),
      pw::log_rpc::RpcLogDrain(
          2,
          client2_buffer,
          shared_mutex,
          RpcLogDrain::LogDrainErrorHandling::kIgnoreWriterErrors),
  };

  pw::sync::InterruptSpinLock log_encode_lock;
  std::array<std::byte, kMaxLogEntrySize> log_encode_buffer
      PW_GUARDED_BY(log_encode_lock);

  extern "C" void pw_tokenizer_HandleEncodedMessageWithPayload(
      pw_tokenizer_Payload metadata, const uint8_t message[], size_t size_bytes) {
    int64_t timestamp =
        pw::chrono::SystemClock::now().time_since_epoch().count();
    std::lock_guard lock(log_encode_lock);
    pw::Result<pw::ConstByteSpan> encoded_log_result =
      pw::log::EncodeTokenizedLog(
          metadata, message, size_bytes, timestamp, log_encode_buffer);

    if (!encoded_log_result.ok()) {
      GetMultiSink().HandleDropped();
      return;
    }
    GetMultiSink().HandleEntry(encoded_log_result.value());
  }
  }  // namespace

  pw::log_rpc::RpcLogDrainMap drain_map(drains);
  pw::log_rpc::RpcLogDrainThread log_thread(GetMultiSink(), drain_map);
  pw::log_rpc::LogService log_service(drain_map);

  pw::multisink::MultiSink& GetMultiSink() {
    static pw::multisink::MultiSink multisink(multisink_buffer);
    return multisink;
  }
  }  // namespace foo_log

Logging in other source files
-----------------------------
To defer logging, other source files must simply include ``pw_log/log.h`` and
use the :ref:`module-pw_log` APIs, as long as the source set that includes
``foo_log.cc`` is setup as the log backend.
