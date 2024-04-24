.. _seed-0124:

===============================================================
0124: Interfaces for Retrieving Size Information from Multisink
===============================================================
.. seed::
   :number: 124
   :name: Interfaces for Retrieving Size Information from Multisink
   :status: open_for_comments
   :proposal_date: 2024-01-22
   :cl: 188670
   :authors: Jiacheng Lu
   :facilitator: Carlos Chinchilla

-------
Summary
-------
This SEED proposes adding interfaces to :ref:`module-pw_multisink` to retrieve
the capacity and filled size from its underlying buffer.

----------
Motivation
----------
Currently, ``pw_multisink`` provides ``Listener`` to help schedule draining of
entries. Interfaces of ``pw_multisink`` and its ``Listener`` provides no
information about capacity or consumed size of the underlying buffer.

By adding interfaces to ``pw_multisink`` to provide size information of its
underlying buffer, scheduling policies can be implemented to trigger draining
based on, for example, the size of unread data in the buffer.

--------
Proposal
--------

Add new interface ``UnsafeGetUnreadEntriesSize()`` to
``pw_multisink::MultiSink::Drain`` that reports size of unread data in the
underlying buffer.

  .. code-block:: cpp

     class MutilSink {
      public:
      ...

        class Drain {
           ...

           // Both two APIs beturn size in bytes of the valid data in the
           // underlying buffer that has not been read by this drain.

           // This is a thread unsafe version. It requires the `lock` of
           // `multisink` being held. For example, it can be used inside
           // Listeners' methods, where lock already held, to check size
           // on handling each new entry.
           size_t UnsafeGetUnreadEntriesSize() PW_NO_LOCK_SAFETY_ANALYSIS;

           // A thread-safe verson.
           size_t GetUnreadEntriesSize() PW_LOCKS_EXCLUDED(multisink_->lock_);

           ...

         protected:
           friend class MultiSink;

           MultiSink* multisink_;
        };

      ...

      private:
        LockType lock_;
     };


---------------------
Problem investigation
---------------------
``pw_multisink`` is a popular choice to buffer data from logs for Pigweed based
softwares. An example is :ref:`module-pw_log_rpc` where an instance of
``pw_multisink`` is used to buffer logs before they are drained and transmitted.
Make sure ``pw_multisink`` are drained in time is important to reduce the
chances of dropping logs because of running out of space.
However, the draining and transmission behavior may be constrained by the
property of underlying physical channels. For certain physical channels, there
are tradeoffs between frequency of transmission and costs.

PCI is one of the example of physical channels that have the above mentioned
tradeoffs. PCI implementation normally have different levels of power states for
power efficiency. Transmitting data over PCI continuously may result to it not
being able to enter deep sleep states. Also, PCI are normally have a high
transmission bandwidth. Buffering data to a certain threshold and then
sending them all over in a single transmission fits better with its design.

The ``OnNewEntryAvailable`` on the current ``Listener`` interfaces does not
provide enough information about buffered data size because the current
implementation of underlying buffer is :ref:`module-pw_ring_buffer`, it stores
additional, variable lenghted data for its internal for each entry.


Assuming the proposed interface is avaible, the imagined use case looks like:

  .. code-block:: cpp

     class DrainThread: public pw::thread::ThreadCore,
                        public pw::multisink::MultiSink::Listener {
      public:

        ... // initialize with drain

        bool NeedFlush(size_t unread_entries_size) {
           ...
        }

        void Flush(pw::multisink::MultiSink::Drain& drain) {
          ...
        }

        void OnNewEntryAvailable() override {
           if (NeedFlush(drain_.UnsafeGetUnreadEntriesSize())) {
              flush_threshold_reached_notification_.release();
           }
        }

        void Run() override {
           multisink_.AttachListner(*this);

           while (true) {
              flush_threshold_reached_notification_.acquire();
              Flush(drain_);
           }
        }


      private:
        pw::multisink::MultiSink& multisink_;
        pw::multisink::MultiSink::Drain& drain_;
        pw::ThreadNotification flush_threshold_reached_notification_;
     };


---------------
Detailed design
---------------

Implement ``EntriesSize()`` in
``pw_ring_buffer::PrefixedEntryRingBufferMulti::Reader`` to provide the number
of bytes between its reader pointer and ring buffer's writer pointer.

  .. code-block:: cpp

     class PrefixedEntryRingBufferMulti {
       class Reader : public IntrusiveList<Reader>::Item {
        public:

         // Get the size of the unread entries currently in the ring buffer.
         // Return value:
         // Number of bytes
         size_t EntriesSize() const {
           // Case: Not wrapped.
           if (read_idx_ < buffer_->write_idx_) {
             return buffer_->write_idx_ - read_idx_;
           }
           // Case: Wrapped.
           if (read_idx_ > buffer_->write_idx_) {
             return buffer_->buffer_bytes_ - (read_idx_ - buffer_->write_idx_);
           }

           // No entries remaining.
           if (entry_count_ == 0) {
             return 0;
           }

           return buffer_->buffer_bytes_;
         }

        private:
         PrefixedEntryRingBufferMulti* buffer_;
         size_t read_idx_;
       };

      private:
       size_t write_idx_;
       size_t buffer_bytes_;
     };


The unread data size of ``Drain`` is directly fetched from ring buffer's
``Reader``. Because ``pw_multisink`` uses ``lock_`` to protect accesses to all
listeners' methods already, in order to support calling the proposed interfaces
from listeners, this design introduces two versions of API, one thread-safe
version that is intended to be used outside of listeners, and one thread-unsafe
version requires that ``lock_`` of ``pw_multisink`` being held when invoking.

  .. code-block:: cpp

     namespace pw {
     namespace multisink {

     class MutilSink {
      public:
       ...

        class Drain {
         public:

           // Both two APIs beturn size in bytes of the valid data in the
           // underlying buffer that has not been read by this drain.

           // Ideally it should use annotation
           //     PW_EXCLUSIVE_LOCKS_REQUIRED(multisink_->lock_)
           // however, Listener interfaces, where it is intended to be called,
           // cannot be annotated using multisink's lock. Static analysis is not
           // doable.
           size_t UnsafeGetUnreadEntriesSize() PW_NO_LOCK_SAFETY_ANALYSIS {
              return reader_.EntriesSize();
           }

           size_t GetUnreadEntriesSize() PW_LOCKS_EXCLUDED(multisink_->lock_) {
              std::lock_guard lock(multisink_->lock_);
              return UnsafeGetUnreadEntriesSize();
           }

         protected:
           friend class MultiSink;

           MultiSink* multisink_;
           ring_buffer::PrefixedEntryRingBufferMulti::Reader reader_;
        };

      ...

      private:
        LockType lock_;
        ring_buffer::PrefixedEntryRingBufferMulti ring_buffer_
           PW_GUARDED_BY(lock_);
     };

     }  // namespace multisink
     }  // namespace pw

------------
Alternatives
------------

Add on buffer size change interface to listener
===============================================
Add ``OnBufferSizeChange`` interface to ``pw_multisink::MultiSinkListener``. The
new interface gets invoked when the available size of the underlying buffer
changes.

Interface example:

  .. code-block:: cpp

     class MultiSink {
      public:
       class Listener {
        public:

         ...

         virtual void OnNewEntryAvailable() = 0;

         virtual void OnBufferSizeChange(size_t total_size, size_t used_sized);
       };

       ...
     }


Imagined implementation of ``OnBufferSizeChange`` being invoked after an entry
push or pop. It uses existing interfaces of the underlying
:ref:`module-pw_ring_buffer`. In reality, this implementation does not work well,
explained in the **problems** sections below.

  .. code-block:: cpp

     void MutilSink::HandleEntry(ConstByteSpan entry) {
       std::lock_guard lock(lock_);
       ...
       ring_buffer_.PushBack(entry);
       NotifyListenersBufferSizeChanged(
         ring_buffer_.TotalSizeBytes(),
         ring_buffer_.TotalUsedBytes());
       ...
     }

  .. code-block:: cpp

    void MutilSink::PopEntry(Drain& drain, ConstByteSpan entry) {
      std::lock_guard lock(lock_);
      ...
      const size_t used_size_before_pop = ring_buffer_.TotalUsedBytes();
      drain.reader_.PopFront();
      const size_t used_size_after_pop = ring_buffer_.TotalUsedBytes();
      if (used_size_before_pop != used_size_after_pop) {
        NotifyListenersBufferSizeChanged(
          ring_buffer_.TotalSizeBytes(),
          used_size_after_pop);
      }
      ...
    }


Problem 1. Find the slowest drain
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
When there are multiple drains attached to ``pw_multisink``, only the slowest
drain(s) frees space from the underlying ``pw_ring_buffer`` when pops.

``pw_multisink`` supports :ref:`module-pw_multisink-late_drain_attach` which
attaches an internal drain that never pops. The ``TotalUsedBytes()`` reported by
underlying ``pw_ring_buffer`` counts from the slowest drain and always reports
the full capacity instead of the real used size.


Problem 2. Push a entry when buffer is full may decrease used size
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
When the pushing of a new entry exceeds the remaining free space of the
underlying buffer, the push can still succeed, by silent dropping entries from
the slowest drain. However, depending on the size of dropped entries and the
size of the new entry, the used buffer size may increase, decrease or stay the
same.

If the user of the proposed ``OnBufferSizeChange`` interface is comparing the
reported used bytes with a threshold value, it is likely that the moment of
underlying buffer being full may not be catched.

Although it is possible to also trigger ``OnBufferSizeChange`` with
``used_size == total_size`` when the above situation happens, the implementation
may also require exposing internal states of ``pw_ring_buffer``.

--------------
Open questions
--------------
