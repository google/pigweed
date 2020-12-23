.. _module-pw_symbolizer:

-------------
pw_symbolizer
-------------

.. warning::
  This module is under construction and may not be ready for use.

pw_symbolizer provides python-based tooling for symbolizing addresses emitted by
on-device firmware.

LlvmSymbolizer
==============
The ``LlvmSymbolizer`` is a python layer that wraps ``llvm-symbolizer`` to
produce symbols from provided addresses.

.. code:: py

  import pw_symbolizer

  symbolizer = pw_symbolizer.LlvmSymbolizer(Path('device_fw.elf'))
  sym = symbolizer.symbolize(0x2000ac21)
  print(f'You have a bug here: {sym}')

It can also be used to create nicely formatted symbolized stack traces.

.. code:: py

  import pw_symbolizer

  symbolizer = pw_symbolizer.LlvmSymbolizer(Path('device_fw.elf'))
  print(symbolizer.dump_stack_trace(backtrace_addresses))

Which produces output like this:

.. code-block:: none

  Stack Trace (most recent call first):
     1: at device::system::logging_thread_context (0x08004BE0)
        in threads.cc:0
     2: at device::system::logging_thread (0x0800B508)
        in ??:?
     3: at device::system::logging_thread_context (0x08004CB8)
        in threads.cc:0
     4: at device::system::logging_thread (0x0800B3C0)
        in ??:?
     5: at device::system::logging_thread (0x0800B508)
        in ??:?
     6: at (0x0800BAF7)
        in ??:?
     7: at common::log::LoggingThread::Run() (0x0800BAD1)
        in out/common/log/logging_thread.cc:26
     8: at pw::thread::threadx::Context::ThreadEntryPoint(unsigned long) (0x0800539D)
        in out/pigweed/pw_thread_threadx/thread.cc:41
     9: at device::system::logging_thread_context (0x08004CB8)
        in threads.cc:0
    10: at device::system::logging_thread_context (0x08004BE0)
        in threads.cc:0
