.. _showcase-sense-tutorial-crash-handler:

=====================================
15. Handle crashes and view snapshots
=====================================
When a system crashes it can be difficult to debug the cause of the
crash. To help with this, :ref:`pw_system <module-pw_system>` provides
a crash handler built on :ref:`module-pw_cpu_exception` which is invoked
when a CPU exception is triggered.

The :ref:`pw_system <module-pw_system>` crash handler will automatically
create a crash snapshot on exception and reboot the system. This snapshot can
then be downloaded onto a host system for analysis.


.. _showcase-sense-tutorial-crash-handler-crash:

----------------
Generate a crash
----------------
#. If you're not already connected to your Pico via ``pw_console``, do so now:

   .. tab-set::

      .. tab-item:: VS Code
         :sync: vsc

         .. tab-set::

            .. tab-item:: Pico 1 & 1W (RP2040)
               :sync: rp2040

               In **Bazel Targets** expand **//apps/production**, then right-click
               **:rp2040_console (native binary)**, then select **Run target**.

            .. tab-item:: Pico 2 & 2W (RP2350)
               :sync: rp2350

               In **Bazel Targets** expand **//apps/production**, then right-click
               **:rp2350_console (native binary)**, then select **Run target**.

      .. tab-item:: CLI
         :sync: cli

         Run the terminal-based console:

         .. tab-set::

            .. tab-item:: Pico 1 & 1W (RP2040)

               .. code-block:: console

                  bazelisk run //apps/production:rp2040_console

            .. tab-item:: Pico 2 & 2W (RP2350)

               .. code-block:: console

                  bazelisk run //apps/production:rp2350_console

``pw_system`` provides an RPC to crash the system by triggering a
`HardFault <https://developer.arm.com/documentation/107706/0100/System-exceptions/Fault-exceptions-and-their-causes>`_.
To invoke this RPC, type the following into the **Python Repl** (bottom-left pane) of ``pw_console``:

.. code-block:: pycon

   >>> device.rpcs.pw.system.proto.DeviceService.Crash()

If you're using the :ref:`full setup <showcase-sense-tutorial-hardware>`,
on system restart the presence of a crash snapshot will be detected, and
the following will be in the system logs:

.. code-block:: text

   INF  pw_system  RpcDevice  00:00:00.000  pw_system main
   ERR  pw_system  RpcDevice  00:00:00.000  ==========================
   ERR  pw_system  RpcDevice  00:00:00.000  ======CRASH DETECTED======
   ERR  pw_system  RpcDevice  00:00:00.000  ==========================
   ERR  pw_system  RpcDevice  00:00:00.000  Crash snapshots available.
   ERR  pw_system  RpcDevice  00:00:00.000  Run `device.get_crash_snapshots()` to download and clear the snapshots.
   INF  pw_system  RpcDevice  00:00:00.000  System init


On a :ref:`"Pico and Enviro+" or "Pico only" <showcase-sense-tutorial-hardware>`
setup, the snapshot will still be generated, but logs won't be visible in the console. When
the system restarts, the connection to the console is broken so logs
will not be displayed. After invoking the ``Crash()`` RPC, exit the console
and :ref:`start it again <showcase-sense-tutorial-pico-rpc-interact>` to
re-establish the connection.

.. _showcase-sense-tutorial-crash-handler-view:

---------------------
View a crash snapshot
---------------------

The crash snapshot contains relevant information to debug crashes, such
as register state thread backtraces and un-flushed logs. If there is a
crash snapshot on the system, it can be downloaded to the host with the
following RPC.

.. code-block:: pycon

   >>> device.get_crash_snapshots()

This RPC will download the snapshot, decode it and save it in a
temporary directory, the location of which will be printed to the
console as follows:

.. code-block:: text

   INF  Wrote crash snapshot to: /var/folders/2j/sjk9390d5rxc3c9ycwcf3mdh0103lh/T/crash_0.txt

It's also possible to specify the path as part of the RPC call:

.. code-block:: pycon

   >>> device.get_crash_snapshots("/path/")

The decoded text file should look similar to this truncated example:

.. code-block::

   Device crash cause:
       pw_system/device_service_pwpb.cc:38 Crash: RPC triggered crash

   Reason token:      0x735f7770
   CPU Arch:          ARMV8M

   Exception caused by a usage fault.

   Active Crash Fault Status Register (CFSR) fields:
   UNDEFINSTR  Undefined Instruction UsageFault.
       The processor has attempted to execute an undefined
       instruction. When this bit is set to 1, the PC value stacked
       for the exception return points to the undefined instruction.
       An undefined instruction is an instruction that the processor
       cannot decode.

   All registers:
   pc         0x10000f0a pw::system::DeviceServicePwpb::Crash(pw::system::proto::pwpb::CrashRequest::Message const&, pw::system::proto::pwpb::CrashResponse::Message&) (/b/pw_system/device_service_pwpb.cc:38)
   lr         0x10012787 pw::StringBuilder::FormatVaList(char const*, std::__va_list) (/build/pw_string/string_builder.cc:102)
   psr        0x41000000
   msp        0x20081fe0 __scratch_y_end__ (??:?)
   psp        0x2000a100 pw::system::rpc_thread_context (freertos_target_hooks.cc:0)
   exc_return 0xfffffffd
   cfsr       0x00010000
   msplim     0x00000000
   psplim     0x20002288
   mmfar      0xe000ed34 __scratch_y_end__ (??:?)
   bfar       0xe000ed38 __scratch_y_end__ (??:?)
   icsr       0x00400806
   hfsr       0x00000000
   shcsr      0x00070008
   control    0x00000000
   r0         0x2000a0e0 pw::system::rpc_thread_context (freertos_target_hooks.cc:0)
   r1         0x0000003e pw_assert_basic_HandleFailure (/b/pw_assert_basic/basic_handler.cc:74)
   r2         0x0000002b pw_assert_basic_HandleFailure (/b/pw_assert_basic/basic_handler.cc:74)
   r3         0x2000a100 pw::system::rpc_thread_context (freertos_target_hooks.cc:0)
   r4         0x10019596
   r5         0x2000a178 pw::system::rpc_thread_context (freertos_target_hooks.cc:0)
   r6         0x10019eec pw::system::proto::pw_rpc::pwpb::DeviceService::Service<pw::system::DeviceServicePwpb>::kPwRpcMethods (??:?)
   r7         0x2000a108 pw::system::rpc_thread_context (freertos_target_hooks.cc:0)
   r8         0x2000a118 pw::system::rpc_thread_context (freertos_target_hooks.cc:0)
   r9         0x2000a16e pw::system::rpc_thread_context (freertos_target_hooks.cc:0)
   r10        0x2000b4f0 pw::system::(anonymous namespace)::server (hdlc_rpc_server.cc:0)
   r11        0x2000a22c pw::system::rpc_thread_context (freertos_target_hooks.cc:0)
   r12        0x00000008 pw_assert_HandleFailure (/b/pw_assert_basic/assert_basic.cc:20)

   Thread State
     6 threads running, RpcThread active at the time of capture.
                       ~~~~~~~~~

   Thread (RUNNING): RpcThread <-- [ACTIVE]
   Est CPU usage: unknown
   Stack info
     Current usage:   0x2000a288 - 0x2000a100 (392 bytes, 1.20%)
     Est peak usage:  944 bytes, 2.88%
     Stack limits:    0x2000a288 - 0x2000228c (32764 bytes)
   Stack Trace (most recent call first):
     1: at void pw::rpc::internal::PwpbMethod::CallSynchronousUnary<pw::system::proto::pwpb::RebootRequest::Message, pw::system::proto::pwpb::RebootResponse::Message>(pw::rpc::internal::CallContext const&, pw::rpc::internal::Packet const&, pw::system::proto::pwpb::RebootRequest::Message&, pw::system::proto::pwpb::RebootResponse::Message&) const (0x10000F59)
         in /build/pw_rpc/pwpb/public/pw_rpc/pwpb/internal/method.h:258
     2: at void pw::rpc::internal::PwpbMethod::CallSynchronousUnary<pw::system::proto::pwpb::CrashRequest::Message, pw::system::proto::pwpb::CrashResponse::Message>(pw::rpc::internal::CallContext const&, pw::rpc::internal::Packet const&, pw::system::proto::pwpb::CrashRequest::Message&, pw::system::proto::pwpb::CrashResponse::Message&) const (0x10001137)
         in /build/pw_rpc/pwpb/public/pw_rpc/pwpb/internal/method.h:267
     3: at xQueueSemaphoreTake (0x10013049)
         in /build/external/freertos+/queue.c:1555
     4: at void pw::rpc::internal::PwpbMethod::SynchronousUnaryInvoker<pw::system::proto::pwpb::CrashRequest::Message, pw::system::proto::pwpb::CrashResponse::Message>(pw::rpc::internal::CallContext const&, pw::rpc::internal::Packet const&) (0x10000F4F)
         in /build/pw_rpc/pwpb/public/pw_rpc/pwpb/internal/method.h:322
     5: at pw::rpc::Server::ProcessPacket(pw::rpc::internal::Packet) (0x1000EA9D)
         in /build/pw_rpc/public/pw_rpc/internal/method.h:0
     6: at pw::rpc::Server::ProcessPacket(pw::span<std::byte const, 4294967295u>) (0x1000E9CD)
         in /build/pw_rpc/server.cc:40
     7: at pw::system::RpcDispatchThread::Run() (0x10008625)
         in /build/pw_system/hdlc_rpc_server.cc:127
     8: at pw::thread::freertos::Context::ThreadEntryPoint(void*) (0x1000EFA5)
         in /build/third_party/fuchsia/repo/sdk/lib/fit/include/lib/fit/internal/function.h:362
     9: at prvTaskExitError (0x100137C9)
         in /build/external/freertos+/portable/GCC/ARM_CM33_NTZ/non_secure/port.c:634

    ...

   Device Logs:
   [RpcDevice] pw_system 0 pw_system main targets/rp2040/boot.cc:56
   [RpcDevice] pw_system 0 System init pw_system/init.cc:65
   [RpcDevice] pw_system 0 Registering RPC services pw_system/init.cc:75

    ...


.. _showcase-sense-tutorial-crash-handler-summary:

-------
Summary
-------
On this page, we met ``pw_cpu_exception``, the CPU exception handler entry point.
We also learned how to generate crashes and download the resulting crash snapshot.

Next, head over to :ref:`showcase-sense-tutorial-outro` to wrap up your
tour of Pigweed.
