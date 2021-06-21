.. _module-pw_multisink:

============
pw_multisink
============
This is an module that forwards messages to multiple attached sinks, which
consume messages asynchronously. It is not ready for use and is under
construction.

Module Configuration Options
============================
The following configurations can be adjusted via compile-time configuration
of this module, see the
:ref:`module documentation <module-structure-compile-time-configuration>` for
more details.

.. c:macro:: PW_MULTISINK_CONFIG_LOCK_INTERRUPT_SAFE

  Whether an interrupt-safe lock is used to guard multisink read/write
  operations.

  By default, this option is enabled and the multisink uses an interrupt
  spin-lock to guard its transactions. If disabled, a mutex is used instead.

  Disabling this will alter the entry precondition of the multisink,
  requiring that it not be called from an interrupt context.

Iterator
========
It may be useful to access the entries in the underlying buffer when no drains
are attached or in crash contexts where dumping out all entries is desirable,
even if those entries were previously consumed by a drain. This module provides
an iteration class that is thread-unsafe and like standard iterators, assumes
that the buffer is not being mutated while iterating. A
`MultiSink::UnsafeIterationWrapper` class that supports range-based for-loop
usage canbe acquired via `MultiSink::UnsafeIteration()`.

The iterator starts from the oldest available entry in the buffer, regardless of
whether all attached drains have already consumed that entry. This allows the
iterator to be used even if no drains have been previously attached.

.. code-block:: cpp

  // Create a multisink and a test string to push into it.
  constexpr char kExampleEntry[] = "Example!";
  std::byte buffer[1024];
  MultiSink multisink(buffer);
  MultiSink::Drain drain;

  // Push an entry before a drain is attached.
  multisink.HandleEntry(kExampleEntry);
  multisink.HandleEntry(kExampleEntry);

  // Iterate through the entries, this will print out:
  //  "Example!"
  //  "Example!"
  // Note: PrintByteArray is not a provided utility function.
  for (ConstByteSpan entry : multisink.UnsafeIteration()) {
    PrintByteArray(entry);
  }

  // Attach a drain and consume only one of the entries.
  std::byte read_buffer[512];
  uint32_t drop_count = 0;

  multisink.AttachDrain(drain);
  drain.GetEntry(read_buffer, drop_count);

  // !! A function causes a crash before we've read out all entries.
  FunctionThatCrashes();

  // ... Crash Context ...

  // You can use a range-based for-loop to walk through all entries,
  // even though the attached drain has consumed one of them.
  // This will also print out:
  //  "Example!"
  //  "Example!"
  for (ConstByteSpan entry : multisink.UnsafeIteration()) {
    PrintByteArray(entry);
  }
