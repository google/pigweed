.. _module-pw_cpu_exception_risc_v:

=======================
pw_cpu_exception_risc_v
=======================
This module lays the foundation for implementing a backend cpu exception
for the RISCV architectures:

For now, this contains the cpu state, protos, and tools needed for parsing
and the snapshot. The entry for exception handling will still need to be
implemented by the user.

Python processor
================
This module's included Python exception analyzer tooling provides snapshot
integration via a ``process_snapshot()`` function that produces a multi-line
dump from a serialized snapshot proto, for example:

.. code-block::

   All registers:
   mepc       0x20047144 example::Serivice::Crash(_example_service_CrashRequest const&, _pw_protobuf_Empty&) (src/example_service/service.cc:131)
   mcause     0x00000003
   mstatus    0x80007800
   mtval      0x00000000
   ra         0x2001e92c pw_log_tokenized_HandleLog (src/system/log.cc:124)
   sp         0x00095660
   t0         0x20047120
   t1         0xfffffff0
   t2         0x00000800
   fp         0x00095760
   s1         0x000956dc
   a0         0x00000004
   a1         0x000956dc
   a2         0x00000000
   a3         0x00000004
   a4         0x00000007
   a5         0x000814b0
   a6         0x000955e6
   a7         0x00000000
   s2         0x00000004
   s3         0x20059114
   s4         0x00000000
   s5         0x000aae98
   s6         0x00084a8f
   s7         0xb7002653
   s8         0xd20200f3
   s9         0x00000002
   s10        0xa5a5a5a5
   s11        0xa5a5a5a5
   t3         0xfffffffd
   t4         0x00000000
   t5         0xa5a5a5a5
   t6         0xa5a5a5a5

Module Configuration Options
============================
The following configurations can be adjusted via compile-time configuration of
this module, see the
:ref:`module documentation <module-structure-compile-time-configuration>` for
more details.

.. c:macro:: PW_CPU_EXCEPTION_RISCV_LOG_LEVEL

   The log level to use for this module. Logs below this level are omitted.

   This defaults to ``PW_LOG_LEVEL_DEBUG``.
