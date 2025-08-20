:tocdepth: 2
:nosearch:

.. _docs-changelog:

=====================
What's new in Pigweed
=====================

------------------------------------------------
(June 2025) This changelog is temporarily paused
------------------------------------------------
.. changelog_highlights_start

.. _commit log: https://pigweed.googlesource.com/pigweed/pigweed/+log/refs/heads/main

Our changelog (:ref:`docs-changelog`) is paused while we investigate how to
further automate our changelog authoring process to make the workload more
manageable. Only the changelog is paused; Pigweed itself is still a highly
active project!  In the meantime, you can join our monthly community meeting
(:ref:`Pigweed Live <docs-changelog-live>`) or check out our `commit log`_
to get a sense of what we've been working on recently.

.. changelog_highlights_end

.. _docs-changelog-live:

--------------------------------
Talk to the team at Pigweed Live
--------------------------------
.. pigweed-live::

.. _docs-changelog-latest:
.. _docs-changelog-2025-02-06:

-----------
Feb 6, 2025
-----------
Highlights (Jan 25, 2025 to Feb 6, 2025):

* **Tokenization improvements in C++**: The new
  :cpp:func:`pw::tokenizer::Detokenizer::FromCsv` C++ method constructs a
  detokenizer from a CSV file. :ref:`Tokenization domains <seed-0105>` are now
  supported in C++.

* **New global variables wrapper** :cpp:class:`pw::RuntimeInitGlobal` declares
  a global variable that is initialized at runtime. Its destructor is never
  run.

* **Bazel-based docs build**: ``pigweed.dev`` is now built with Bazel. See
  :ref:`blog-08-bazel-docgen`.

.. _docs-changelog-2025-02-06-Modules:

Modules
=======

.. _docs-changelog-2025-02-06-Modules-pw_allocator:

pw_allocator
------------
New features:

.. 9f0f4f8d888e7ebfd228efd622df996f3125f2f1

* :cpp:class:`pw::Allocator` now has more overloads for customizing
  allocated array alignment. Commit: `Add overloads to customize allocated
  array alignment <https://pwrev.dev/260276>`__.

Changes:

.. c48adeb92ff36577a34f22cf7b2a0b8d0086a3b2

* ``pw::allocator::test::SynchronizedAllocatorForTest`` was removed.
  Commit: `Remove SynchronizedAllocatorForTest
  <https://pwrev.dev/264698>`__.

.. _docs-changelog-2025-02-06-Modules-pw_bluetooth:

pw_bluetooth
------------
New features:

.. 7bec9117b6f21884a5046ecabf351b671b167885

* ``pw_bluetooth`` now supports a :ref:`snoop log
  <module-pw_bluetooth-snoop-log>` for recording
  HCI RX/TX traffic. Commit: `Add snoop log <https://pwrev.dev/226611>`__.
  Bug: :bug:`389995204`.

.. _docs-changelog-2025-02-06-Modules-pw_bluetooth_proxy:

pw_bluetooth_proxy
------------------
New features:

.. 89a3c9cab649585ebeb7b863035d28ff419829bc

* The new :cpp:class:`pw::bluetooth::proxy::AcquireGattNotifyChannel`
  method returns a GATT Notify channel that supports sending notifications
  to a particular connection handle and attribute. Commit: `Support
  acquire of gatt notify channels <https://pwrev.dev/264954>`__. Bugs:
  :bug:`369709521`, :bug:`379337272`.

.. _docs-changelog-2025-02-06-Modules-pw_bluetooth_sapphire:

pw_bluetooth_sapphire
---------------------
New features:

.. c6f84aa0c5a57695a4a57e3811c89135e9344f00

* :ref:`module-pw_bluetooth_sapphire-fuchsia-zxdb` explains how to use
  Fuchsia's kernel debugger. Commit: `Document how to use the Zxdb
  Debugger <https://pwrev.dev/263812>`__.

.. _docs-changelog-2025-02-06-Modules-pw_containers:

pw_containers
-------------
New features:

.. 16ddae866c80f3cee061f189cee149700f81d188

* :cpp:class:`pw::Vector` now has an explicit ``constexpr`` constructor.
  Using this constructor will place the entire object in ``.data`` by
  default, which will increase ROM size. Commit: `Add explicit constexpr
  constructor for Vector <https://pwrev.dev/263692>`__.

Bug fixes:

.. 852571bdf8b031b7ca89fff2ae9fdaa4f6cfe0a6

* A bug was fixed where ``pw::Vector::insert`` was move assigning to
  destroyed objects. Commit: `Do not move assign to destroyed objects in
  Vector::insert <https://pwrev.dev/252452>`__. Bug: :bug:`381942905`.

.. _docs-changelog-2025-02-06-Modules-pw_crypto:

pw_crypto
---------
New features:

.. a1eb87b67a72231755b7fd6671e99d7f7276ba20

* :cpp:class:`pw::crypto::aes_cmac::Cmac` provides support for the
  AES-CMAC algorithm. Commit: `Add Aes::Cmac <https://pwrev.dev/231913>`__.

.. _docs-changelog-2025-02-06-Modules-pw_tokenizer:

pw_tokenizer
------------
New features:

.. 9b46aef8010f3c2edd87f2365d9ef6ee4656df56

* The new :cpp:func:`pw::tokenizer::Detokenizer::FromCsv` C++ method
  constructs a detokenizer from a CSV file. Commit: `Add support for CSV
  parsing in C++ <https://pwrev.dev/256653>`__.

.. 8fe4260fdbbf806a2470396d0d1da1bb6b15d522

* :ref:`Tokenization domains <seed-0105>` are now supported in C++.
  Commit: `Add support for domains in C++ <https://pwrev.dev/255173>`__.

.. _docs-changelog-2025-02-06-Modules-pw_toolchain:

pw_toolchain
------------
New features:

.. 5f9420a551774235c883a621e639228525b13591

* :cpp:class:`pw::RuntimeInitGlobal` is a new wrapper for global
  variables. See :ref:`module-pw_toolchain-cpp-globals`. Commit:
  `Introduce RuntimeInitGlobal <https://pwrev.dev/263875>`__.

.. d2f7a36184d742e27761591a0dac13fa421e6b32

* :ref:`module-pw_toolchain` has started to support a Zephyr toolchain.
  Commit: `Add support for Zephyr toolchain <https://pwrev.dev/263836>`__.

.. 51a7b5cc4bbdbb54d4eb0a9b0200a76c7c479c65

* :ref:`module-pw_toolchain-bazel-clang-tidy` explains how to integrate
  Pigweed's toolchain with ``clang-tidy``. Commit: `Document clang-tidy +
  Bazel <https://pwrev.dev/262873>`__. Bug: :bug:`341723612`.

.. _docs-changelog-2025-02-06-Modules-pw_transfer:

pw_transfer
-----------
Bug fixes:

.. d95bb9205b6826e7c35f06f2ae53e354a1d795e8

* In proto3, when a retry config option is 0, ``pw_transfer`` no longer
  attempts to set it. Commit: `Ignore 0 retry values from config proto
  <https://pwrev.dev/265253>`__. Bug: :bug:`357145010`.

.. _docs-changelog-2025-02-06-Build-systems:

Build systems
=============

.. _docs-changelog-2025-02-06-Build-systems-Bazel:

Bazel
-----
New features:

.. 5f466288aea2a17142258a4ed0683b57aa59c711

* The Pigweed Bazel build has started to support Zephyr's toolchain.
  Commit: `Add Zephyr toolchain CIPD repo <https://pwrev.dev/263832>`__.

.. _docs-changelog-2025-02-06-Docs:

Docs
====
New features:

.. 18076431f27790a69116ef91bb8f8c877eb6a479

* `pigweed.dev/rustdoc <https://pigweed.dev/rustdoc>`__ now provides an
  index of all Pigweed Rust crates. Previously, that page would 404.
  Commit: `Add index page to Rust API docs <https://pwrev.dev/263838>`__.

.. 55b363ba45eb871df096ca060c93a45f73b741a7

* When viewing the docs on a staging site, there's now a banner at the
  top of the docs site to make it clear that you're not viewing the
  official Pigweed docs. Commit: `Present banner on staged docs
  <https://pwrev.dev/263513>`__. Bug: :bug:`304835851`.

.. 809d32b33e2dd368bc91704e7d70f7069d6cc7d9

* ``pigweed.dev`` is now built with Bazel. See
  :ref:`blog-08-bazel-docgen`. Commit: `Add Bazel migration blog post
  <https://pwrev.dev/264515>`__.

.. _docs-changelog-2025-01-24:

------------
Jan 24, 2025
------------
.. changelog_highlights_start

Highlights (Jan 10, 2025 to Jan 24, 2025):

* **Thread creation API**: The cross-platform thread creation API proposed
  in :ref:`seed-0128` has been implemented.
* **Layering check**: Upstream Pigweed toolchains now support
  :ref:`layering check <module-pw_toolchain-bazel-layering-check>` in Bazel.
  Including headers that aren't in the ``hdrs`` of a ``cc_library``
  you directly depend on becomes a compile-time error.
* **Cortex-A support**: pw_interrupt_cortex_a is a new
  ``pw_interrupt`` backend for Arm Cortex-A processors. ``pw_toolchain``
  now supports Arm Cortex-A35.
* **Atomic API**: The new :ref:`module-pw_atomic` module provides software
  implementations of atomic operations.

.. changelog_highlights_end

.. _docs-changelog-2025-01-24-Modules:

Modules
=======

.. _docs-changelog-2025-01-24-Modules-pw_atomic:

pw_atomic
---------
New features:

.. 120f202e25b4c59111012e9568c74f4af2fdb09d

* The new :ref:`module-pw_atomic` module provides software
  implementations of atomic operations. Commit: `Add module for atomic
  operations <https://pwrev.dev/239719>`__.

.. _docs-changelog-2025-01-24-Modules-pw_bluetooth_proxy:

pw_bluetooth_proxy
------------------
New features:

.. c20f1e99713ae9959a223d8504690ccbf98f260f

* Clients of ``pw_bluetooth_proxy`` can now register a callback function
  for inspecting host-to-controller L2CAP basic channel packets. Commit:
  `Add host to controller callback packet sniffing
  <https://pwrev.dev/260553>`__. Bug: :bug:`390191420`.

.. _docs-changelog-2025-01-24-Modules-pw_cli:

pw_cli
------
New features:

.. 69614fba8ccf19b2dcaf60f214353a733329fcaa

* The new :py:class:`pw_cli.git_repo.GitRepoFinder` helper class
  efficiently finds Git repo roots. Commit: `Add helper for efficiently
  finding git repo roots <https://pwrev.dev/254024>`__. Bug:
  :bug:`326309165`.

.. _docs-changelog-2025-01-24-Modules-pw_digital_io_mcuxpresso:

pw_digital_io_mcuxpresso
------------------------
New features:

.. f282c15c48732f4c4e1e3da15c1cfddea2b9cb47

* :cpp:class:`pw::digital_io::McuxpressoDigitalInOutInterrupt` now
  supports interrupt triggers on both edges. Commit: `Emulate kBothEdges
  trigger via level interrupt <https://pwrev.dev/260793>`__. Bug:
  :bug:`390456846`.

.. _docs-changelog-2025-01-24-Modules-pw_env_setup:

pw_env_setup
------------
Changes:

.. 537825f5f67281aee3764444de234e81722a2401

* All transitive Python dependencies are now pinned. Commit: `Pin all
  transitive Python package dependencies <https://pwrev.dev/261413>`__.
  Bug: :bug:`390257072`.

.. _docs-changelog-2025-01-24-Modules-pw_interrupt_cortex_a:

pw_interrupt_cortex_a
---------------------
New features:

.. e61919ca93f922bc67fc6bd2f005c00f858cdcbd

* [Deprecated] pw_interrupt_cortex_a is a new ``pw_interrupt`` backend
  for Arm Cortex-A processors. Commit: `Add pw_interrupt backend for
  A-profile processors <https://pwrev.dev/261396>`__.

.. _docs-changelog-2025-01-24-Modules-pw_protobuf:

pw_protobuf
-----------
Changes:

.. a3cd0bc42a929df218bbc3ed96944cacf5cead7a

* The ``kMaxEncodedSizeBytes`` constant has been renamed to
  ``kMaxEncodedSizeBytesWithoutValues`` to reflect the fact that it
  sometimes doesn't represent a message's true maximum size. Commit:
  `Disambiguate maximum size constants <https://pwrev.dev/259012>`__. Bug:
  :bug:`379868242`.

.. _docs-changelog-2025-01-24-Modules-pw_rpc:

pw_rpc
------
Changes:

.. 0afdf903105c46e8572591de16b7fd2964ad0874

* Recent Java client call ID changes were reverted because they were
  causing RPC timeouts. Commit: `Revert Java client call ID changes
  <https://pwrev.dev/260892>`__.

.. _docs-changelog-2025-01-24-Modules-pw_sync:

pw_sync
-------
Changes:

.. 86cb968d79207a634c196bc1d289aa1cf25591c7

* Time-related methods previously in :cpp:class:`pw::sync::Borrowable`
  were moved to a new :cpp:class:`pw::sync::TimedBorrowable` class so that
  projects can use ``Borrowable`` without depending on :ref:`module-pw_chrono`.
  Commit: `Split TimedBorrowable from Borrowable
  <https://pwrev.dev/260313>`__.

.. _docs-changelog-2025-01-24-Modules-pw_system:

pw_system
---------
Changes:

.. f1cb7ec19fca8dd7332d31d0ae81309e68601514

* The ``//pw_system:config`` Bazel rule is now public to make it
  possible to reuse the same configurations when creating custom RPC
  servers or I/O backends outside of Pigweed. Commit: `Make config library
  public in Bazel <https://pwrev.dev/261693>`__.

.. _docs-changelog-2025-01-24-Modules-pw_tokenizer:

pw_tokenizer
------------
New features:

.. a90ad7872bd7f178d049264214ae3404c212fc4d

* The new :c:macro:`PW_NESTED_TOKEN_FMT` macro is a format specifier for
  doubly nested token arguments. Commit: `Create generic macro for a
  nested token format <https://pwrev.dev/253267>`__.

Changes:

.. 1b8d5de1f136c9c46d62869554d1cf9672b8a815

* Token domains have been limited to certain characters. Commit: `Limit
  token domains to certain characters <https://pwrev.dev/253952>`__.

.. _docs-changelog-2025-01-24-Modules-pw_toolchain:

pw_toolchain
------------
New features:

.. 1a98c3d9725b4178148a354b917f15d86fc374b5

* ``pw_toolchain`` now supports Arm Cortex-A35. Commit: `Add toolchain
  config for Cortex-A35 <https://pwrev.dev/261733>`__.

.. 03e6941c72b91f2cb9550b1b1ba59fa9fe862ab2

* Upstream Pigweed toolchains now support
  :ref:`layering check <module-pw_toolchain-bazel-layering-check>`.
  Commit: `Document layering check <https://pwrev.dev/261552>`__. Bug:
  :bug:`219091175`.

.. _docs-changelog-2025-01-24-Modules-pw_thread:

pw_thread
---------
New features:

.. e9d4e4d30b4a49b2ca4dba88656dc660b0a0bcaf

* The cross-platform thread creation API proposed in :ref:`seed-0128`
  has been implemented. Commit: `Generic thread creation
  <https://pwrev.dev/255065>`__. Bug: :bug:`373524851`.

.. _docs-changelog-2025-01-09:

-----------
Jan 9, 2025
-----------
Highlights (Dec 27, 2024 to Jan 9, 2025):

* **Bazel 8**: Pigweed now :ref:`depends on Bazel 8
  <docs-changelog-2025-01-09-Build-systems-Bazel>`.

* **FuzzTest and CMake**: FuzzTest is now :ref:`supported in
  CMake projects <docs-changelog-2025-01-09-Modules-pw_fuzzer>`.

* **BoringSSL**: ``pw_crypto`` now :ref:`supports
  BoringSLL <docs-changelog-2025-01-09-Modules-pw_crypto>`.

* **pw_rpc Java improvements**: The ``pw_rpc`` Java client now
  has better :ref:`concurrent RPC request support
  <docs-changelog-2025-01-09-Modules-pw_rpc>`.

.. _docs-changelog-2025-01-09-Modules:

Modules
=======

.. _docs-changelog-2025-01-09-Modules-pw_containers:

pw_containers
-------------
New features:

.. 68e18edf9f1b3913c73b1a4332bbce6521609916

* Intrusive lists now support move operations. Commit: `Support moving
  intrusive lists <https://pwrev.dev/255894>`__.

.. _docs-changelog-2025-01-09-Modules-pw_chrono:

pw_chrono
---------
New features:

.. e88f3c4397c5cafafda8635ac93d42483059a9f9

* ``pw_chrono`` snapshots now support optional clock names. Commit: `Add
  support for clock names <https://pwrev.dev/253753>`__.

.. _docs-changelog-2025-01-09-Modules-pw_crypto:

pw_crypto
---------
New features:

.. 462b37b0820e284069b8e42f6e61438177b60cb7

* ``pw_crypto`` now supports :ref:`BoringSSL <module-pw_crypto-boringssl>`.
  Commit: `Add BoringSSL backend for AES <https://pwrev.dev/231914>`__.

Changes:

.. 483a24a69950b8be7ae924e28a7504150378c8ee

* ``micro_ecc`` support has been removed. Commit: `Remove micro_ecc
  support <https://pwrev.dev/229672>`__. Bug: :bug:`359924206`.

.. _docs-changelog-2025-01-09-Modules-pw_digital_io_mcuxpresso:

pw_digital_io_mcuxpresso
------------------------
New features:

.. 8a5fc59a80af2fb2b11c2cd16ecc21a210fe1065

* The new ``pw::digital_io::McuxpressoDigitalInOutInterrupt`` class
  supports interrupts on the GPIO interrupt block which enables using
  interrupts on more pins. Commit: `Add McuxpressoDigitalInOutInterrupt
  support <https://pwrev.dev/247972>`__.

Changes:

.. 34521ea9d72066a02d4b562eb6d2dd628e424e58

* ``pw::digital_io::McuxpressoDigitalInInterrupt`` has been deprecated.
  ``pw::digital_io::McuxpressoPintInterrupt`` should be used instead.
  Commit: `Introduce McuxpressoPintInterrupt
  <https://pwrev.dev/258994>`__. Bug: :bug:`337927184`.

.. _docs-changelog-2025-01-09-Modules-pw_fuzzer:

pw_fuzzer
---------
New features:

.. 736d6a39f5cdde223bfcbaf6f8c8fec7d512379a

* :ref:`FuzzTest <module-pw_fuzzer-guides-using_fuzztest>` can now be
  used in CMake projects. Commit: `Make FuzzTest usable be external CMake
  projects <https://pwrev.dev/239049>`__. Bug: :bug:`384978398`.

.. _docs-changelog-2025-01-09-Modules-pw_log_basic:

pw_log_basic
------------
Changes:

.. a542e417c4367b3b6c4fed88a172b9c2bdd2a837

* The maximum length for the function name field has increased from 20
  to 30 characters and is now left-aligned. Commit: `Adjust field widths
  <https://pwrev.dev/258174>`__.

.. _docs-changelog-2025-01-09-Modules-pw_protobuf:

pw_protobuf
-----------
Changes:

.. 8706efb9847543d226c5cb17494faffcd034cfa0

* Regular callbacks now ignore fields with unset decode callbacks.
  Previously they caused ``DATA_LOSS`` errors. Commit: `Allow unset
  callback fields in message structs <https://pwrev.dev/258392>`__.

.. _docs-changelog-2025-01-09-Modules-pw_rpc:

pw_rpc
------
New features:

.. 15d4ae5ff36cd452023fdfc07835f5f1635f05ef

* The Java client now supports making multiple concurrent RPC requests
  to the same method. Commit: `Increment call_ids for java client
  <https://pwrev.dev/258792>`__.

.. _docs-changelog-2025-01-09-Modules-pw_span:

pw_span
-------
New features:

.. 031bf132386f7350cb86338928368f46c5b76d1c

* The new docs section :ref:`module-pw_span-start-params` explains why
  ``pw::span`` objects should be passed by value. Commit: `Recommend passing
  pw::span objects by value <https://pwrev.dev/257072>`__. Bug:
  :bug:`387107922`.

.. _docs-changelog-2025-01-09-Modules-pw_stream:

pw_stream
---------
New features:

.. e7380e5da83527b80087e86b63224e052074d10f

* The new ``pw::stream::SocketStream::IsReady()`` method indicates
  whether the streaming socket connection is ready. Commit: `Add ready
  method to socket stream <https://pwrev.dev/253772>`__.

.. _docs-changelog-2025-01-09-Modules-pw_thread:

pw_thread
---------
Changes:

.. 717e4f58092e947eba23afb8be099d2ccf1247c3

* It is now simpler to disable the ``join()`` function when it's not
  supported. Commit: `Simplify disabling join() function when not
  supported <https://pwrev.dev/257913>`__.

.. _docs-changelog-2025-01-09-Modules-pw_toolchain:

pw_toolchain
------------
Changes:

.. 8adc4c3e84423554c5ad6549fefe3d0e035985c6

* The float ABI configuration for Arm Cortex-M33 was changed from
  ``soft`` to ``softfp``. Commit: `Update m33 float-abi
  <https://pwrev.dev/259412>`__. Bug: :bug:`388354690`.

.. _docs-changelog-2025-01-09-Modules-pw_toolchain_bazel:

pw_toolchain_bazel
------------------
Changes:

.. 413a81576837f344c55a8d64b5d1807769e513b0

* Most of ``pw_toolchain_bazel`` has been removed because it has been
  upstreamed to ``rules_cc``. Commit: `Remove contents
  <https://pwrev.dev/252472>`__. Bug: :bug:`346388161`.

.. _docs-changelog-2025-01-09-Modules-pw_uart:

pw_uart
-------
Changes:

.. 71c2d0d0e9eb7cc27111a20b3190af1593944676

* It is no longer safe to call any ``pw_uart`` read or write methods
  from a ``pw::uart::UartNonBlocking::DoRead()`` callback context. Commit:
  `Restrict UartNonBlocking::DoRead() callback
  <https://pwrev.dev/255732>`__. Bug: :bug:`384966926`.

.. _docs-changelog-2025-01-09-Modules-pw_unit_test:

pw_unit_test
------------
Bug fixes:

.. 203c6c8fd11fbdf01b51e564de03150f38dab430

* The buffer for expectation logs was increased in size to prevent
  expectation logs from getting cut off. Commit: `Expectation buffer
  cleanup <https://pwrev.dev/259055>`__. Bug: :bug:`387513166`.

.. _docs-changelog-2025-01-09-Build-systems:

Build systems
=============

.. _docs-changelog-2025-01-09-Build-systems-Bazel:

Bazel
-----
Changes:

.. b13f7bf334b239174e87d91fbfac8b8c1d209403

* Pigweed now depends on Bazel 8.0.0. Commit: `Update to Bazel 8.0.0
  <https://pwrev.dev/242033>`__. Bug: :bug:`372510795`.

.. _docs-changelog-2025-01-09-Docs:

Docs
====
New features:

.. b977a3aeb1f4cf5a1b113396bbf9f9af6c1f1658

* The :ref:`Sense tutorial <showcase-sense-tutorial-intro>` has been
  updated to cover all variations on the Raspberry Pi Pico. Commit:
  `Refresh Sense tutorial <https://pwrev.dev/254652>`__.

.. 9337bf2516b4e0876d5308ec15939cb2dc9e6ab8

* :ref:`Pigweed Toolchain <toolchain>` now has a homepage. Commit: `Add
  toolchain homepage <https://pwrev.dev/247593>`__. Bug: :bug:`373454866`.

.. _docs-changelog-2024-12-26:

------------
Dec 26, 2024
------------
Highlights (Dec 12, 2024 to Dec 26, 2024):

* **TLSF allocator**: ``pw_allocator`` has a new :ref:`two-layer, segregated
  fit allocator <docs-changelog-2024-12-26-Modules-pw_allocator>`.
* **Checked arithmetic**: ``pw_numeric`` has :ref:`a suite of new arithmetic
  operations <docs-changelog-2024-12-26-Modules-pw_numeric>` that check for
  overflows.
* **Constant expression unit tests**: ``pw_unit_test`` has a
  :ref:`new constexpr unit test <docs-changelog-2024-12-26-Modules-pw_unit_test>`
  that runs at both compile-time and runtime.
* **Bazel module integration guidance**: :ref:`docs-bazel-integration` now provides
  guidance on how to integrate Pigweed into projects that use Bazel modules.

.. _docs-changelog-2024-12-26-Modules:

Modules
=======

.. _docs-changelog-2024-12-26-Modules-pw_allocator:

pw_allocator
------------
New features:

.. f674d68203b26bf0207ab79099696e2b3b3cd9b3

* The new :cpp:class:`pw::allocator::TlsfAllocator` is a two-layer,
  segregated fit allocator. Its 2D array of buckets incurs overhead but it
  can satisfy requests quickly and has much better fragmentation performance
  than ``WorstFitAllocator``. Commit: `Add TLSF allocator
  <https://pwrev.dev/234818>`__.

.. _docs-changelog-2024-12-26-Modules-pw_build:

pw_build
--------
New features:

.. ee6f9976ba7e78737a1a74ae05f06da397b896c8

* The new ``pw_rust_crates_extension`` Bazel extension lets a project
  override the ``rust_crates`` repo when needed without requiring every
  project to define one. Commit: `Add pw_rust_crates_extension
  <https://pwrev.dev/254952>`__. Bug: :bug:`384536812`.

.. _docs-changelog-2024-12-26-Modules-pw_containers:

pw_containers
-------------
New features:

.. 999adb191f8f512b524f0a65bd0662ac7854ef20

* Queues and dequeues now have explicit ``constexpr`` constructors.
  Commit: `Add explicit constexpr constructors for deques/queues
  <https://pwrev.dev/250434>`__.

.. _docs-changelog-2024-12-26-Modules-pw_multibuf:

pw_multibuf
-----------
Changes:

.. b439dd3e602e6c3568ff2ed68a12c2ecf90391a9

* :cpp:class:`pw::multibuf::MultiBufAllocator` no longer supports async.
  :cpp:class:`pw::multibuf::MultiBufAllocatorAsync` should be used
  instead. Commit: `Move async to new MultiBufAllocatorAsync
  <https://pwrev.dev/255015>`__. Bug: :bug:`384583239`.

.. _docs-changelog-2024-12-26-Modules-pw_numeric:

pw_numeric
----------
New features:

.. d6827c16644efa06f524a702903397e3cc07ba4c

.. TODO: https://pwbug.dev/389134105 - Fix these links.

* :cpp:type:`pw::CheckedAdd`, :cpp:type:`pw::CheckedIncrement`,
  :cpp:type:`pw::CheckedSub`, :cpp:type:`pw::CheckedDecrement`, and
  :cpp:type:`pw::CheckedMul` are new arithmetic methods that check for
  overflows. Commit: `Add checked_arithmetic.h
  <https://pwrev.dev/253172>`__. Bug: :bug:`382262919`.

.. _docs-changelog-2024-12-26-Modules-pw_thread:

pw_thread
---------
New features:

.. 4f536c6c1137a7282e7f800aa636c79c5c629191

* The new :cpp:type:`pw::ThreadPriority` class is a generic priority
  class that can be used by any ``pw_thread`` backend. Commit: `Thread
  priority class <https://pwrev.dev/242214>`__.

.. _docs-changelog-2024-12-26-Modules-pw_toolchain:

pw_toolchain
------------
New features:

.. 13446e506fb94bcf4345bbf74b7987cff0fe0e3a

* ``pw_toolchain`` now supports Arm Cortex-M3. Commit: `Add Cortex M3
  support <https://pwrev.dev/254474>`__.

.. _docs-changelog-2024-12-26-Modules-pw_unit_test:

pw_unit_test
------------
New features:

.. c13d91eae61a50dfbe08023983337086509e7a8e

* :c:macro:`PW_CONSTEXPR_TEST` is a new unit test that is executed both
  at compile-time in a ``static_assert()`` and at runtime as a GoogleTest
  ``TEST()``. Commit: `Test framework for constexpr unit tests
  <https://pwrev.dev/242213>`__.

.. _docs-changelog-2024-12-26-Docs:

Docs
====
New features:

.. 42962f4a909048bb7f9464af018515bb5c9ee94c

* :ref:`docs-bazel-integration` has been updated to describe how to
  integrate Pigweed into projects that use Bazel modules (``bzlmod``). Commit:
  `Bazel integration: bzlmod, Bazel versions
  <https://pwrev.dev/254413>`__.

.. _docs-changelog-2024-12-11:

------------
Dec 11, 2024
------------
Highlights (Nov 28, 2024 to Dec 11, 2024):

* **New blog post**: :ref:`docs-blog-06-better-cpp-toolchains`
  summarizes our journey to upstream modular toolchains in rules_cc.

* **Customizable enum tokenization**: The new
  :c:macro:`PW_TOKENIZE_ENUM_CUSTOM` macro lets you customize how enum values
  are tokenized.

* **AES API in pw_crypto**: :ref:`module-pw_crypto` now has an
  Mbed-TLS backend for AES.

.. _docs-changelog-2024-12-11-Modules:

Modules
=======

.. _docs-changelog-2024-12-11-Modules-pw_allocator:

pw_allocator
------------
Bug fixes:

.. b3d4f6ec721999b6bd4a856386a16c0b102d4f3c

* A bug was fixed where the ``FirstFitAllocator`` incorrectly allocated
  from the front. Commit: `Fix first-fit with threshold
  <https://pwrev.dev/253233>`__. Bug: :bug:`382513957`.

.. _docs-changelog-2024-12-11-Modules-pw_assert:

pw_assert
---------
Changes:

.. bb9f65d47f203a2623543f5adf20ef15d747524e

* Error messages in constant expressions have been improved. Commit:
  `Improve error messages in constant expressions
  <https://pwrev.dev/251914>`__. Bug: :bug:`277821237`.

.. _docs-changelog-2024-12-11-Modules-pw_async2:

pw_async2
---------
New features:

.. 4a1d9b2c3f76002ad7462e2a4373fa3eedfda692

* :cpp:class:`pw::async2::OnceReceiver` can now be constructed with a
  value. Commit: `Support value constructor for OnceReceiver
  <https://pwrev.dev/251452>`__.

.. _docs-changelog-2024-12-11-Modules-pw_bluetooth_proxy:

pw_bluetooth_proxy
------------------
New features:

.. ffb532447804634ffa9a97f78886e8422025f9c2

* The new
  :cpp:func:`pw::bluetooth::proxy::ProxyHost::SendAdditionalRxCredits`
  method lets you send additional RX credits when needed. Previously this
  logic was coupled with the L2CAP connection-oriented channel
  acquisition. Commit: `Separate rx_additional_credits method
  <https://pwrev.dev/252352>`__. Bug: :bug:`380076024`.

.. b336566abf4d6d1119f5373dc534e1f1e1070523

* Transport type for ``pw::bluetooth::proxy::BasicL2capChannel`` can now
  be specified during channel creation. Commit: `Un-hardcode transport
  type for BasicL2capChannel <https://pwrev.dev/252556>`__.

.. e85884825ea35e9b3b6f86ab0d12d1a1c5e3518d

* A flow control mechanism for writes was added. When a channel's write
  fails because there's no space, and then space becomes available, the
  channel is now notified. Commit: `Add write flow control mechanism
  <https://pwrev.dev/251435>`__. Bug: :bug:`380299794`.

.. 99944ede9942532e5a12996575497ecb31a10442

* Proxies now reset after receiving ``HCI_Reset`` commands. Commit:
  `Reset proxy on HCI_Reset <https://pwrev.dev/251472>`__. Bug:
  :bug:`381902130`.

.. 2bfeaec98417551db59ec6dcc718f526a15d193f

* The new :cpp:class:`pw::bluetooth::proxy::L2capStatusDelegate` class
  lets you receive connection/disconnection notifications for a particular
  L2CAP service. Commit: `Add L2cap service listener API
  <https://pwrev.dev/249754>`__. Bug: :bug:`379558046`.

.. _docs-changelog-2024-12-11-Modules-pw_build:

pw_build
--------
New features:

.. b8d1cd064d4440e216f62802ab2378eb0c3eb269

* New guidance on :ref:`module-pw_build-bazel-pw_cc_binary` was added.
  Commit: `Document pw_cc_binary <https://pwrev.dev/252052>`__.

Changes:

.. 2ce8bc27e467224bf503fada12a29d8a4a2b81d7

* When a ``pw_python_venv`` targets has no source packages and no
  requirements, an empty Python venv is created and ``pip-compile`` is no
  longer used. Commit: `Allow for empty Python build venvs
  <https://pwrev.dev/253253>`__. Bug: :bug:`380293856`.

.. _docs-changelog-2024-12-11-Modules-pw_cli:

pw_cli
------
New features:

.. 13c7f3ca42864136624da1d8452b0110d61b850e

* Specifying whether color should be enabled on an output is now more
  granular. Commit: `Allow output-specific color checks
  <https://pwrev.dev/252292>`__.

Changes:

.. 73b5fbf1ba77139796d29b7a064ec3c76127bae4

* The Python function ``pw_presubmit.tools.exclude_paths()`` was moved
  to the ``pw_cli.file_filter`` module. Commit: `Move exclude_paths
  <https://pwrev.dev/252293>`__.

.. _docs-changelog-2024-12-11-Modules-pw_containers:

pw_containers
-------------
Bug fixes:

.. a7c4dd8b045259a12194742793eaa03aa9e07443

* A bug was fixed where :cpp:class:`pw::Vector` was move-assigning to
  destroyed objects. Commit: `Do not move assign to destroyed objects in
  Vector::erase <https://pwrev.dev/251992>`__. Bug: :bug:`381942905`.

.. _docs-changelog-2024-12-11-Modules-pw_crypto:

pw_crypto
---------
New features:

.. 9aceb7c03cab42e1d2ae7c4e2dd9de718fbe3c68

* :ref:`module-pw_crypto` now has an Mbed-TLS backend for AES. Commit:
  `Implement Mbed-TLS backend for AES <https://pwrev.dev/231912>`__.

.. 23cc90c00be0e6f8c3eef0adfe1a0906fa77b346

* The new :cpp:func:`pw::crypto::aes::backend::DoEncryptBlock` is an
  initial facade for AES. Commit: `Add AES facade
  <https://pwrev.dev/231911>`__.

.. _docs-changelog-2024-12-11-Modules-pw_grpc:

pw_grpc
-------
New features:

.. d30c2bbc46ccb4e874da27c3b33dc48e30cdc893

* Data is now queued if a stream or connection has no available send
  window. Previously data was dropped in this case. The send queues are
  non-blocking. Commit: `Implement per stream send queues and make sending
  non-blocking <https://pwrev.dev/249952>`__. Bug: :bug:`382294674`.

Bug fixes:

.. 983b4f196331b7873c59f555a39ed519cb707943

* A :cpp:class:`pw::multibuf::MultiBufAllocator` is now required when
  creating a :cpp:class:`pw::grpc::Connection` instance. Commit: `Remove
  old constructor and make multibuf allocator required
  <https://pwrev.dev/252555>`__. Bug: :bug:`382294674`.

.. _docs-changelog-2024-12-11-Modules-pw_presubmit:

pw_presubmit
------------
New features:

.. 9720483d67050eeda1910c707195aca45879ff90

* A new guide for Pigweed contributors on :ref:`managing the Bazel
  lockfile <docs-bazel-lockfile>` was published. Commit: `Bazel lockfile
  check <https://pwrev.dev/253554>`__. Bug: :bug:`383387420`.

.. _docs-changelog-2024-12-11-Modules-pw_rpc:

pw_rpc
------
Changes:

.. eb762cabb91d3712fe2ca6a470172f2164247072

* The newly public ``internal_packet_proto`` library makes it possible
  to generate ``packet.proto`` code for non-supported languages. E.g. this
  makes it possible to write a Dart/Flutter RPC client. Commit: `Make
  packet proto library public in Bazel <https://pwrev.dev/249692>`__.

.. _docs-changelog-2024-12-11-Modules-pw_snapshot:

pw_snapshot
-----------
New features:

.. b950987d209c57b18389aaca91d9a3a9ef7066f5

* The new ``thread_processing_callback`` parameter of
  :py:func:`pw_snapshot.processor.process_snapshot` lets you do custom
  thread processing during snapshot decoding. Commit: `Add per-thread
  processing callback for snapshot decoding <https://pwrev.dev/251392>`__.

.. _docs-changelog-2024-12-11-Modules-pw_system:

pw_system
---------
New features:

.. a4d795f13b833950d14ff9803d43e49ec1fb5e47

* In the Bazel build ``rpc_server`` is now a
  :ref:`module-pw_build-bazel-pw_facade` which lets you swap out the default
  HDLC server with something else in your project. Commit: `Add facade for rpc
  server in Bazel <https://pwrev.dev/252172>`__.

.. _docs-changelog-2024-12-11-Modules-pw_thread:

pw_thread
---------
Changes:

.. 9e4c976345d4d2de321e17853e9fb86493ff2200

* ``ThreadCore`` logic was moved out of ``pw::Thread``. Commit: `Move
  ThreadCore logic out of pw::Thread <https://pwrev.dev/253264>`__.

.. _docs-changelog-2024-12-11-Modules-pw_tokenizer:

pw_tokenizer
------------
New features:

.. 23370ed5ed0e01273080c231ee3475916dc74fba

* The new :c:macro:`PW_TOKENIZE_ENUM_CUSTOM` macro lets you customize
  how enum values are tokenized. Commit: `Add macro for tokenizing enums
  with custom string <https://pwrev.dev/250492>`__.

.. _docs-changelog-2024-12-11-Modules-pw_toolchain:

pw_toolchain
------------
New features:

.. 807a3aa14a2035f3c1693e3c023740d086ce7c94

* In the GN build it is now possible to completely move away from GNU
  libraries. Commit: `Support replacing GNU libs for ARM
  <https://pwrev.dev/250572>`__. Bug: :bug:`322360978`.

.. _docs-changelog-2024-12-11-Docs:

Docs
====
New features:

.. 4a28597bf8cc64f7cfc4cc019ba969bc10747c2b

* A new :ref:`Bazel style guide <docs-pw-style-bazel>` was added.
  Commit: `Add Bazel style guide <https://pwrev.dev/240811>`__. Bug:
  :bug:`371564331`.

.. 5a62fe471eab617742ded0ba84c693e4f2d29585

* A new blog post on :ref:`C/C++ Bazel toolchains
  <docs-blog-06-better-cpp-toolchains>` was published.
  Commit: `Shaping a better future for Bazel C/C++ toolchains
  <https://pwrev.dev/253332>`__.

.. _docs-changelog-2024-11-27:

------------
Nov 27, 2024
------------
Highlights (Nov 15, 2024 to Nov 27, 2024):

* **pw_allocator updates**: The :ref:`module-pw_allocator-api-bucket` class
  has been refactored to be more flexible and the :ref:`block API
  <module-pw_allocator-api-block>` has been refactored to support static
  polymorphism.

.. _docs-changelog-2024-11-27-Modules:

Modules
=======

.. _docs-changelog-2024-11-27-Modules-pw_allocator:

pw_allocator
------------
Changes:

.. 0942b69025f2987ca9512141fe3cf3a7046f042c

* ``BestFitBlockAllocator`` was renamed to ``BestFitAllocator`` and
  ``WorstFitBlockAllocator`` was renamed to ``WorstFitAllocator``. These
  classes have been refactored to use :ref:`buckets
  <module-pw_allocator-api-bucket>`. Commit: `Refactor best- and worst-fit
  allocators to use buckets <https://pwrev.dev/234817>`__.

.. 0766dbaf5305202c6f67e18184bb165df8426713

* ``FirstFitBlockAllocator``, ``LastFitBlockAllocator``, and
  ``DualFirstFitBlockAllocator`` were merged into a single class:
  :ref:`module-pw_allocator-api-first_fit_allocator`. Commit: `Refactor
  first fit allocators <https://pwrev.dev/234816>`__.

.. 3bfdac7a7826a4e2a1dc7a7174fd8d6276546c26

* The :ref:`module-pw_allocator-api-bucket` class has been refactored to
  be more flexible. Commit: `Refactor Bucket
  <https://pwrev.dev/234815>`__.

.. 65b5e336df018fbc9d124ca09d71a5dcccd1a8c0

* Metric calculation for blocks that shift bytes has changed. Commit:
  `Fix metrics for blocks that shift bytes <https://pwrev.dev/249372>`__.
  Bug: :bug:`378743727`.

.. 33d00a77472a6ff545032b213aa24dfe6a39d606

* :cpp:class:`pw::allocator::BlockAllocator` now returns
  :cpp:class:`pw::allocator::BlockResult`. Commit: `Use BlockResult in
  BlockAllocator <https://pwrev.dev/234811>`__.

.. 6417a523b06e03dce3453e96c3a1bec6ab511768

* The :ref:`block API <module-pw_allocator-api-block>` has been
  refactored to support static polymorphism. Commit: `Add static
  polymorphism to Block <https://pwrev.dev/232214>`__.

.. _docs-changelog-2024-11-27-Modules-pw_bytes:

pw_bytes
--------
New features:

.. a287811e5e99eab8d4ddfcaf9f1a505fd1e3eb17

* The new :cpp:func:`pw::IsAlignedAs` utility functions make it easier
  to check alignment. Commit: `Add utility for checking alignment
  <https://pwrev.dev/248192>`__.

.. _docs-changelog-2024-11-27-Modules-pw_presubmit:

pw_presubmit
------------
New features:

.. e278ead3bfe1361c3ff08e5329636a35abbcef6c

* The new ``includes_presubmit_check`` verifies that ``cc_library``
  Bazel targets don't use the ``includes`` attribute. Commit: `Add check
  for cc_library includes <https://pwrev.dev/251172>`__. Bug:
  :bug:`380934893`.

.. _docs-changelog-2024-11-27-Modules-pw_protobuf:

pw_protobuf
-----------
New features:

.. f776679bbea5f4ae376ab924d80760bb2f1e69a0

* :ref:`pw_protobuf-message-limitations` now has more guidance around
  protobuf versioning and ``optional`` fields. Commit: `Expand message
  structure limitations docs section <https://pwrev.dev/249072>`__.

.. _docs-changelog-2024-11-27-Modules-pw_transfer:

pw_transfer
-----------
Changes:

.. 1c771e0fd88511ef3550108572572db1f036d0a5

* Warnings logs are now emitted when client or server streams close
  unexpectedly. Commit: `Log when streams close unexpectedly
  <https://pwrev.dev/249912>`__.

.. 48712ad0655654b4dcc9b62085a58445fe0af696

* The window size on retried data now shrinks in an attempt to reduce
  network congestion. Commit: `Shrink window size on retried data
  <https://pwrev.dev/249532>`__.

.. _docs-changelog-2024-11-27-Modules-pw_unit_test:

pw_unit_test
------------
Changes:

.. c4d59ce4d011e11781bcb6dc6660ad947a7ee8df

* Successful expectations are no longer stringified by default. Commit:
  `Stop stringifying successful expectations
  <https://pwrev.dev/248693>`__.

.. _docs-changelog-2024-11-27-Modules-pw_rpc:

pw_rpc
------
Changes:

.. 92e854a4d179f0b340a0f8b5a662012ea4b8635c

* A warning log is now emitted when a server receives a completion
  request but client completion callbacks have been disabled. Commit:
  `Warn when client completion callback is disabled
  <https://pwrev.dev/249414>`__.

.. _docs-changelog-2024-11-14:

------------
Nov 14, 2024
------------
.. changelog_highlights_start

Highlights (Nov 1, 2024 to Nov 14, 2024):

* **ELF API**: The new :cpp:class:`pw::elf::ElfReader` class is a
  basic reader for ELF files.
* **Updated Bluetooth APIs**: There's a new low energy
  connection-oriented channels API and the :ref:`module-pw_bluetooth`
  API has been modernized.
* **Updated SEED process**: "Intent Approved" and "On Hold" statuses
  were added to the SEED lifecycle.

.. changelog_highlights_end

.. _docs-changelog-2024-11-14-Modules:

Modules
=======

.. _docs-changelog-2024-11-14-Modules-pw_allocator:

pw_allocator
------------
Changes:

.. 1dcac6a863b8adfc930de769ec56fd44f1e4448f

* ``pw::allocator::AsPmrAllocator`` was renamed to
  :cpp:class:`pw::allocator::PmrAllocator`. Commit: `Separate PMR from
  Allocator <https://pwrev.dev/246412>`__.

.. _docs-changelog-2024-11-14-Modules-pw_async2:

pw_async2
---------
New features:

.. 21933c60384c25d73dba60c90e74cc44d2a446de

* The ``new`` operator for coroutines now accepts an optional alignment
  argument. Commit: `Accept alignment in CoroPromiseType::operator new
  <https://pwrev.dev/248638>`__. Bug: :bug:`378929156`.

.. d20009a8e35dfdb881f77b6d171c697b61dba5c3

* The new :c:macro:`PW_TRY_READY` and :c:macro:`PW_TRY_READY_ASSIGN`
  helper macros reduce boilerplate in non-coroutine async code. Commit:
  `Add PW_TRY_READY_* control flow macros <https://pwrev.dev/243818>`__.

.. _docs-changelog-2024-11-14-Modules-pw_bluetooth:

pw_bluetooth
------------
New features:

.. c9ad96cf7be8a05a7d1bf6ec933114c7cc39f012

* The new :cpp:class:`pw::bluetooth::low_energy::Channel`,
  :cpp:class:`pw::bluetooth::low_energy::ChannelListener`, and
  :cpp:class:`pw::bluetooth::low_energy::ChannelListenerRegistry` classes
  provide a low energy connection-oriented channels API. Commit: `Add LE
  Connection-Oriented Channels API <https://pwrev.dev/227371>`__. Bug:
  :bug:`357142749`.

Changes:

.. a615b8bf5234f48b8a33e6c837aa7521fa80d92a

* The :ref:`module-pw_bluetooth` API has been modernized. Commit:
  `Modernize APIs <https://pwrev.dev/219393>`__. Bug: :bug:`350994818`.

.. _docs-changelog-2024-11-14-Modules-pw_bluetooth_proxy:

pw_bluetooth_proxy
------------------
New features:

.. b8ee89e76ff4e8c467ea35291b2a141175c737e9

* :cpp:class:`pw::bluetooth::proxy::L2capCoc` now supports reading.
  Commit: `L2CAP CoC supports reading <https://pwrev.dev/232172>`__. Bug:
  :bug:`360934032`.

.. _docs-changelog-2024-11-14-Modules-pw_channel:

pw_channel
----------
New features:

.. 0a4e6db6ad35d0bf062c22b471fef2a66948a90f

* The new :cpp:func:`pw::Channel::PendAllocateWriteBuffer` method
  simplifies the allocation of write buffers that need small
  modifications. Commit: `Move to PendAllocateWriteBuffer
  <https://pwrev.dev/246239>`__.

Changes:

.. bd17ed6971f608e2d66da8b2954bc13ad3e2c664

* The ``pw::channel::WriteToken`` method was removed. Commit: `Remove
  WriteToken <https://pwrev.dev/245932>`__.

.. 0422de1761e52ad5d1fad821880025fc27facf1c

* ``pw_channel`` inheritance has been refactored
  to ensure that conversions between compatible variants are valid.
  Commit: `Rework inheritance to avoid SiblingCast
  <https://pwrev.dev/247732>`__.

.. _docs-changelog-2024-11-14-Modules-pw_elf:

pw_elf
------
New features:

.. 8ee78791500354f85df94b228f63b3a42a882040

* The new :cpp:class:`pw::elf::ElfReader` class is a basic reader for
  ELF files. Commit: `Add ElfReader <https://pwrev.dev/244893>`__.

.. _docs-changelog-2024-11-14-Modules-pw_i2c:

pw_i2c
------
Bug fixes:

.. 6165aa470b35106b79b2e57ecec9951cff30acdc

* I2C flags are now correctly set for transactions that occur on an I3C
  bus. Commit: `Set the i2c flags correctly for transactions on an i3c bus
  <https://pwrev.dev/245754>`__. Bug: :bug:`373451623`.

.. _docs-changelog-2024-11-14-Modules-pw_metric:

pw_metric
---------
New features:

.. a751fa9bd38a30f08a14c336e7bc77878a5d60c5

* ``pwpb`` now prioritizes the ``.pwpb_options`` for protobuf codegen.
  Commit: `Add metrics_service.pwpb_options to BUILD.gn
  <https://pwrev.dev/246112>`__.

.. aacf94efb59dac4a661e88e2ba95acda20c5ccb1

* The new :cpp:func:`PW_METRIC_TOKEN` makes it easier for tests to
  create tokens for metrics. Commit: `Expose metric token format via
  PW_METRIC_TOKEN <https://pwrev.dev/244332>`__.

.. _docs-changelog-2024-11-14-Modules-pw_protobuf:

pw_protobuf
-----------
New features:

.. acbeaab3d280f748d235a31025d7a35dd38fa516

* The ``Find*()`` methods have been extended to support iterating over
  repeated fields. See :ref:`module-pw_protobuf-read`. Commit:
  `Extend Find() APIs to support repeated fields <https://pwrev.dev/248432>`__.

Changes:

.. 6a16fab34795f2976e04457d099a05c671b79b54

* It's no longer necessary to set a callback for every possible field in
  a message when you're only interested in a few fields. Commit: `Allow
  unset oneof callbacks <https://pwrev.dev/246692>`__.

.. _docs-changelog-2024-11-14-Modules-pw_protobuf_compiler:

pw_protobuf_compiler
--------------------
Changes:

.. cd0b4fb52d8fb2012fbd8483f66a48d593e83bb9

* ``pwpb_options`` files are now explicitly processed first, followed by
  regular ``.options`` files. Commit: `Don't rely on options file ordering
  <https://pwrev.dev/247472>`__.

.. _docs-changelog-2024-11-14-Modules-pw_rpc:

pw_rpc
------
Changes:

.. f0ba9b05187506d7526ca3585401c8df7b4e9d4d

* The Python client previously reused call IDs after ``16384``. The limit
  has been increased to ``2097152``. Commit: `Allocate more call IDs in
  the Python client <https://pwrev.dev/245067>`__. Bug: :bug:`375658481`.

.. _docs-changelog-2024-11-14-Modules-pw_rpc_transport:

pw_rpc_transport
----------------
New features:

.. 18d5fbfb289c8675dd683d62ab69b83147e9d70e

* The new ``pw::rpc::RpcIngress::num_total_packets`` method tracks how
  many packets an ingress RPC handler has received. Commit: `Track number
  of RPC packets received <https://pwrev.dev/247194>`__. Bug:
  :bug:`373449543`.

.. _docs-changelog-2024-11-14-Modules-pw_sensor:

pw_sensor
---------
New features:

.. b7246bf88df6aa030dec7b9510e7015b5035616c

* The new ``extras`` key lets applications specify additional metadata
  that's not supported or used by Pigweed. Commit: `Add freeform extras
  field to sensor.yaml <https://pwrev.dev/248195>`__.

.. _docs-changelog-2024-11-14-Modules-pw_spi:

pw_spi
------
Changes:

.. 6aadd54afb308c09edb833985b387442df732fef

* :cpp:class:`pw::spi::Initiator` is now a `non-virtual interface
  <https://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Non-Virtual_Interface>`__.
  Commit: `Finalize non-Virtual interface on
  Initiator <https://pwrev.dev/236236>`__. Bug: :bug:`308479791`.

.. _docs-changelog-2024-11-14-Modules-pw_spi_linux:

pw_spi_linux
------------
Bug fixes:

.. d17b5acd42c6e983ce4b12a7ebc3184205a06d45

* The ``pw::Spi::Initiator::DoWriteRead()`` method now correctly handles
  transfers where either the write buffer or the read buffer is empty.
  Commit: `Fix read/write-only transfers <https://pwrev.dev/246053>`__.

.. _docs-changelog-2024-11-14-Modules-pw_stream:

pw_stream
---------
New features:

.. b2c1616caaff13d41e434cbcb8ae0530a20962ca

* The new :cpp:func:`pw::stream::Stream::ReadExact` method reads exactly
  the number of bytes requested into a provided buffer, if supported.
  Commit: `Add ReadExact() wrapper <https://pwrev.dev/243552>`__.

.. _docs-changelog-2024-11-14-Modules-pw_tokenizer:

pw_tokenizer
------------
New features:

.. 94e2314ddea1499a7868bd3d30621b709e25f7d4

* The new :cpp:func:`pw::tokenizer::Detokenizer::FromElfFile` method
  constructs a detokenizer from the ``.pw_tokenizer.entries`` section of
  an ELF binary. Commit: `Add Detokenizer::FromElfFile
  <https://pwrev.dev/243532>`__. Bug: :bug:`374367947`.

Bug fixes:

.. 7fb7bb1dc5d69c79de1b0b72316a34a800f2d5b0

* The Python detokenizer now correctly handles multiple nested tokens in
  one log string. Commit: `Update regex for nested args
  <https://pwrev.dev/248232>`__.

.. _docs-changelog-2024-11-14-SEEDs:

SEEDs
=====
New features:

.. a1acd00d02ebe42116a4982ad809264f3c673021

* "Intent Approved" and "On Hold" statuses were added to the SEED
  lifecycle. Commit: `(SEED-0001) Add "Intent Approved" and "On Hold"
  states to lifecycle <https://pwrev.dev/248692>`__.

.. _docs-changelog-2024-10-31:

------------
Oct 31, 2024
------------
Highlights (Oct 18, 2024 to Oct 31, 2024):

* The new :c:macro:`PW_TEST_EXPECT_OK`, :c:macro:`PW_TEST_ASSERT_OK`,
  and :c:macro:`PW_TEST_ASSERT_OK_AND_ASSIGN` macros provide test matchers
  for :ref:`module-pw_status` and :ref:`module-pw_result` values.
* The Sense tutorial has expanded guidance around :ref:`handling crashes
  and viewing snapshots <showcase-sense-tutorial-crash-handler>`.
* The new :cpp:class:`pw::LinkerSymbol` class represents a symbol
  provided by a linker.
* The new ``extra_frame_handlers`` parameter lets you add custom HDLC frame
  handlers when using the Python-based :ref:`module-pw_system` console.

.. _docs-changelog-2024-10-31-Modules:

Modules
=======

.. _docs-changelog-2024-10-31-Modules-pw_allocator:

pw_allocator
------------
New features:

.. 6fd4792308999b9c5949e9b778994d064a80a8b7

* The new :c:macro:`PW_ALLOCATOR_ENABLE_PMR` macro lets you disable the
  ability to use an allocator with the polymorphic versions of standard
  library containers. Commit: `Make pw::Allocator::as_pmr optional
  <https://pwrev.dev/245254>`__.

Changes:

.. c3c61885927ee9e8fac187a52b1ab139919fe5e7

* Multiple methods were renamed or removed in preparation for upcoming
  work to split up :cpp:class:`pw::allocator::Block`. Commit: `Streamline
  Block API <https://pwrev.dev/232213>`__.

.. _docs-changelog-2024-10-31-Modules-pw_assert:

pw_assert
---------
Changes:

.. 673e56ac476a345a0f25319633d8ce0ad0c0cd93

* ``pw_assert`` now verifies whether ``PW_CHECK`` message arguments are
  valid, regardless of what ``pw_assert`` backend is being used. Commit:
  `Verify PW_CHECK message arguments in the API
  <https://pwrev.dev/244744>`__.

.. _docs-changelog-2024-10-31-Modules-pw_assert_tokenized:

pw_assert_tokenized
-------------------
New features:

.. ef5f6bf505343c0926532f53dc788b26c3640048

* :c:macro:`pw_assert_HandleFailure` is now supported. Commit: `Support
  pw_assert_HandleFailure <https://pwrev.dev/244793>`__.

.. _docs-changelog-2024-10-31-Modules-pw_async2:

pw_async2
---------
New features:

.. cec451dc5c2cb132717068d686e4e034b06762c9

* :cpp:class:`pw::async2::Join` lets you join several separate pendable
  values. Commit: `Add Join combinator <https://pwrev.dev/244612>`__.

.. 0945ffb46234253d08467adebd8869ebb39234fa

* The new :cpp:func:`pw::async2::TimeFuture::Reset` method resets a
  ``TimeFuture`` instance to expire at a specified time. Commit: `Add
  TimerFuture::Reset <https://pwrev.dev/243993>`__.

Changes:

.. 9528eac2b6c6243dc5fb6a4d72f33d8d7e0d573c

* Waker storage has moved to a macro-based API. See
  :c:macro:`PW_ASYNC_STORE_WAKER` and :c:macro:`PW_ASYNC_CLONE_WAKER`.
  Commit: `Move to macro-based Waker API <https://pwrev.dev/245068>`__.
  Bug: :bug:`376123061`.

.. _docs-changelog-2024-10-31-Modules-pw_bluetooth_proxy:

pw_bluetooth_proxy
------------------
New features:

.. a8c756860ae104ef5e87439ef4f8cc4fbbb2fab7

* Bazel builds are now supported. Commit: `Add bazel build support
  <https://pwrev.dev/243874>`__.

.. _docs-changelog-2024-10-31-Modules-pw_build:

pw_build
--------
New features:

.. 170f745d98d101d73564fa61420f1a0836053033

* The new :cpp:class:`pw::LinkerSymbol` class represents a symbol
  provided by a linker. Commit: `Add LinkerSymbol
  <https://pwrev.dev/242635>`__.

.. _docs-changelog-2024-10-31-Modules-pw_channel:

pw_channel
----------
Changes:

.. 5f2649a6e5afdecd7b5d59aab06f59bb4d73d9f3

* ``pw::channel::AnyChannel::Write`` was renamed to
  :cpp:func:`pw::channel::AnyChannel::StageWrite` and
  ``pw::channel::AnyChannel::PendFlush`` was renamed to
  :cpp:func:`pw::channel::AnyChannel::PendWrite`. Commit: `Rename
  {Write->StageWrite, PendFlush->PendWrite} <https://pwrev.dev/245539>`__.

.. _docs-changelog-2024-10-31-Modules-pw_containers:

pw_containers
-------------
New features:

.. afb18a6ded0c43e91ce85e5ec97ae0206c4873ca

* :cpp:class:`pw::IntrusiveSet` is a new class like ``std::set<Key,
  Compare>`` that uses intrusive items as keys and
  :cpp:class:`pw::IntrusiveMultiSet` is a new class like
  ``std::multiset<Key, Compare>`` that uses intrusive items. Commit: `Add
  IntrusiveSet and IntrusiveMultiSet <https://pwrev.dev/240053>`__.

Changes:

.. bde3f80e6e038cebe264283af625faf06a64d8d0

* The ``erase`` methods of
  :cpp:class:`pw::containers::future::IntrusiveList`,
  :cpp:class:`pw::IntrusiveMap`, and :cpp:class:`pw::IntrusiveMultiMap`
  have been overloaded to make them easier to use. Commit: `Add methods to
  erase by item <https://pwrev.dev/243257>`__.

.. _docs-changelog-2024-10-31-Modules-pw_log:

pw_log
------
Changes:

.. 661bf47f06e604384a4c7eb7bf79d2f3da71ee74

* The signature for :c:macro:`PW_LOG` macro changed. A ``verbosity``
  level must now always be passed as the second argument when invoking
  ``PW_LOG``. Commit: `Explicitly pass verbosity to PW_LOG
  <https://pwrev.dev/239035>`__.

.. _docs-changelog-2024-10-31-Modules-pw_multibuf:

pw_multibuf
-----------
New features:

.. 9ab6e749821eddb8a82f7724c1af10ef6accc016

* The new :cpp:func:`pw::multibuf::FromSpan` function creates a multibuf from
  an existing span and a ``deleter`` callback. Commit: `Add FromSpan
  <https://pwrev.dev/245132>`__. Bug: :bug:`373725545`.

.. _docs-changelog-2024-10-31-Modules-pw_multisink:

pw_multisink
------------
New features:

.. 773331a904413e92d86c6e4cb658af77cf889d76

* The new :cpp:func:`pw::multisink::UnsafeDumpMultiSinkLogsFromEnd`
  utilitiy function dumps contents as a series of log entries. Commit:
  `Add UnsafeForEachEntryFromEnd() <https://pwrev.dev/244556>`__. Bug:
  :bug:`375653884`.

.. _docs-changelog-2024-10-31-Modules-pw_polyfill:

pw_polyfill
-----------
Changes:

.. d3e10fad55171d8cdce399916acbf37b2dec732b

* :c:macro:`PW_CONSTINIT` now fails when used without compiler support.
  Commit: `Make PW_CONSTINIT support mandatory
  <https://pwrev.dev/243892>`__.

.. _docs-changelog-2024-10-31-Modules-pw_protobuf:

pw_protobuf
-----------
Bug fixes:

.. 368cf8be3cb1909453c9c2cc67ec951517943086

* ``pw_protobuf`` now fails when the ``max_count`` or ``fixed_count``
  options of ``pwpb`` are used on unsupported field types. Commit: `Fail
  when a max count is set with an unsupported type
  <https://pwrev.dev/236816>`__.

Changes:

.. 9525d75843cede1c67f78de7c2f86bb30ac98efe

* Code generator options can now be specified in files ending with
  ``.pwpb_options``. This is useful for projects that wish to strictly
  separate Nanopb and ``pw_protobuf`` options. Commit: `Allow
  .pwpb_options as an options file extension
  <https://pwrev.dev/241137>`__.

.. _docs-changelog-2024-10-31-Modules-pw_ring_buffer:

pw_ring_buffer
--------------
New features:

.. b90180561658453318c36b125fd8af37de713a3f

* ``pw_ring_buffer`` readers now support the ``--`` decrement operator.
  Commit: `Add a decrement operator <https://pwrev.dev/244555>`__. Bug:
  :bug:`375653884`.

.. _docs-changelog-2024-10-31-Modules-pw_system:

pw_system
---------
New features:

.. 1f2341cd64eaacb9de474510293397b50165b3b5

* The new ``extra_frame_handlers`` parameter lets you add custom HDLC
  frame handlers when using the Python-based ``pw_system`` console.
  Commit: `Make console support extra hdlc frame handlers
  <https://pwrev.dev/245192>`__.

Bug fixes:

.. 9b2dd9ccb35a49f57c053e88a55da35ad24fc93d

* A bug was fixed where the latest logs were not being captured in crash
  snapshots. Commit: `Ensure latest logs are captured in crash snapshot
  <https://pwrev.dev/244557>`__. Bug: :bug:`375653884`.

.. 0aa57cea2e06c48a24266b21d472155c2379189f

* The ``pw_system`` crash dump now includes a main stack thread
  backtrace. Commit: `Add main stack thread backtrace capture to crash
  dump <https://pwrev.dev/242337>`__. Bug: :bug:`354767156`.

.. ed55dbc3fa5eb05e2b557fad0eff2b1bf1f93751

* The ``pw_system`` crash dump now includes FreeRTOS thread backtraces.
  Commit: `Add freertos thread backtrace capture to crash dump
  <https://pwrev.dev/234155>`__. Bug: :bug:`354767156`.

.. _docs-changelog-2024-10-31-Modules-pw_thread:

pw_thread
---------
Changes:

.. e5db91d7df18444134ce4fede3a1a0c3a9f5c1fc

* The legacy ``thread::Id`` alias has been migrated to
  ``pw::Thread::id``. Commit: `Migrate to Thread::id
  <https://pwrev.dev/238432>`__. Bug: :bug:`373524945`.

.. _docs-changelog-2024-10-31-Modules-pw_tokenizer:

pw_tokenizer
------------
New features:

.. 4b7733f3f0c9e010ccd6bee3ff45d64b92e65fa7

* :c:macro:`PW_APPLY` is a new general macro that supports macro
  expansion and makes tokenizing enums easier. Commit: `Create generic
  macro for tokenizing enums <https://pwrev.dev/242715>`__. Bug:
  :bug:`3627557773`.

.. _docs-changelog-2024-10-31-Modules-pw_toolchain:

pw_toolchain
------------
New features:

.. b40ecc98b47a37b187f40408ae0c3d89ef9b5f79

* ``pw_toolchain`` now supports the Arm Cortex-M55F GCC toolchain.
  Commit: `Add ARM Cortex-M55F GCC toolchain
  <https://pwrev.dev/244672>`__. Bug: :bug:`375562597`.

.. _docs-changelog-2024-10-31-Modules-pw_unit_test:

pw_unit_test
------------
New features:

.. ce0e3e2d1b7eec7cdf59fbb2ceed2b1cb3edd1ec

* The new :c:macro:`PW_TEST_EXPECT_OK`, :c:macro:`PW_TEST_ASSERT_OK`,
  and :c:macro:`PW_TEST_ASSERT_OK_AND_ASSIGN` macros provide test matchers
  for :ref:`module-pw_status` and :ref:`module-pw_result` values. Commit:
  `Define pw::Status matchers <https://pwrev.dev/243615>`__. Bugs:
  :bug:`338094795`, :bug:`315370328`.

Changes:

.. 618eaa4f4c1ee0357bdfba85290a9a6e8c9aee71

* :cpp:func:`RUN_ALL_TESTS` is now a function. Previously it was a
  macro. Commit: `Use a function for RUN_ALL_TESTS()
  <https://pwrev.dev/243889>`__.

.. _docs-changelog-2024-10-31-Modules-pw_watch:

pw_watch
--------
New features:

.. 5e7d1a0fddab1204d94ff43cee833431eda75af8

* ``pw_watch`` can now be invoked through ``bazelisk``. Commit: `Bazel
  run support <https://pwrev.dev/242094>`__. Bug: :bug:`360140397`.

.. _docs-changelog-2024-10-31-Docs:

Docs
====
New features:

.. c5e6cab3b190f4cdc32e3209582a7697f2992a85

* The Sense tutorial has expanded guidance around :ref:`handling crashes
  and viewing snapshots <showcase-sense-tutorial-crash-handler>`. Commit:
  `Add crash handler section to sense tutorial
  <https://pwrev.dev/242735>`__. Bug: :bug:`354767156`.

.. _docs-changelog-2024-10-31-Targets:

Targets
=======

.. _docs-changelog-2024-10-31-Targets-RP2350:

RP2350
------
New features:

.. 892394fe74db6decd3799873f910862cef6a182d

* ``MemManage``, ``BusFault``, and ``UsageFault`` exception handlers are
  now enabled on the RP2350 target. Commit: `Add MemManage, BusFault &
  UsageFault exception handler <https://pwrev.dev/242336>`__. Bug:
  :bug:`354767156`.

.. _docs-changelog-2024-10-17:

------------
Oct 17, 2024
------------
Highlights (Oct 04, 2024 to Oct 17, 2024):

* **Math module**: The new :ref:`module-pw_numeric` module is a collection of
  mathematical utilities optimized for embedded systems.
* **C++ Coroutines**: The new :ref:`docs-blog-05-coroutines` blog post
  discusses the nuances of using coroutines in embedded systems.
* **New SEEDs**: SEEDs :ref:`seed-0103` and :ref:`seed-0128` were accepted.

.. _docs-changelog-2024-10-17-Modules:

Modules
=======

.. _docs-changelog-2024-10-17-Modules-pw_allocator:

pw_allocator
------------
New features:

.. 9aae89c7bab1de5a914c462f814fec6528e27a0f

* The new :cpp:func:`pw::Allocator::MakeUniqueArray` template function
  allows a ``UniquePtr`` to hold an array of elements. Commit: `Add
  UniquePtr::MakeUniqueArray <https://pwrev.dev/239913>`__.

.. d3a6358972d5897266e2b5ecf50681a8e8456e5b

* The new :cpp:class:`pw::allocator::BlockResult` class communicates the
  results and side effects of allocation requests. Commit: `Add
  BlockResult <https://pwrev.dev/232212>`__.

Bug fixes:

.. 57183dee645126c67dcccbb479c730492ef168f6

* A data race was fixed. Commit: `Fix data race
  <https://pwrev.dev/242736>`__. Bug: :bug:`372446436`.

.. _docs-changelog-2024-10-17-Modules-pw_async2:

pw_async2
---------
New features:

.. 068949bbe9f8a5a03d9b44ae740461c4c01691ca

* The new :cpp:func:`pw::async2::EnqueueHeapFunc` function heap-allocates
  space for a function and enqueues it to run on a dispatcher.
  Commit: `Add EnqueueHeapFunc <https://pwrev.dev/242035>`__.

Changes:

.. eb03d32b80c25d59000d86fc8417cce91cbc243a

* :cpp:class:`pw::async2::PendFuncTask` now has a default template type
  of :cpp:type:`pw::Function`. Commit: `Provide default template type for
  PendFuncTask <https://pwrev.dev/242918>`__.

.. _docs-changelog-2024-10-17-Modules-pw_build:

pw_build
--------
New features:

.. 7698704f57a69ff5a913f0b2d43d3cc419d10446

* The newly relanded ``pw_copy_and_patch_file`` feature provides the
  ability to patch a file during a Bazel or GN build. Commit: `Add
  pw_copy_and_patch_file <https://pwrev.dev/240832>`__.

.. _docs-changelog-2024-10-17-Modules-pw_cli_analytics:

pw_cli_analytics
----------------
New features:

.. 6ae64ef2889810d43682b85e0c793018ae9a507c

* The new :ref:`module-pw_cli_analytics` module collects and transmits
  analytics on usage of the ``pw`` command line interface. Commit:
  `Initial commit <https://pwrev.dev/188432>`__. Bug: :bug:`319320838`.

.. _docs-changelog-2024-10-17-Modules-pw_console:

pw_console
----------
Bug fixes:

.. 8bd77aba07ab3dce5220b23994cd3ecfbcefda10

* A divide-by-zero error in the ``pw_console`` progress bar was fixed.
  Commit: `Fix progress bar division by zero
  <https://pwrev.dev/233033>`__.

.. _docs-changelog-2024-10-17-Modules-pw_env_setup:

pw_env_setup
------------
Changes:

.. a789e9c308f3b289c950e8afb3d891fa5b7b39ac

* ``//pw_env_setup/py/pw_env_setup/cipd_setup/black.json`` has been
  removed. Commit: `Remove black.json <https://pwrev.dev/241359>`__.

.. c42ec10b3824a5e15bc4e92d2065bd95143e9aad

* Python 2 support has been removed from ``pw_env_setup``. Commit: `Drop
  Python 2 support <https://pwrev.dev/242713>`__. Bug: :bug:`373905972`.

.. _docs-changelog-2024-10-17-Modules-pw_numeric:

pw_numeric
----------
New features:

.. 0c98e51f046d2de13e5ea8509452b99beb6776ec

* The new :ref:`module-pw_numeric` module is a collection of
  mathematical utilities optimized for embedded systems. Commit: `New
  module for mathematical utilities <https://pwrev.dev/240655>`__.

.. 1eadbb9e0d8de149ee300c9f60933878498b3544

* The new :cpp:func:`pw::IntegerDivisionRoundNearest` function performs
  integer division and rounds to the nearest integer. It gives the same
  result as ``std::round(static_cast<double>(dividend) /
  static_cast<double>(divisor))`` but requires no floating point
  operations and is ``constexpr``. Commit: `Rounded integer division
  <https://pwrev.dev/240656>`__.

.. _docs-changelog-2024-10-17-Modules-pw_protobuf:

pw_protobuf
-----------
Changes:

.. 205570386eac8fe6e0269b7fbbab1449eb565036

* ``oneof`` protobuf fields can't be inlined within a message structure.
  They must be encoded and decoded using callbacks. See
  :ref:`pw_protobuf-per-field-apis`. Commit: `Force use of callbacks for oneof
  <https://pwrev.dev/242392>`__. Bug: :bug:`373693434`.

.. 6efc99b3ee854dd54a0b1465d9014c54e01b21b9

* The ``import_prefix`` parameter in the
  ``pw_protobuf.options.load_options`` Python function was replaced with
  an ``options_files`` parameter that lets you directly specify the
  location of ``.options`` files. Commit: `Support directly specifying
  options file locations <https://pwrev.dev/240833>`__. Bug:
  :bug:`253068333`.

.. _docs-changelog-2024-10-17-Modules-pw_rpc:

pw_rpc
------
Bug fixes:

.. 05e93dadc080e45d624d92b80879297cfade417c

* A bug was fixed where previously ``Call`` objects were not getting
  reinitialized correctly. Commit: `Fix Call not getting reset on default
  constructor assignment <https://pwrev.dev/239718>`__. Bug:
  :bug:`371211198`.

.. _docs-changelog-2024-10-17-Modules-pw_spi:

pw_spi
------
Changes:

.. 4321a46654fae21df8e8fb971cd5c618b8b73d3f

* :cpp:class:`pw::spi::Initiator` now uses a non-virtual interface (NVI)
  pattern. Commit: `Use non-virtual interface (NVI) pattern on
  pw::spi::Initiator <https://pwrev.dev/236234>`__. Bug: :bug:`308479791`.

.. _docs-changelog-2024-10-17-Modules-pw_stream_uart_mcuxpresso:

pw_stream_uart_mcuxpresso
-------------------------
Changes:

.. d08c60cad881afa835a22d2bbfe36a0d6f018c1c

* :ref:`module-pw_stream_uart_mcuxpresso` is being merged into
  :ref:`module-pw_uart_mcuxpresso`. Commit: `Remove dma_stream
  <https://pwrev.dev/241201>`__. Bug: :bug:`331617914`.

.. _docs-changelog-2024-10-17-Modules-pw_system:

pw_system
---------
New features:

.. fd6b7a96cd142fcfbf979c2ebf3ea4ac2e342612

* The new ``--debugger-listen`` and ``--debugger-wait-for-client``
  options make it easier to debug the ``pw_system`` console. Commit: `Add
  \`debugger-listen\` and \`debugger-wait-for-client\` options
  <https://pwrev.dev/233752>`__.

.. _docs-changelog-2024-10-17-Modules-pw_tokenizer:

pw_tokenizer
------------
New features:

.. be439834757b0abcd0e81a77a0c8c39beca2d4db

* All domains from ELF files are now loaded by default. Commit: `Load
  all domains from ELF files by default <https://pwrev.dev/239509>`__.
  Bugs: :bug:`364955916`, :bug:`265334753`.

.. 56aa667aaa527d86241d27c5361e0d27f5aed06d

* CSV databases now include the token's domain as the third column.
  Commit: `Include the domain in CSV databases
  <https://pwrev.dev/234414>`__. Bug: :bug:`364955916`.

.. 9c37b722d9a807222c289069967222166c8613f5

* Tokenizing enums is now supported. Commit: `Add support for tokenizing
  enums <https://pwrev.dev/236262>`__. Bug: :bug:`362753838`.

Changes:

.. 17df82d4c2b77d1667f24f5b27a256dbab31686f

* When a domain is specified, any whitespace will be ignored in domain
  names and removed from the database. Commit: `Ignore whitespace in
  domain values <https://pwrev.dev/241212>`__. Bug: :bug:`362753840`.

.. _docs-changelog-2024-10-17-Modules-pw_toolchain:

pw_toolchain
------------
New features:

.. 0125f4a94c827612f1ae863b60d3fa301fbd773c

* The new :ref:`module-pw_toolchain-bazel-compiler-specific-logic`
  documentation provides guidance on how to handle
  logic that differs between compilers. Commit: `Add Bazel mechansim for
  clang/gcc-specific flags <https://pwrev.dev/238429>`__. Bug:
  :bug:`361229275`.

.. _docs-changelog-2024-10-17-Modules-pw_uart:

pw_uart
-------
New features:

.. b39ad5c71df860223a8f908219bfdcfbdda1e5f5

* The new :cpp:class:`pw::uart::UartStream` class implements the
  :cpp:class:`pw::stream::NonSeekableReaderWriter` interface on top of a
  UART device. Commit: `Add pw::uart::UartStream
  <https://pwrev.dev/241200>`__. Bug: :bug:`331603164`.

.. f6a7bb781754447aa9eea82af60962070815b4f8

* The new :cpp:class:`pw::uart::UartBlockingAdapter` class provides a
  blocking UART interface on top of a
  :cpp:class:`pw::uart::UartNonBlocking` device. Commit: `Add
  UartBlockingAdapter <https://pwrev.dev/238393>`__. Bug:
  :bug:`369679732`.

.. bdcf65850213372533c9422fdec0a199af112161

* The new :cpp:func:`pw::uart::UartNonBlocking::FlushOutput` function
  ensures that all enqueued data has been transmitted. Commit: `Add
  UartNonBlocking::FlushOutput() <https://pwrev.dev/238572>`__. Bug:
  :bug:`370051726`.

.. _docs-changelog-2024-10-17-Modules-pw_web:

pw_web
------
Changes:

.. c4ea179e91c6aee6b9d41b9fe301269189970850

* The ``device`` RPC APIs now support creating request messages for RPCs
  and calling the ``device`` API with those request messages. Commit:
  `Improvements to \`device\` RPC APIs <https://pwrev.dev/238052>`__.

.. _docs-changelog-2024-10-17-Docs:

Docs
====
New features:

.. 91d4349e08e22c50e5a738dee31cc95724eab50d

* The new :ref:`docs-blog-05-coroutines` blog post discusses the nuances
  of using coroutines in embedded systems. Commit: `Add coroutine blog
  post <https://pwrev.dev/216111>`__.

Changes:

.. 84375274c2a7dbc0fc29cb1e718d8cdfa05085fa

* Guides for contributing ``pigweed.dev`` documentation have been
  consolidated. Commit: `Consolidate
  content for pigweed.dev contributors <https://pwrev.dev/242192>`__.

.. _docs-changelog-2024-10-17-SEEDs:

SEEDs
=====
New features:

.. 385019a0292797dd63f00f008efe36d52d4d698a

* SEED :ref:`seed-0103` was accepted. Commit: `(SEED-0103) pw_protobuf:
  Past, present, and future <https://pwrev.dev/133971>`__.

.. ec62be9c7c9e28c8bae26d9e73f1ce341e5e5cd5

* :ref:`seed-0128` was accepted. Commit: `(SEED-0128) Abstracting thread
  creation <https://pwrev.dev/206670>`__.

.. _docs-changelog-2024-10-17-Targets:

Targets
=======

.. _docs-changelog-2024-10-17-Targets-rp2040:

RP2040
------
New features:

.. 8e0d91c3f7da31448419584ae9287de57ed5452f

* A new helper, ``flash_rp2350``, was added to
  ``//targets/rp2040/flash.bzl``. Commit: `Update \`flash_rp2040\` helper
  with --chip argument <https://pwrev.dev/242917>`__.

.. _docs-changelog-2024-10-03:

-----------
Oct 3, 2024
-----------

Highlights (Sep 20, 2024 to Oct 3, 2024):

* The :ref:`module-pw_async2` and :ref:`module-pw_containers` docs
  now contain code examples that are built and tested alongside the rest
  of Pigweed, minimizing the chance that they bit rot over time.
* The new :cpp:class:`pw::async2::Dispatcher` class is a single-
  threaded, cooperatively scheduled runtime for async tasks.
* The new :cpp:class:`pw::uart::UartBase` class provides a common
  abstract base class for UART interfaces.
* :cpp:class:`pw::rpc::RawServerReaderWriter` and
  :cpp:class:`pw::rpc::RawClientReaderWriter` have new methods that let
  you directly serialize RPC payloads to the RPC system's encoding buffer
  instead of requiring a copy from an externally managed buffer.

.. _docs-changelog-2024-10-03-Modules:

Modules
=======

.. _docs-changelog-2024-10-03-Modules-pw_allocator:

pw_allocator
------------
New features:

.. 020780642847dba69a9b2025f1f698fe3d8e4801

* The new :cpp:func:`pw::allocator::CalculateFragmentation` method
  calculates a fragmentation metric. This should not be invoked on-device
  unless the device has robust floating-point support. Commit: `Add
  MeasureFragmentation <https://pwrev.dev/238417>`__.

.. _docs-changelog-2024-10-03-Modules-pw_async2:

pw_async2
---------
New features:

.. 801fb32919777aefd7a734ce9c2c1e6aec782ab7

* The new :cpp:class:`pw::async2::Dispatcher` class is a single-
  threaded, cooperatively scheduled runtime for async tasks. Commit:
  `Refactor Dispatcher to raise top-level API out of CRTP
  <https://pwrev.dev/237972>`__. Bug: :bug:`342000726`.

.. 846bb7d3672e94c13451bec81098d3304d8395a9

* The :ref:`module-pw_async2` docs now have examples. Commit: `Expand
  docs with examples <https://pwrev.dev/234095>`__.

.. _docs-changelog-2024-10-03-Modules-pw_boot:

pw_boot
-------
Changes:

.. fba0833c176a79ec83403fa9d48407c5ddebb99f

* The ``main()`` function forward declaration has been moved out of an
  explicit ``extern C`` block to prevent pedantic warnings in newer Clang
  toolchains. Commit: `Move main forward declaration out of extern "C"
  block <https://pwrev.dev/237333>`__. Bug: :bug:`366374135`.

.. _docs-changelog-2024-10-03-Modules-pw_build:

pw_build
--------
Changes:

.. 53b16cd8d2907855b13bec9159286927de602a72

* The rules that previously existed in ``//pw_build/pigweed.bzl`` have
  been split into separate files. If you relied on
  ``//pw_build/pigweed.bzl`` you may need to update some ``load()``
  statements in your Bazel files. Commit: `Break apart pigweed.bzl
  <https://pwrev.dev/239133>`__. Bug: :bug:`370792896`.

.. _docs-changelog-2024-10-03-Modules-pw_chrono:

pw_chrono
---------
New features:

.. 279ab4a35543900b62e674d14c2e663532ad5a18

* The new ``--stamp`` Bazel flag ensures that Bazel builds properly
  record the actual build time as opposed to a cached value. Commit:
  `Properly stamp build time in Bazel <https://pwrev.dev/237809>`__. Bug:
  :bug:`367739962`.

.. _docs-changelog-2024-10-03-Modules-pw_containers:

pw_containers
-------------
New features:

.. 819dd2ceb430eae53909bea7d5a23c7743ef0fc2

* The ``pw_containers`` docs now provides examples on how to add
  intrusive items to multiple containers. See
  :ref:`module-pw_containers-intrusive_list-example`. Commit:
  `Multiple container example <https://pwrev.dev/237472>`__.

.. 85469bdd7f4b8eea95fad4514002383e09210a6f

* The :ref:`module-pw_containers` docs now have code examples that are
  built and tested as part of the normal upstream Pigweed build, which
  helps ensure that they don't bit rot. Commit: `Add examples
  <https://pwrev.dev/236612>`__.

Bug fixes:

.. 92ab0326113d1d6732a6d81dcc25abb330053b22

* A bug was fixed where nodes in an ``AATreeItem`` could become
  orphaned. Commit: `Fix tree rebalancing <https://pwrev.dev/237415>`__.

.. _docs-changelog-2024-10-03-Modules-pw_log:

pw_log
------
Changes:

.. 5d9a1e84e7c55a7b194a8bf459b720cbba220d0c

* The ``PW_MODULE_LOG_NAME_DEFINED`` macro has been removed. Commit:
  `Remove unused macro PW_MODULE_LOG_NAME_DEFINED
  <https://pwrev.dev/238554>`__.

.. _docs-changelog-2024-10-03-Modules-pw_metric:

pw_metric
---------
Bug fixes:

.. 30dcf2b202c792599151e7b09699e836e3cc44a3

* A bug causing ``pw::metric::Metric::Dump()`` and
  ``pw::metric::Group::Dump()`` to log invalid JSON objects was fixed.
  Commit: `Emit valid JSON from Metric::Dump
  <https://pwrev.dev/237933>`__.

.. _docs-changelog-2024-10-03-Modules-pw_preprocessor:

pw_preprocessor
---------------
Changes:

.. 92438518417d49c3457090c1b3c57f71007743e8

* ``PW_MACRO_ARG_COUNT`` now supports up to 256 arguments. Commit:
  `Expand PW_MACRO_ARG_COUNT to 256 arguments
  <https://pwrev.dev/237993>`__.

.. _docs-changelog-2024-10-03-Modules-pw_presubmit:

pw_presubmit
------------
Bug fixes:

.. 6422c9ae137ad37a9ec172dcd95f294ce5631f82

* A bug was fixed that was causing Pigweed's auto-generated ``rustdoc``
  API references to not be built. ``rustdoc`` documentation at
  ``pigweed.dev/rustdoc/*`` should be working again. Commit: `Have
  docs_build check rust docs <https://pwrev.dev/238189>`__. Bug:
  :bug:`369864378`.

.. _docs-changelog-2024-10-03-Modules-pw_rpc:

pw_rpc
------
New features:

.. 8e2fc6cfe825631416043a972cdd93875562a4fd

* :cpp:class:`pw::rpc::RawServerReaderWriter` and
  :cpp:class:`pw::rpc::RawClientReaderWriter` have new methods that let
  you directly serialize RPC payloads to the RPC system's encoding buffer
  instead of requiring a copy from an externally managed buffer. Commit:
  `Add callback writes to raw RPC call objects
  <https://pwrev.dev/239353>`__.

.. _docs-changelog-2024-10-03-Modules-pw_spi:

pw_spi
------
New features:

.. c25923e13c339cc678e8900770a512b85064f99a

* ``operator!=`` is now implemented. Commit: `Minor enhancements to
  pw::spi::Config <https://pwrev.dev/238932>`__.

Changes:

.. c25923e13c339cc678e8900770a512b85064f99a

* ``operator()``, ``operator==``, and ``operator!=`` are now marked
  ``constexpr`` to enable compile-time equality checking. Commit: `Minor
  enhancements to pw::spi::Config <https://pwrev.dev/238932>`__.

.. _docs-changelog-2024-10-03-Modules-pw_spi_rp2040:

pw_spi_rp2040
-------------
Changes:

.. 783b29c23dd865fde344501c1b99adba4c956479

* ``spi_init()`` from the Pico SDK must be called before using the
  ``pw_spi`` initiator. Commit: `Minor cleanup
  <https://pwrev.dev/236233>`__.

.. _docs-changelog-2024-10-03-Modules-pw_thread:

pw_thread
---------
Changes:

.. 30bdace4866039e26a05f8baa379630e066ad660

* The old ``pw::Thread`` constructor that takes ``void(void*)`` has been
  removed from the public API. This is a breaking change. Constructor
  usage should be migrated to the new constructor that takes
  ``pw::Function<void()>``. Commit: `Make the deprecated Thread
  constructor private <https://pwrev.dev/236435>`__. Bug:
  :bug:`367786892`.

.. 2a0f0dfccc19b86a686777afa647b0c75a87c863

* The ``pw::thread::Thread`` class was renamed to
  :cpp:type:`pw::Thread`. Commit: `Migrate from pw::thread::Thread to
  pw::Thread <https://pwrev.dev/236723>`__.

.. f1070484e7d2f4e429332c6d8520a3676b8cb965

* ``pw::thread::Id`` has been renamed to ``pw::Thread::id``. Commit:
  `Introduce pw::Thread and pw::Thread::id <https://pwrev.dev/236796>`__.

.. _docs-changelog-2024-10-03-Modules-pw_tokenizer:

pw_tokenizer
------------
New features:

.. e26be58d6db0215e6a762a5a28ad74584e9a0482

* The new :c:macro:`PW_TOKENIZER_DEFINE_TOKEN` macro makes it easier to
  support tokenized enums and domains. Commit: `Expose API to define new
  token entry <https://pwrev.dev/238272>`__. Bug: :bug:`369881416`.

Changes:

.. 02a68bb680ebddeba8f0ad6cbbc6bb81d7568759

* CSV databases now have 4 columns: token, date removed, domain, and
  string. The domain column was added as part of :ref:`seed-0105`. Legacy
  databases that only support the other 3 columns continue to be
  supported. Tokens in legacy databases are always in the default domain
  ``""``. Commit: `Support CSV databases with the domain
  <https://pwrev.dev/234413>`__. Bug: :bug:`364955916`.

.. _docs-changelog-2024-10-03-Modules-pw_toolchain:

pw_toolchain
------------
Changes:

.. bf7078a044353df9683ecb65561a0edca45a4f95

* The ``pw_toolchain`` docs were refactored. Bazel-specific guidance is
  now in :ref:`module-pw_toolchain-bazel` and GN-specific guidance is now
  in :ref:`module-pw_toolchain-gn`. Commit: `Split out build-system
  specific docs <https://pwrev.dev/238816>`__.

.. 3d0fac908c139ce83eed93727601d13747b03bf7

* The Bazel rules at ``//pw_toolchain/args/BUILD.bazel`` moved to
  ``//pw_toolchain/cc/args/BUILD.bazel``. If you rely on these rules you
  may need to update your ``load()`` statements. Commit: `Move
  pw_toolchain/args to pw_toolchain/cc/args <https://pwrev.dev/238817>`__.

.. _docs-changelog-2024-10-03-Modules-pw_transfer:

pw_transfer
-----------
Bug fixes:

.. da9a7e7cc2bd983d428949a6f5d85b9757b5178f

* A bug was fixed where resumed transfers would send a window of ``0``
  repeatedly. Commit: `Fix offset receive transfer startup
  <https://pwrev.dev/237095>`__. Bug: :bug:`368620868`.

.. _docs-changelog-2024-10-03-Modules-pw_uart:

pw_uart
-------
New features:

.. 7dc3b1b2a69972ae19bbe43f922aefd6dda73a3e

* The new :cpp:class:`pw::uart::UartBase` class provides a common
  abstract base class for UART interfaces. Commit: `Add UartBase class
  <https://pwrev.dev/238092>`__. Bug: :bug:`369678735`.

.. b4e75393442526d409968e4aac685515944b3e3e

* The new :cpp:func:`pw::uart::Uart::ReadAtLeast` method reads data from
  the UART and blocks until at least the specified number of bytes have
  been received. The new :cpp:func:`pw::uart::Uart::ReadExactly` method
  reads data from the UART and blocks until the entire buffer has been
  filled. Commit: `Add ReadAtLeast and ReadExactly methods
  <https://pwrev.dev/236268>`__. Bug: :bug:`368149122`.

.. f946f6ae13c3bbc2a648b36f9ff55642e9d23b34

* The new :cpp:class:`pw::uart::UartNonBlocking` class provides a
  callback-based interface for performing non-blocking UART communication.
  It defines the interface that concrete UART implementations must adhere
  to. Commit: `Add uart_non_blocking.h API <https://pwrev.dev/210371>`__.
  Bugs: :bug:`341356437`, :bug:`331617095`.

.. 1dc9a789f52aad1f0e738a1cfd5993e2272c38d2

* The new :cpp:func:`pw::uart::Uart::SetFlowControl` method lets
  applications configure hardware flow control on UART devices. Commit:
  `Add method to set flow control <https://pwrev.dev/237953>`__.

Changes:

.. d31705b84d24f0ef17e6b8eef6cae13f96f942e4

* ``pw::uart::Uart::ConservativeReadAvailable()`` was moved to
  :cpp:func:`pw::uart::UartBase::ConservativeReadAvailable()` and
  ``pw::uart::Uart::ClearPendingReceiveBytes()`` was moved to
  :cpp:func:`pw::uart::UartBase::ClearPendingReceiveBytes()`. Commit:
  `Move non-blocking methods from Uart to UartBase
  <https://pwrev.dev/238533>`__. Bug: :bug:`369679732`.

.. _docs-changelog-2024-10-03-Modules-pw_uart_mcuxpresso:

pw_uart_mcuxpresso
------------------
Bug fixes:

.. 94d2c3995c8a14995490ef7de015e933a960c2d5

* A bug was fixed where the ``pw::uart::DmaUartMcuxpresso::Deinit()``
  method didn't clear an initialization flag, which caused the
  ``pw::uart::DmaUartMcuxpresso::Init()`` method to be skipped on
  subsequent enables. Commit: `Fix disable bug
  <https://pwrev.dev/237394>`__.

.. _docs-changelog-2024-10-03-Docs:

Docs
====
New features:

.. 2ef99131bbb73e475f64281b6515eedba4a8cb79

* The Pigweed blog now has an `RSS feed <https://pigweed.dev/rss.xml>`_.
  Commit: `Create RSS feed for blog <https://pwrev.dev/225491>`__. Bug:
  :bug:`345857642`.

.. 8f18755dc9a225caacf2b190d1114ebfda4a2642

* The new blog post :ref:`blog-04-fixed-point` outlines how replacing
  soft floats with fixed-point arithmetic can result in speed improvements
  and binary size reductions without sacrificing correctness. Commit: `Add
  fixed point blog <https://pwrev.dev/234312>`__.

Bug fixes:

.. e81cd5e642a0c92264caafb14a2f4931cb14fced

* When a Pigweed module is listed as supporting Rust in
  ``//docs/sphinx/module_metadata.json``, a link to that module's ``rustdoc`` API
  reference is now auto-generated in the ``pigweed.dev`` site nav. Commit:
  `Auto-link to Rust API references <https://pwrev.dev/237934>`__. Bug:
  :bug:`328503976`.

.. _docs-changelog-2024-10-03-SEEDs:

SEEDs
=====
Changes:

.. 905bce3bd61280a8254ac5b3b2f78e2d3059faa0

* SEED-0123 was rejected because Pigweed ended up upstreaming the
  relevant APIs to ``rules_cc`` instead of sprouting them into a separate
  repo. Commit: `(SEED-123) Reject the SEED <https://pwrev.dev/236453>`__.

.. _docs-changelog-2024-10-03-Targets:

Targets
=======
.. a5a199593f1375177d5805f882f303d4c8b8cea2

* The ``main()`` forward declaration for ``emcraft_sf2_som``,
  ``host_device_simulator``, and ``stm32f429i_disc1_stm32cube`` are no
  longer explicitly marked ``extern C`` to prevent pedantic warnings in
  newer Clang toolchains. Commit: `Remove implicit extern C
  <https://pwrev.dev/237092>`__. Bug: :bug:`366374135`.

.. _docs-changelog-2024-09-19:

------------
Sep 19, 2024
------------
Highlights (Sep 06, 2024 to Sep 19, 2024):

* **New container classes**: The new :cpp:class:`pw::IntrusiveMap` and
  :cpp:class:`pw::IntrusiveMultiMap` classes can be used for associative
  dictionaries, sorted lists, and more.
* **Protobuf Editions**: Initial support for `Protobuf Editions
  <https://protobuf.dev/editions/overview>`__ was added for GN-based and
  CMake-based projects.
* **Token domains**: The :ref:`Detokenizer
  <module-pw_tokenizer-detokenization>` now supports
  :ref:`token domains <seed-0105>`.

.. _docs-changelog-2024-09-19-Modules:

Modules
=======

.. _docs-changelog-2024-09-19-Modules-pw_allocator:

pw_allocator
------------
New features:

.. d5fcc90b39ee7568855390535fa854cea8f33c95

* The new :c:macro:`PW_ALLOCATOR_STRICT_VALIDATION` option lets you
  enable more expensive checks to aggressively enforce invariants when
  testing. The new :c:macro:`PW_ALLOCATOR_BLOCK_POISON_INTERVAL` option
  allows setting the poisoning rate more easily from the build rather than
  in code via template parameters. See
  :ref:`module-pw_allocator-module-configuration`. Commit: `Add module config
  <https://pwrev.dev/232211>`__.

Bug fixes:

.. 82759ccb711c3f34320ae9ae37bf70a029baec57

* A bug was fixed where ``pw_allocator`` always split the first block
  even if there was not enough room for the first block to be split into
  two, which could cause heap corruption and crashes. Commit: `Check for
  room to split the first block <https://pwrev.dev/235312>`__. Bug:
  :bug:`366175024`.

.. _docs-changelog-2024-09-19-Modules-pw_assert:

pw_assert
---------
Changes:

.. cfcf0059926589e26f318e29df8733e5a09c2928

* :c:macro:`PW_CHECK_OK` now accepts any expression that's convertible
  to :cpp:class:`pw::Status`. Commit: `Update PW_CHECK_OK() to handle any
  expr convertible to Status <https://pwrev.dev/234820>`__. Bugs:
  :bug:`357682413`, :bug:`365592494`.

.. _docs-changelog-2024-09-19-Modules-pw_async2:

pw_async2
---------
New features:

.. cfcbaf5bbc67288b5e8954f22528c4de9312effe

* The new
  :cpp:func:`pw::async2::SimulatedTimeProvider::AdvanceUntilNextExpiration`
  utility method is useful for advancing test time without
  random periods or endless iterations. Commit: `Add more
  SimulatedTimeProvider utilities <https://pwrev.dev/234929>`__.

Changes:

.. ed0fd1f45a3a137965dbb2075227b8ef0e91f935

* Coroutines now log the requested size when an allocation fails.
  Commit: `Log size of failed coroutine allocations
  <https://pwrev.dev/234801>`__.

.. _docs-changelog-2024-09-19-Modules-pw_build:

pw_build
--------
Bug fixes:

.. 3919d9638b6454512595c8ad39fb8806d4ac9629

* An issue was fixed where bootstrap failed when
  ``pw_rust_static_library`` was used. Commit: `Fix Undefined identifier
  <https://pwrev.dev/232371>`__.

.. _docs-changelog-2024-09-19-Modules-pw_bytes:

pw_bytes
--------
New features:

.. cda5ba673366d189e0ea326a0fa808df181730a7

* The new :cpp:class:`pw::PackedPtr` template class provides a way to
  store extra data in the otherwise unused least significant bits of a
  pointer. Commit: `Add PackedPtr <https://pwrev.dev/235104>`__.

.. _docs-changelog-2024-09-19-Modules-pw_containers:

pw_containers
-------------
New features:

.. df3b7ba1f94902e81e375ce9935749163c411515

* ``pw::IntrusiveList`` now has a :ref:`size report
  <module-pw_containers-intrusivelist-size-report>`. Commit:
  `Add IntrusiveForwardList size report to the docs
  <https://pwrev.dev/233651>`__.

.. 8a3250d2f4287c2f66c4afd7679f9b10f789e764

* The new :cpp:class:`pw::IntrusiveMap` and
  :cpp:class:`pw::IntrusiveMultiMap` classes can be used for associative
  dictionaries, sorted lists, and more. Commit: `Add IntrusiveMap and
  IntrusiveMultiMap <https://pwrev.dev/216828>`__.

Changes:

.. 314e457eaf3a801115542d777e2157e6df85fb31

* ``pw::IntrusiveList<T>`` was renamed to
  ``pw::IntrusiveForwardList<T>`` and a new doubly-linked intrusive list
  was added as ``pw::containers::future::IntrusiveList<T>``. An alias,
  ``pw::IntrusiveList<T>``, was added to maintain compatibility with
  existing code and will be removed in the future. The original
  implementation can still be temporarily enabled by setting
  ``PW_CONTAINERS_USE_LEGACY_INTRUSIVE_LIST``. Commit: `Add doubly linked
  list <https://pwrev.dev/230811>`__. Bug: :bug:`362348318`.

.. _docs-changelog-2024-09-19-Modules-pw_env_setup:

pw_env_setup
------------
Changes:

.. 16f0f6387505dc27e7c1a76387b05524752b4602

* The Git submodule check is now skipped when no ``.git`` file or
  directory is detected. Commit: `Add check for git in
  _check_submodule_presence <https://pwrev.dev/234212>`__. Bug:
  :bug:`365557573`.

.. _docs-changelog-2024-09-19-Modules-pw_ide:

pw_ide
------
Bug fixes:

.. 145b45747105fb95e5625c00a7533e5375d124ea

* When ``clangd`` is not found, ``pw ide sync`` now cleanly handles the
  lack of ``clangd`` and successfully completes the rest of the sync.
  Commit: `Don't fail sync on missing clangd
  <https://pwrev.dev/236475>`__. Bug: :bug:`349189723`.

.. _docs-changelog-2024-09-19-Modules-pw_protobuf:

pw_protobuf
-----------
New features:

.. b299522cffb0a18e07528e923f376ceee3e9c188

* Initial support for `Protobuf Editions
  <https://protobuf.dev/editions/overview>`__ was added for GN-based and
  CMake-based projects. Commit: `Basic edition support
  <https://pwrev.dev/235873>`__.

.. _docs-changelog-2024-09-19-Modules-pw_spi_linux:

pw_spi_linux
------------
Bug fixes:

.. eefd313bdb13098552cd713598b937debe80d3d4

* A performance issue was fixed where ``Configure()`` was being called
  on each ``pw::spi::Device::WriteRead()`` call. Commit: `Avoid
  unnecessary ioctl()s in Configure() <https://pwrev.dev/235877>`__. Bug:
  :bug:`366541694`.

.. _docs-changelog-2024-09-19-Modules-pw_spi_mcuxpresso:

pw_spi_mcuxpresso
-----------------
Changes:

.. 9d175062d56972f082ce99753092b75419a228ce

* ``pw::spi::McuxpressoInitiator::DoConfigure()`` was renamed to
  ``pw::spi::McuxpressoInitiator::DoConfigureLocked()``. Commit: `Rename
  DoConfigure() to DoConfigureLocked() <https://pwrev.dev/236232>`__.

.. _docs-changelog-2024-09-19-Modules-pw_sys_io_stm32cube:

pw_sys_io_stm32cube
-------------------
Bug fixes:

.. b0f73feb04effde3b7751c53c21b7a163f234eb8

* A bug was fixed where the GPIO mode of the UART RX GPIO pin on
  STM32F1XX devices was not being correctly set. Commit: `Fix UART RX GPIO
  mode for f1xx family <https://pwrev.dev/235332>`__.

.. _docs-changelog-2024-09-19-Modules-pw_system:

pw_system
---------
New features:

.. 5e148c19477521afbbedcc8a91a2c5b2a07bc334

* The console's new ``timestamp_decoder`` constructor parameter lets
  applications provide custom timestamp parsers. Commit: `Support
  timestamp parser as an argument <https://pwrev.dev/234931>`__. Bug:
  :bug:`344606797`.

Changes:

.. faac61757b5428be3787729d328f6f2f3ebfa9f1

* The log library header (``pw_system/log.h``) of ``pw_system`` is now
  public and can be used outside of ``pw_system``. Commit: `Make log
  library header public <https://pwrev.dev/233411>`__.

.. _docs-changelog-2024-09-19-Modules-pw_thread:

pw_thread
---------
Changes:

.. 8a67d6b57b526757ffa010be2be402c42cd13ac4

* ``pw::thread::Thread`` now takes ``pw::Function<void()>``, which
  should be used in place of the ``void(void*)`` function pointer and
  ``void*`` argument. Commit: `Mark legacy function* / void* constructor
  as deprecated <https://pwrev.dev/236454>`__. Bug: :bug:`367786892`.

.. _docs-changelog-2024-09-19-Modules-pw_tokenizer:

pw_tokenizer
------------
New features:

.. 9fb87e78e4c41778fc950714d58e6602f63d27e6

* The :ref:`Detokenizer <module-pw_tokenizer-detokenization>` now
  supports :ref:`token domains <seed-0105>`. Commit:
  `Add token domain support to Detokenizer <https://pwrev.dev/234968>`__.
  Bug: :bug:`362752722`.

.. 08ff555993b8ab250ea03a9f12aaf5c2d1c9c705

* :py:class:`pw_tokenizer.tokens.Database` now supports :ref:`token
  domains <seed-0105>`. Commit: `Use domains in the Python tokens.Database
  class <https://pwrev.dev/234412>`__.

.. _docs-changelog-2024-09-19-Modules-pw_toolchain:

pw_toolchain
------------
New features:

.. 40f756e2d3c40eeb32832309dbcae989fb750268

* ``WORKSPACE`` toolchain registration is now configurable so that
  downstream projects can manually control which toolchains get
  registered. Commit: `Make toolchain registration configurable
  <https://pwrev.dev/235712>`__. Bug: :bug:`346388161`.

.. _docs-changelog-2024-09-19-Modules-pw_transfer:

pw_transfer
-----------
Bug fixes:

.. f1f654a15a3adce476c2d68643eee56f3c225dd4

* A bug was fixed where a handshake timeout was not set after
  ``START_ACK`` is processed. Commit: `Bugfix for start handshake, and
  rate limit logs <https://pwrev.dev/236572>`__. Bug: :bug:`361281209`.

Changes:

.. 2496aee1a4ab3d98526a7357943b69347a39903a

* When a receiver receives a chunk of data it already has the receiver
  now sends a ``PARAMETERS_CONTINUE`` chunk instead of requesting
  retransmission. Commit: `Send continue parameters for already received
  chunks <https://pwrev.dev/235100>`__.

.. f1f654a15a3adce476c2d68643eee56f3c225dd4

* TX data chunk logs have been rate-limited to only send once every 3
  seconds. Commit: `Bugfix for start handshake, and rate limit logs
  <https://pwrev.dev/236572>`__. Bug: :bug:`361281209`.

.. _docs-changelog-2024-09-19-Modules-pw_uart_mcuxpresso:

pw_uart_mcuxpresso
------------------
New features:

.. e8ab2b0ac31c0dde6febd0d384c0ea7d688f6803

* Flow control can now be configured. Commit: `Add support for
  configuring flow control <https://pwrev.dev/236896>`__. Bug:
  :bug:`368150004`.

Changes:

.. e8ab2b0ac31c0dde6febd0d384c0ea7d688f6803

* Flow control, parity mode, and stop bits now have default values.
  Commit: `Add support for configuring flow control
  <https://pwrev.dev/236896>`__. Bug: :bug:`368150004`.

.. _docs-changelog-2024-09-19-Build-systems:

Build systems
=============

.. _docs-changelog-2024-09-19-Build-systems-Bazel:

Bazel
-----
New features:

.. 4ceb5b8bf0faf75c0b051114abf85a2ea73ca39c

* The new ``do_not_build`` tag specifies targets that should be excluded
  from wildcard builds. The new ``do_not_run_test`` tag specifies test
  targets that should be built but not executed. Commit: `Introduce
  do_not_build, do_no_run_test tags <https://pwrev.dev/223492>`__. Bug:
  :bug:`353531487`.

.. 54679d205e4888302ab24882e6fb64bf8ba964c6

* `Platform-based flags <https://github.com/bazelbuild/proposals/blob/ma
  in/designs/2023-06-08-platform-based-flags.md>`__ have been re-enabled.
  Commit: `Re-enable platform-based flags <https://pwrev.dev/234135>`__.
  Bug: :bug:`301334234`.

.. _docs-changelog-2024-09-19-Miscellaneous:

Miscellaneous
=============
Bug fixes:

.. 982c7f42878871e7f85dbc5420ff17f0b8ede237

* An issue was fixed where the Fuchsia SDK was always fetched during
  Bazel workspace initialization and caused unnecessary downloads. Commit:
  `Use @fuchsia_clang as a cipd repository <https://pwrev.dev/233531>`__.
  Bug: :bug:`346416385`.

-----------
Sep 5, 2024
-----------
.. note::

   This changelog update is shorter than previous ones because we're
   experimenting with only showing user-facing new features, changes,
   and bug fixes. I.e. we're omitting commits that don't affect
   downstream Pigweed projects.

Highlights (Aug 24, 2024 to Sep 5, 2024):

* **New backends**: :ref:`module-pw_async_fuchsia` (a ``pw_async``
  backend for Fuchsia that implements ``Task`` and ``FakeDispatcher``),
  :ref:`module-pw_log_fuchsia` (a ``pw_log`` backend for Fuchsia
  that uses the ``fuchsia.logger.LogSink`` FIDL API to send logs),
  :ref:`module-pw_random_fuchsia` (a ``pw_random`` backend for Fuchsia
  that implements :cpp:class:`pw::random::RandomGenerator`)
  and :ref:`module-pw_uart_mcuxpresso` (a ``pw_uart`` backend for
  NXP MCUXpresso devices).
* **New theme**: The underlying Sphinx theme powering ``pigweed.dev`` is now
  `PyData <https://pydata-sphinx-theme.readthedocs.io/en/stable/>`_. In
  addition to improving website usability, this theme should also reduce
  the ``pigweed.dev`` maintenance workload over time. See
  :ref:`seed-0130` for more information.
* **Arm Cortex-M55 support**: ``pw_toolchain`` and ``pw_system`` now
  support Arm Cortex-M55 cores.
* **Bazel cloud demo**: The new :ref:`Bazel cloud features
  <showcase-sense-tutorial-bazel_cloud>`
  page in the Sense tutorial shows you how to use BuildBuddy
  to share logs and speed up builds with remote caching.

Modules
=======

pw_async2
---------
* The new :cpp:class:`pw::async2::TimeProvider` class can be used to
  create timers in a dependency-injection-friendly way.
  Commit: `Add TimeProvider
  <https://pwrev.dev/232411>`__

pw_async_fuchsia
----------------
* :ref:`module-pw_async_fuchsia` is a new ``pw_async`` backend for Fuchsia
  that implements ``Task`` and ``FakeDispatcher``.
  Commit: `Create pw_async Fuchsia backend
  <https://pwrev.dev/230896>`__

pw_chrono
---------
* :cpp:class:`pw::chrono::VirtualClock` is a new virtual base class for
  timers that enables writing
  timing-sensitive code that can be tested using simulated clocks such as
  :cpp:class:`pw::chrono::SimulatedSystemClock`.
  Commit: `Add VirtualClock
  <https://pwrev.dev/233031>`__

pw_cli
------
* The new :py:meth:`pw_cli.git_repo.GitRepo.commit_date()` method returns
  the datetime of a specified commit.
  Commit: `Add in option to retrieve commit date
  <https://pwrev.dev/216275>`__

pw_digital_io_mcuxpresso
------------------------
* The GPIO clock is now enabled even when GPIO is disabled.
  Commit: `Enable gpio clock even when disabling gpio
  <https://pwrev.dev/232131>`__
  (issue `#356689514 <https://pwbug.dev/356689514>`__)

pw_log_fuchsia
--------------
* :ref:`module-pw_log_fuchsia` is a new ``pw_log`` backend that uses the
  ``fuchsia.logger.LogSink`` FIDL API to send logs.
  Commit: `Create pw_log Fuchsia backend
  <https://pwrev.dev/231052>`__

pw_log_rpc
----------
* ``pw_log_rpc.LogStreamHandler.listen_to_logs()`` was renamed to
  :py:meth:`pw_log_rpc.LogStreamHandler.start_logging()`.
  Commit: `Invoke pw.log.Logs.Listen() to restore prior behavior
  <https://pwrev.dev/233991>`__
  (issue `#364421706 <https://pwbug.dev/364421706>`__)

pw_log_zephyr
-------------
* Use of shell ``printf`` macros within ``if`` blocks that don't use
  braces no longer causes compile errors.
  Commit: `Make shell printf macros safe for use in if/else blocks
  <https://pwrev.dev/232031>`__

pw_package
----------
* ``picotool`` installation on macOS was fixed.
  Commit: `Fix pictotool install on mac
  <https://pwrev.dev/234238>`__

pw_random_fuchsia
-----------------
* :ref:`module-pw_random_fuchsia` provides an implementation of
  :cpp:class:`pw::random::RandomGenerator` that uses Zircon.
  Commit: `Create Fuchsia backend for pw_random
  <https://pwrev.dev/230895>`__

pw_rpc
------
* New documentation (:ref:`module-pw_rpc-guides-raw-fallback`) was added that
  explains how to define a raw method within a non-raw service.
  Commit: `Provide examples of raw methods in docs
  <https://pwrev.dev/232877>`__
* Many RPC-related classes were moved out of ``pw_hdlc`` and into
  ``pw_rpc`` or ``pw_stream``.
  Commit: `Relocate RPC classes from pw_hdlc
  <https://pwrev.dev/230172>`__
  (issues `#330177657 <https://pwbug.dev/330177657>`__,
  `#360178854 <https://pwbug.dev/360178854>`__)

pw_spi_mcuxpresso
-----------------
* The new ``pw::spi::FifoErrorCheck`` enum lets you configure whether
  ``pw::spi::McuxpressoResponder`` instances should log FIFO errors.
  Commit: `Add check_fifo_error to responder config
  <https://pwrev.dev/232215>`__

pw_stream_uart_linux
--------------------
* The new :cpp:struct:`pw::stream::UartStreamLinux::Config` struct lets
  you configure baud rate and control flow.
  Commit: `Add Config struct
  <https://pwrev.dev/233591>`__
  (issue `#331871421 <https://pwbug.dev/331871421>`__)

pw_sync
-------
* :cpp:func:`pw::sync::InterruptSpinLock::try_lock` and similar functions
  have been annotated with ``[[nodiscard]]`` which means that ignoring
  their return values will result in compiler warnings.
  Commit: `[[nodiscard]] for try_lock() and similar functions
  <https://pwrev.dev/229311>`__

pw_system
---------
* :ref:`module-pw_system` now supports Arm Cortex-M55 systems.
  Commit: `Support ARM Cortex M55 system
  <https://pwrev.dev/231632>`__
  (issue `#361691368 <https://pwbug.dev/361691368>`__)

pw_thread
---------
* :cpp:class:`pw::thread::Options` has moved to its own header
  (``pw_thread/options.h``) to make it possible to work with the class
  without relying on the thread facade.
  Commit: `Move pw::thread::Options to its own header
  <https://pwrev.dev/232151>`__

pw_tokenizer
------------
* In Python the detokenizer prefix is now set in the
  :py:class:`pw_tokenizer.detokenize.Detokenizer` constructor.
  Commit: `Set prefix in Detokenizer; fix typing issues
  <https://pwrev.dev/234311>`__

pw_toolchain
------------
* Arm Cortex-M55 toolchain support was added.
  Commit: `Add ARM Cortex-M55 toolchain
  <https://pwrev.dev/231631>`__
  (issue `#361691368 <https://pwbug.dev/361691368>`__)

pw_uart_mcuxpresso
------------------
* The new :ref:`module-pw_uart_mcuxpresso` module is a
  :ref:`module-pw_uart` backend for NXP MCUXpresso devices.
  Commit: `Introduce DMA UART backend for NXP devices
  <https://pwrev.dev/232831>`__

Docs
====
* New documentation (:ref:`docs-pw-style-cpp-logging`) about logging best
  practices was added.
  Commit: `Add logging recommendations
  <https://pwrev.dev/210204>`__
* The new :ref:`Bazel cloud features <showcase-sense-tutorial-bazel_cloud>`
  page in the Sense tutorial shows you how to use BuildBuddy
  to share logs and speed up builds with remote caching.
  Commit: `Add cloud build section to Sense tutorial
  <https://pwrev.dev/233751>`__
  (issue `#363070027 <https://pwbug.dev/363070027>`__)
* :ref:`docs-contributing` now links to good first issues for people who
  want to contribute to upstream Pigweed.
  Commit: `Add link to good first issue list
  <https://pwrev.dev/233652>`__
* ``pigweed.dev/live`` now links to the Pigweed Live meeting notes.
  Commit: `Add shortlink for Pigweed Live notes
  <https://pwrev.dev/232032>`__

Targets
=======

RP2350
------
* RP2350 crash snapshots now show the correct architecture.
  Commit: `Fix architecture in crash snapshot
  <https://pwrev.dev/232231>`__
  (issue `#362506213 <https://pwbug.dev/362506213>`__)

------------
Aug 23, 2024
------------
.. _Google Pigweed comes to our new RP2350: https://www.raspberrypi.com/news/google-pigweed-comes-to-our-new-rp2350/

Highlights (Aug 8, 2024 to Aug 23, 2024):

* **RP2350 Support**: Pigweed now supports the new Raspberry Pi RP2350 MCU.
  Check out `Google Pigweed comes to our new RP2350`_ on the Raspberry Pi
  blog for the full story and :ref:`showcase-sense` to try it out.

Build systems
=============

Bazel
-----
* `Add missing counting_semaphore and thread_yield backends
  <https://pwrev.dev/228392>`__

Modules
=======

pw_allocator
------------
* `Disable example spin_lock test on RP2
  <https://pwrev.dev/231251>`__
  (issues `#358411629 <https://pwbug.dev/358411629>`__,
  `#361354335 <https://pwbug.dev/361354335>`__)

pw_async2
---------
The new ``pw::async2::MakeOnceSenderAndReceiver()`` function template makes it
easier to simultaneously create a sender and receiver for asynchronously
sending values. The new ``pw::async2::MakeOnceSenderAndReceiver()`` function
template works similarly but is used for references.

* `Create OnceSender & OnceReceiver
  <https://pwrev.dev/226231>`__
  (issue `#350994818 <https://pwbug.dev/350994818>`__)
* `Remove accidental macro #undef
  <https://pwrev.dev/229275>`__

pw_bloat
--------
The ``pw bloat`` CLI command now supports a ``--custom-config`` option to
specify a custom Bloaty config file so that non-memory regions can be
analyzed.

* `Support custom bloaty configs in CLI command
  <https://pwrev.dev/216133>`__

pw_bluetooth
------------
* `Create more emboss event definitions
  <https://pwrev.dev/231091>`__
  (issue `#42167863 <https://pwbug.dev/42167863>`__)
* `Add create connection cancel return
  <https://pwrev.dev/230251>`__
  (issue `#42167863 <https://pwbug.dev/42167863>`__)
* `Add LEReadSupportedStates emboss event
  <https://pwrev.dev/228961>`__
* `Add HCI Command OpCode definitions
  <https://pwrev.dev/228672>`__
* `Add more emboss definitions
  <https://pwrev.dev/228671>`__
  (issue `#42167863 <https://pwbug.dev/42167863>`__)
* `Make SupportedCommands emboss struct more ergonomic
  <https://pwrev.dev/228155>`__

pw_bluetooth_proxy
------------------
* `Release H4 buff on error
  <https://pwrev.dev/229011>`__

pw_bluetooth_sapphire
---------------------
* `Add common bt-host clang warnings
  <https://pwrev.dev/228651>`__
  (issue `#345799180 <https://pwbug.dev/345799180>`__)
* `Add new emboss compiler file ir_data_fields.py
  <https://pwrev.dev/228655>`__
  (issue `#358665524 <https://pwbug.dev/358665524>`__,
  `#335724776 <https://pwbug.dev/335724776>`__)

pw_build
--------
The new ``glob_dirs()`` Starlark helper returns a list of directories matching
the provided glob pattern. The new ``match_dir()`` Starlark helper returns a
single directory that matches the provided glob pattern and fails if there's
more than one match.

* `Add mod proc_macro to rust macro targets
  <https://pwrev.dev/230013>`__
* `Remove output_name attr in rust_library
  <https://pwrev.dev/230012>`__
* `Make pw_load_phase_test host only
  <https://pwrev.dev/230072>`__
* `Add glob_dirs() Starlark helper
  <https://pwrev.dev/228956>`__

pw_channel
----------
* `Ensure that stream_channel_test resources live forever
  <https://pwrev.dev/228154>`__
  (issue `#358078118 <https://pwbug.dev/358078118>`__)

pw_cpu_exception_cortex_m
-------------------------
* `Fix cpu exception handler on armv8m
  <https://pwrev.dev/231372>`__
  (issue `#323215726 <https://pwbug.dev/323215726>`__)

pw_display
----------
The new :ref:`module-pw_display` experimental module provides graphic display
support and framebuffer management.

* `Color library
  <https://pwrev.dev/229606>`__
  (issue `#359953386 <https://pwbug.dev/359953386>`__)
* `Create module directory
  <https://pwrev.dev/229712>`__

pw_env_setup
------------
* `Get bazelisk instead of bazel
  <https://pwrev.dev/226376>`__
  (issue `#355438774 <https://pwbug.dev/355438774>`__)

pw_grpc
-------
When a frame with a payload has a stream-reset error, the payload is now skipped.

* `Improve logging when receiving unknown RPC
  <https://pwrev.dev/231011>`__
* `Skip HTTP2 frame payload for frames that result in stream reset
  <https://pwrev.dev/230951>`__

pw_i2c_rp2040
-------------
The ``clock_frequency`` field in ``pw::i2c::Rp2040Initiator::Config`` has been
renamed to ``clock_frequency_hz``.

* `Include label in clock_frequency
  <https://pwrev.dev/221412>`__

pw_ide
------
``pw_ide`` now searches all parent directories for the presence of a
``pigweed.json`` file to determine the root directory. The workspace
root can be programmatically configured via the new
``pw_ide.settings.PigweedIdeSettings.workspace_root`` property. The
``pigweed.activateBazeliskInNewTerminals`` option in VS Code now defaults
to ``false``.

* `Fix .pw_ide.yaml paths
  <https://pwrev.dev/230991>`__
* `Support different workspace root
  <https://pwrev.dev/217220>`__
* `Disable Bazelisk auto-activation by default
  <https://pwrev.dev/228493>`__
  (issue `#358384211 <https://pwbug.dev/358384211>`__)

pw_kvs
------
References to ``pw::kvs::Key`` must be replaced with ``std::string_view``.

* `Remove unnecessary Key alias and test
  <https://pwrev.dev/229976>`__
* `Pass EntryHeader by const reference
  <https://pwrev.dev/229727>`__
  (issue `#254601862 <https://pwbug.dev/254601862>`__)
* `Move inline variable definition to .cc file
  <https://pwrev.dev/228514>`__
  (issue `#357162923 <https://pwbug.dev/357162923>`__)

pw_module
---------
* `Fix OWNERS file parsing
  <https://pwrev.dev/226177>`__

pw_multibuf
-----------
The ``pw::multibuf::MultiBuf::Chunks()`` method and
``pw::multibuf::MultiBuf::ChunkIterable`` class have been removed; use the
new ``pw::multibuf::MultiBufChunks`` class instead.

* `Restructure ChunkIterable
  <https://pwrev.dev/230892>`__
* `Comment updates for consistency
  <https://pwrev.dev/230891>`__

pw_package
----------
* `Use bazel to build picotool package
  <https://pwrev.dev/229431>`__

pw_preprocessor
---------------
The new ``PW_MODIFY_DIAGNOSTIC_CLANG`` define lets you handle Clang-only
warnings separately from other compilers.

* `Test GCC/Clang diagnostic modification macros
  <https://pwrev.dev/231336>`__
* `Introduce PW_MODIFY_DIAGNOSTIC_CLANG
  <https://pwrev.dev/231332>`__
  (issue `#356935569 <https://pwbug.dev/356935569>`__)
* `Expand comment for PW_PACKED
  <https://pwrev.dev/226994>`__

pw_rpc
------
Java client: The new ``PacketByteFactory`` Java class is a helper for creating
request and response packets during testing.

Python client: The ``pw_rpc.console_tools.watchdog.Watchdog`` Python class has a
new ``stop()`` method to stop the watchdog.  The
``pw_rpc.callback_client.call.Call`` Python class now accepts a
``max_responses`` argument that lets you limit how many responses should be
received after a streaming RPC call.  The ``ignore_errors``,
``cancel_duplicate_calls``, and ``override_pending_options`` arguments
previously available in some Python client methods have been removed.

TypeScript client: The ``invoke()`` method in the TypeScript client now accepts
a ``maxResponses`` argument which lets you limit how many responses to a
streaming RPC call should be stored.

* `Avoid recompiling protos for every test
  <https://pwrev.dev/230135>`__
  (issue `#360184800 <https://pwbug.dev/360184800>`__)
* `Fix typing in unaryWait return value in TS client
  <https://pwrev.dev/231071>`__
* `Create PacketByteFactory
  <https://pwrev.dev/230011>`__
  (issue `#360174359 <https://pwbug.dev/360174359>`__)
* `Add stop method to Watchdog
  <https://pwrev.dev/230692>`__
  (issue `#350822543 <https://pwbug.dev/350822543>`__)
* `Limit maximum stored responses in TypeScript client
  <https://pwrev.dev/229975>`__
* `Add missing Bazel test rules
  <https://pwrev.dev/230691>`__
* `Restore RpcIds for testing; move packet encoding to packets.py
  <https://pwrev.dev/230471>`__
* `Limit maximum stored responses in Python client
  <https://pwrev.dev/229974>`__
  (issue `#262749163 <https://pwbug.dev/262749163>`__)
* `Remove deprecated / obsolete features
  <https://pwrev.dev/229908>`__
* `Fix open in callback_client
  <https://pwrev.dev/169174>`__
  (issue `#309159260 <https://pwbug.dev/309159260>`__)
* `Merge PendingRpc and RpcIds
  <https://pwrev.dev/228952>`__
* `Require an output function for channels
  <https://pwrev.dev/227855>`__

pw_spi_linux
------------
* `Remove linkage specification from cli main
  <https://pwrev.dev/230291>`__

pw_status
---------
The ``StatusWithSize::size_or()`` method has been removed.

* `Convert StatusWithSize to Doxygen
  <https://pwrev.dev/229980>`__
* `Remove StatusWithSize::size_or()
  <https://pwrev.dev/229979>`__

pw_stream
---------
* `Disable mpsc_stream_test for Pi Pico
  <https://pwrev.dev/231212>`__
  (issue `#361369435 <https://pwbug.dev/361369435>`__)

pw_system
---------
The ``pw_system.device.Device`` class constructor now accepts an ``Iterable``
of proto libraries rather than a ``list``.

* `Add type annotation to pw_system.device.Device() write arg
  <https://pwrev.dev/229653>`__
* `Update Device ctor to take Iterable of proto libraries
  <https://pwrev.dev/229811>`__

pw_thread
---------
* `Disable test_thread_context_facade_test on Pi Pico
  <https://pwrev.dev/231291>`__
  (issue `#361369192 <https://pwbug.dev/361369192>`__)

pw_tokenizer
------------
* `Remove unsupported C++11 and C++14 code
  <https://pwrev.dev/222432>`__

pw_trace_tokenized
------------------
* `Fix TokenizedTracer initialization
  <https://pwrev.dev/230314>`__
  (issue `#357835484 <https://pwbug.dev/357835484>`__)

pw_transfer
-----------
Transfer handler registration and unregistration functions now return a boolean
indicating success or failure. The C++ client now always includes a protocol
version in the final chunk.

* `Add return values to handler registrations
  <https://pwrev.dev/230912>`__
* `Always set protocol version in final chunk
  <https://pwrev.dev/229289>`__

pw_unit_test
------------
* `Delete unsupported C++14 compatibility code
  <https://pwrev.dev/229972>`__
* `Fix multi-line test macros
  <https://pwrev.dev/229314>`__

Docs
====
* `Require unit tests to be in unnamed namespace
  <https://pwrev.dev/231211>`__
* `Add structured data to Kudzu blog post
  <https://pwrev.dev/230647>`__
  (issue `#360924425 <https://pwbug.dev/360924425>`__)
* `Fix Pigweed Live CTA link
  <https://pwrev.dev/230693>`__
  (issue `#357957451 <https://pwbug.dev/357957451>`__)
* `Replace 'bazel' with 'bazelisk'
  <https://pwrev.dev/226377>`__
* `Update Sense flashing instructions
  <https://pwrev.dev/229608>`__
* `Update RP2 family udev rules
  <https://pwrev.dev/228513>`__
  (issue `#355291899 <https://pwbug.dev/355291899>`__)
* `Update "Who's using Pigweed" section
  <https://pwrev.dev/228494>`__
* `Update Bazel quickstart mentions
  <https://pwrev.dev/228531>`__
* `Launch Sense
  <https://pwrev.dev/228431>`__

Targets
=======

rp2350
------
Pigweed now supports the new Raspberry Pi RP2350 MCU.

* `Add support for RP2350
  <https://pwrev.dev/228326>`__
  (issue `#354942782 <https://pwbug.dev/354942782>`__)

rp2040
------
* `Reset tty flags after successful flash on posix
  <https://pwrev.dev/229721>`__
* `Remove references to b/261603269
  <https://pwrev.dev/229397>`__
* `Add -fexceptions for the rp2040 PIO assembler
  <https://pwrev.dev/229531>`__
* `Get Pico SDK and Picotool from BCR
  <https://pwrev.dev/228327>`__

Third-party software
====================

Emboss
------
* `Remove -Wdeprecated-copy from public_config
  <https://pwrev.dev/228563>`__
  (issue `#345526399 <https://pwbug.dev/345526399>`__)
* `Update emboss to v2024.0809.170004
  <https://pwrev.dev/228562>`__
  (issue `#345526399 <https://pwbug.dev/345526399>`__)

Fuchsia
-------
* `Update patch to ignore warnings in result.h
  <https://pwrev.dev/231253>`__
* `Copybara import
  <https://pwrev.dev/231293>`__

Rolls
-----
* `fuchsia_infra: [roll] Roll fuchsia-infra-bazel-rules-bazel_sdk-ci
  <https://pwrev.dev/230635>`__
* `fuchsia_infra 54 commits
  <https://pwrev.dev/230634>`__
* `go
  <https://pwrev.dev/230631>`__
* `rust
  <https://pwrev.dev/230472>`__
* `go
  <https://pwrev.dev/228811>`__
* `gn
  <https://pwrev.dev/228731>`__
* `fuchsia_infra 27 commits
  <https://pwrev.dev/228712>`__
* `rust
  <https://pwrev.dev/228566>`__

Miscellaneous
=============
* `Change typedef to using
  <https://pwrev.dev/230351>`__
* `Handle ignored status comments
  <https://pwrev.dev/229652>`__
  (issues `#357136096 <https://pwbug.dev/357136096>`__,
  `#357139112 <https://pwbug.dev/357139112>`__)

Owners
------
* `Add davidroth@
  <https://pwrev.dev/230071>`__

-----------
Aug 7, 2024
-----------
Highlights (Jul 26, 2024 to Aug 7, 2024):

* **Tour of Pigweed**: The new :ref:`Tour of Pigweed <showcase-sense>`
  is a hands-on, guided walkthrough of many key Pigweed features working
  together in a medium-complexity application.
* **Easier pw_digital_io testing**: The new
  ``pw::digital_io::DigitalInOutMock`` class is a mock implementation of
  ``pw:digital_io::DigitalInOut`` that can be used for testing.
* **Code intelligence in VS Code**: The new
  :ref:`module-pw_ide-guide-vscode-code-intelligence` document provides
  guides on using the code intelligence features of the ``pw_ide``
  extension for VS Code.

Build systems
=============

Bazel
-----
* `Get picotool from the BCR
  <https://pwrev.dev/227838>`__
  (issue `#354270165 <https://pwbug.dev/354270165>`__)
* `Manage pw_ide deps via bzlmod
  <https://pwrev.dev/226733>`__
  (issue `#258836641 <https://pwbug.dev/258836641>`__)
* `Manage Java deps through bzlmod
  <https://pwrev.dev/226481>`__
  (issue `#258836641 <https://pwbug.dev/258836641>`__)
* `Provide symlink to clangd at root
  <https://pwrev.dev/226451>`__
  (issue `#355655415 <https://pwbug.dev/355655415>`__)
* `Partial revert of http://pwrev.dev/226007
  <https://pwrev.dev/226271>`__
  (issue `#352389854 <https://pwbug.dev/352389854>`__)

Modules
=======

pw_allocator
------------
* `Fix bucketed block corruption
  <https://pwrev.dev/227604>`__
  (issue `#345526413 <https://pwbug.dev/345526413>`__)
* `Add missing include
  <https://pwrev.dev/227174>`__
  (issue `#356667663 <https://pwbug.dev/356667663>`__)

pw_bluetooth
------------
``ReadLocalSupportedCommandsCommandCompleteEvent`` now provides both a raw bytes
field (``supported_commands_bytes``) and a sub-struct (``supported_commands``)
for easier access to command bits. You can use either ``SupportedCommandsOctet``
or ``SupportedCommands`` to parse a saved ``uint8_t[64]``.

* `Improve ergonomics of emboss SupportedCommands
  <https://pwrev.dev/227931>`__
* `Add more emboss definitions
  <https://pwrev.dev/227951>`__
  (issue `#42167863 <https://pwbug.dev/42167863>`__)

pw_bluetooth_proxy
------------------
* `Release active connections once zero
  <https://pwrev.dev/226400>`__
* `Only log disconnect events for active connections
  <https://pwrev.dev/226393>`__

pw_bluetooth_sapphire
---------------------
* `Handle switch warning with pigweed
  <https://pwrev.dev/227044>`__
  (issue `#355511476 <https://pwbug.dev/355511476>`__)
* `Handle switch warning with pigweed
  <https://pwrev.dev/227043>`__
  (issue `#355511476 <https://pwbug.dev/355511476>`__)
* `Handle switch warning with pigweed
  <https://pwrev.dev/227025>`__
  (issue `#355511476 <https://pwbug.dev/355511476>`__)
* `Write Variable PIN Type for Legacy Pairing
  <https://pwrev.dev/227042>`__
  (issues `#42173830 <https://pwbug.dev/42173830>`__,
  `# b/342151162 <https://pwbug.dev/ b/342151162>`__)
* `Add LegacyPairingState to BrEdrConnectionRequest
  <https://pwrev.dev/227041>`__
  (issue `#42173830 <https://pwbug.dev/42173830>`__)
* `Create and implement LegacyPairingState class
  <https://pwrev.dev/227023>`__
  (issues `#342150626 <https://pwbug.dev/342150626>`__,
  `#42173830 <https://pwbug.dev/42173830>`__)
* `Handle switch warning with pigweed
  <https://pwrev.dev/227040>`__
  (issue `#355511476 <https://pwbug.dev/355511476>`__)
* `Handle switch warning with pigweed
  <https://pwrev.dev/227039>`__
  (issue `#355511476 <https://pwbug.dev/355511476>`__)
* `Translate information & additional attributes
  <https://pwrev.dev/227038>`__
  (issue `#327758656 <https://pwbug.dev/327758656>`__)
* `Return registered services after bredr.Advertise
  <https://pwrev.dev/227022>`__
  (issue `#327758656 <https://pwbug.dev/327758656>`__)
* `Handle switch enum warning with pigweed
  <https://pwrev.dev/227037>`__
  (issue `#355511476 <https://pwbug.dev/355511476>`__)
* `Handle switch warning with pigweed
  <https://pwrev.dev/227021>`__
  (issue `#355511476 <https://pwbug.dev/355511476>`__)
* `Fix shadow variable warnings
  <https://pwrev.dev/227036>`__
  (issue `#355511476 <https://pwbug.dev/355511476>`__)
* `Add panic to EventTypeToString
  <https://pwrev.dev/227035>`__
  (issue `#356388419 <https://pwbug.dev/356388419>`__)
* `Switch over to pw::utf8
  <https://pwrev.dev/227020>`__
  (issue `#337305285 <https://pwbug.dev/337305285>`__)
* `Remove unnecessary cast qual pragma
  <https://pwrev.dev/227034>`__
  (issue `#355511476 <https://pwbug.dev/355511476>`__)
* `Fix statement expression extension warnings
  <https://pwrev.dev/227033>`__
  (issue `#355511476 <https://pwbug.dev/355511476>`__)
* `Fix variadic macro warnings
  <https://pwrev.dev/227019>`__
  (issue `#355511476 <https://pwbug.dev/355511476>`__)
* `Add SetupDataPath FIDL handler
  <https://pwrev.dev/227018>`__
  (issue `#311639690 <https://pwbug.dev/311639690>`__)
* `Create abstract base for IsoStream
  <https://pwrev.dev/227017>`__
* `Remove CommandPacketVariant
  <https://pwrev.dev/227032>`__
  (issue `#42167863 <https://pwbug.dev/42167863>`__)
* `Use emboss for setting ACL priority
  <https://pwrev.dev/227031>`__
  (issue `#42167863 <https://pwbug.dev/42167863>`__)
* `Add test for A2DP offloading
  <https://pwrev.dev/227014>`__
  (issue `#330921787 <https://pwbug.dev/330921787>`__)
* `Explicitly move WeakRef in GetWeakPtr
  <https://pwrev.dev/227013>`__
  (issue `#354026910 <https://pwbug.dev/354026910>`__)
* `Clean up some type sizes
  <https://pwrev.dev/227012>`__
  (issue `#354057871 <https://pwbug.dev/354057871>`__)
* `Use emboss for LELongTermKeyRequestReply
  <https://pwrev.dev/227011>`__
  (issue `#42167863 <https://pwbug.dev/42167863>`__)
* `Remove manufacturer list
  <https://pwrev.dev/226472>`__

pw_build
--------
* `Fix configs in pw_rust_executable
  <https://pwrev.dev/212171>`__
  (issue `#343111481 <https://pwbug.dev/343111481>`__)
* `Auto disable project builder progress bars
  <https://pwrev.dev/226379>`__

pw_chrono_stl
-------------
System clock and timer interfaces have been moved to separate directories so
that you can pick up the backend of one of these without bringing in the other.
This makes it possible to use a custom system timer with the STL system clock,
for example.

* `Move system clock and timer into separate directories
  <https://pwrev.dev/225992>`__

pw_clock_tree_mcuxpresso
------------------------
The new ``pw::clock_tree::ClockMcuxpressoClockIp`` class lets you manage
``clock_ip_name_t`` clocks with the clock tree to save power when
``FSL_SDK_DISABLE_DRIVE_CLOCK_CONTROL`` is set.

* `Introduce ClockMcuxpressoClockIp
  <https://pwrev.dev/226069>`__
  (issue `#355486338 <https://pwbug.dev/355486338>`__)

pw_console
----------
* `Additional UI and code themes
  <https://pwrev.dev/226720>`__
* `Bump version of console js, add titles to log panes
  <https://pwrev.dev/226831>`__

pw_digital_io
-------------
The new ``pw::digital_io::DigitalInOutMock`` class is a mock implementation
of ``pw:digital_io::DigitalInOut`` that can be used for testing.

* `Add Mock
  <https://pwrev.dev/227836>`__

pw_digital_io_linux
-------------------
The default ``pw_log`` logging level changed from ``DEBUG`` to ``INFO``.

* `Set log level to INFO
  <https://pwrev.dev/225912>`__

pw_digital_io_rp2040
--------------------
* `Don't discard status returns
  <https://pwrev.dev/227712>`__
  (issue `#357090965 <https://pwbug.dev/357090965>`__)

pw_env_setup
------------
* `Update rust thumbv7m target
  <https://pwrev.dev/226951>`__
* `Add rustc thumbv7m target
  <https://pwrev.dev/211991>`__
  (issue `#343111481 <https://pwbug.dev/343111481>`__)

pw_hex_dump
-----------
* `Add rule for pw_hex_dump/log_bytes.h
  <https://pwrev.dev/227651>`__
  (issue `#357595992 <https://pwbug.dev/357595992>`__)

pw_ide
------
The Pigweed extension for VS Code will now immediately update the code analysis
target if the ``pigweed.codeAnalysisTarget`` setting in ``settings.json`` is
changed. The new ``pigweed.activateBazeliskInNewTerminals`` setting lets you
specify whether the path to Bazelisk should be added when a VS Code terminal is
launched. The new :ref:`module-pw_ide-guide-vscode-code-intelligence` document
provides guides on using the VS Code extension's code intelligence features.

* `VSC extension 1.3.2 release
  <https://pwrev.dev/225391>`__
* `Show progress bar on manual refreshes
  <https://pwrev.dev/227731>`__
* `Detect manual target change in settings
  <https://pwrev.dev/227606>`__
* `Much faster VSC config parsing
  <https://pwrev.dev/227605>`__
* `Automatically activate Bazelisk in new terminals
  <https://pwrev.dev/226382>`__
* `Use stable clangd path
  <https://pwrev.dev/226431>`__
  (issue `#355655415 <https://pwbug.dev/355655415>`__)
* `Add inactive source file decoration
  <https://pwrev.dev/225733>`__
* `VSC extension refactoring
  <https://pwrev.dev/226059>`__

pw_multibuf
-----------
* `Add missing includes
  <https://pwrev.dev/227331>`__
  (issue `#356667663 <https://pwbug.dev/356667663>`__)

pw_multisink
------------
The new ``pw::multisink::Drain::GetUnreadEntriesCount()`` method is a
thread-safe way to return the number of unread entries in a drain's sink.

* `Interface to read entries count
  <https://pwrev.dev/226351>`__
  (issue `#355104976 <https://pwbug.dev/355104976>`__)

pw_presubmit
------------
* `Fix incl-lang when not at repo root
  <https://pwrev.dev/227185>`__
* `Remove misc program
  <https://pwrev.dev/226995>`__
  (issue `#356888002 <https://pwbug.dev/356888002>`__)
* `Rename "misc" program to "sapphire"
  <https://pwrev.dev/226993>`__
  (issue `#356888002 <https://pwbug.dev/356888002>`__)
* `Add new presubmit steps
  <https://pwrev.dev/226712>`__
  (issue `#356619766 <https://pwbug.dev/356619766>`__)
* `Allow fxbug.dev and crbug.com TODOs
  <https://pwrev.dev/226474>`__
* `Expose name of Bazel executable
  <https://pwrev.dev/226378>`__
  (issue `#355438774 <https://pwbug.dev/355438774>`__)

pw_router
---------
* `Add missing includes
  <https://pwrev.dev/227331>`__
  (issue `#356667663 <https://pwbug.dev/356667663>`__)

pw_rpc
------
* `Fix TypeScript client streaming return type
  <https://pwrev.dev/226717>`__
* `Add yield mode constraint_setting
  <https://pwrev.dev/226551>`__
  (issue `#345199579 <https://pwbug.dev/345199579>`__)

pw_rust
-------
The new ``pw_rust_USE_STD`` toolchain configuration option lets you control
whether the "std" feature should be used when building executables.

* `Add no_std build to basic_executable
  <https://pwrev.dev/211993>`__
  (issue `#343111481 <https://pwbug.dev/343111481>`__)
* `Rename host_executable
  <https://pwrev.dev/211992>`__
  (issue `#343111481 <https://pwbug.dev/343111481>`__)

pw_spi_mcuxpresso
-----------------
* `Add SPI_RxError() and SPI_TxError()
  <https://pwrev.dev/226992>`__

pw_status
---------
``PW_STATUS_CFG_CHECK_IF_USED`` now defaults to ``true`` in Bazel projects,
meaning that ``pw::Status`` objects returned from function calls must be used or
else a compilation error is raised. See :ref:`module-pw_status-reference-unused`.

* `In Bazel, make Status nodiscard
  <https://pwrev.dev/227411>`__
  (issue `#357090965 <https://pwbug.dev/357090965>`__)
* `Don't silently discard status returns
  <https://pwrev.dev/227277>`__
  (issue `#357090965 <https://pwbug.dev/357090965>`__)

pw_symbolizer
-------------
* `Add missing runfiles dep
  <https://pwrev.dev/226719>`__
  (issue `#355527449 <https://pwbug.dev/355527449>`__)
* `Get llvm-symbolizer path from Bazel
  <https://pwrev.dev/226254>`__
  (issue `#355527449 <https://pwbug.dev/355527449>`__)

pw_system
---------
The ``pw_system`` console now lets you control host log and device log levels
separately via the new ``--host-log-level`` and ``--device-log-level``
arguments. The new ``echo()``, ``reboot()``, and ``crash()`` methods of
``pw_system.device.Device`` make it easier to access these common
operations. Crash snapshots are now saved to ``/tmp`` (``C:\\TEMP`` on Windows)
by default. Crash detection logs are now printed across multiple lines to make
them easier to spot.

* `Separate host and device console log levels
  <https://pwrev.dev/227599>`__
* `Don't use implementation_deps
  <https://pwrev.dev/226977>`__
  (issues `#304374970 <https://pwbug.dev/304374970>`__,
  `#356667663 <https://pwbug.dev/356667663>`__)
* `Add console device aliases for common RPCs
  <https://pwrev.dev/226476>`__
* `Save snapshots to /tmp
  <https://pwrev.dev/226392>`__
* `Make crash detection logs louder
  <https://pwrev.dev/226373>`__

pw_tokenizer
------------
* `decode_optionally_tokenized without a Detokenizer
  <https://pwrev.dev/226727>`__

pw_toolchain
------------
* `Clang support for Arm Cortex-M33
  <https://pwrev.dev/228391>`__
  (issue `#358108912 <https://pwbug.dev/358108912>`__)
* `Register Cortex-M7 toolchain
  <https://pwrev.dev/227598>`__
* `Use \`crate_name\` for GN rust targets
  <https://pwrev.dev/223391>`__
* `proc_macro GN cross compile
  <https://pwrev.dev/215011>`__
* `Rustc cross compile to qemu-clang
  <https://pwrev.dev/211994>`__
  (issue `#343111481 <https://pwbug.dev/343111481>`__)
* `Expose a symlink to clangd
  <https://pwrev.dev/226262>`__
  (issue `#355655415 <https://pwbug.dev/355655415>`__)

pw_transfer
-----------
* `Add a delay after opening a stream to delay transfer start
  <https://pwrev.dev/225734>`__
  (issue `#355249134 <https://pwbug.dev/355249134>`__)
* `Use initial timeout when resending start chunks
  <https://pwrev.dev/226452>`__

pw_web
------
The log viewer now defaults to using line wrapping in table cells. The new
``logViews`` property makes it easier to customize each individual log view.
The ``severity`` field for controlling what types of logs to display has
been renamed to ``level``.

* `Add resize handler to message col
  <https://pwrev.dev/221433>`__
  (issue `#351901512 <https://pwbug.dev/351901512>`__)
* `Change word-wrap to default true and save to state
  <https://pwrev.dev/226730>`__
  (issue `#354283022 <https://pwbug.dev/354283022>`__)
* `NPM version bump to 0.0.22
  <https://pwrev.dev/226726>`__
* `Show REPL message at every run
  <https://pwrev.dev/226872>`__
* `NPM version bump to 0.0.21
  <https://pwrev.dev/226713>`__
* `Enable custom titles, log-view access
  <https://pwrev.dev/226771>`__
  (issue `#355272099 <https://pwbug.dev/355272099>`__)
* `Change severity to level
  <https://pwrev.dev/225573>`__
  (issue `#354282161 <https://pwbug.dev/354282161>`__)
* `Replace column menu with MWC components
  <https://pwrev.dev/226151>`__
  (issues `#354712931 <https://pwbug.dev/354712931>`__,
  `#342452087 <https://pwbug.dev/342452087>`__)

Docs
====
The new :ref:`Tour of Pigweed <showcase-sense>` is a hands-on, guided
walkthrough of many key Pigweed features working together in a medium-complexity
application.

* `Start Sense tutorial
  <https://pwrev.dev/220311>`__
* `Fix some bad links in the changelog
  <https://pwrev.dev/226811>`__
* `Update changelog
  <https://pwrev.dev/226251>`__

Rolls
=====
* `gn
  <https://pwrev.dev/227612>`__
* `fuchsia_infra 119 commits
  <https://pwrev.dev/227476>`__
* `cmake
  <https://pwrev.dev/227289>`__
* `rust
  <https://pwrev.dev/227287>`__
* `Clang
  <https://pwrev.dev/226725>`__
  (issue `#356689444 <https://pwbug.dev/356689444>`__)
* `bazel_skylib
  <https://pwrev.dev/226979>`__
* `rust
  <https://pwrev.dev/226257>`__

Third-party software
====================

Emboss
------
* `Append public_deps in GN template
  <https://pwrev.dev/227062>`__

ICU
---
* `Remove ICU
  <https://pwrev.dev/228234>`__

STM32Cube
---------
* `bzlmod-friendly changes
  <https://pwrev.dev/226479>`__
  (issue `#258836641 <https://pwbug.dev/258836641>`__)

Miscellaneous
-------------
* `Run 'pw format --fix'
  <https://pwrev.dev/227186>`__

------------
Jul 25, 2024
------------
Highlights (Jul 12, 2024 to Jul 25, 2024):

* **Extensive Bazel support in the Pigweed extension for VS Code**:
  See :ref:`docs-changelog-20240725-pw_ide` for the full story.
* **Bazel module support**: Upstream Pigweed is now usable as a `Bazel
  module <https://bazel.build/external/module>`_ dependency.
* **Trapping backend for pw_assert**: :ref:`module-pw_assert_trap` is a
  new backend for :ref:`module-pw_assert` that calls ``__builtin_trap()``
  when an assert is triggered.
* **Crash handling in async pw_system**: Crash snapshots can be downloaded
  from ``pw_console`` by calling ``device.get_crash_snapshots()`` in the
  REPL.

Build systems
=============

Bazel
-----
Toolchain registration moved from ``WORKSPACE`` to ``MODULE.bazel``, making
it possible for downstream projects to use upstream Pigweed's toolchains
directly. Pigweed is now usable as a Bazel module dependency. Pigweed now
provides an example ``.bazelrc`` at ``//pw_build/pigweed.bazelrc`` that
downstream users can copy into their own projects.

* `Remove @pigweed from bzl files
  <https://pwrev.dev/226007>`__
  (issue `#352389854 <https://pwbug.dev/352389854>`__)
* `Remove stray @pigweed in load statement
  <https://pwrev.dev/226006>`__
  (issue `#352389854 <https://pwbug.dev/352389854>`__)
* `Move toolchain registration to MODULE.bazel
  <https://pwrev.dev/225471>`__
  (issue `#258836641 <https://pwbug.dev/258836641>`__)
* `Enable bzlmod
  <https://pwrev.dev/211362>`__
  (issue `#258836641 <https://pwbug.dev/258836641>`__)
* `Remove sanitizers from default program
  <https://pwrev.dev/223572>`__
  (issue `#301487567 <https://pwbug.dev/301487567>`__)
* `Modernize pip deps style (2)
  <https://pwrev.dev/224316>`__
* `Organize and document required flags
  <https://pwrev.dev/223817>`__
  (issue `#353750350 <https://pwbug.dev/353750350>`__)
* `Use Python toolchain in custom rules
  <https://pwrev.dev/224298>`__
  (issue `#258836641 <https://pwbug.dev/258836641>`__)
* `Use Python toolchain in custom rules
  <https://pwrev.dev/224272>`__
  (issue `#258836641 <https://pwbug.dev/258836641>`__)
* `Use Python toolchain in custom rules
  <https://pwrev.dev/224272>`__
  (issue `#258836641 <https://pwbug.dev/258836641>`__)
* `Modernize pip deps style
  <https://pwrev.dev/223871>`__
  (issue `#258836641 <https://pwbug.dev/258836641>`__)
* `Create separate "sanitizers" CI program
  <https://pwrev.dev/223595>`__
  (issue `#301487567 <https://pwbug.dev/301487567>`__)
* `Run under tsan, ubsan in presubmit
  <https://pwrev.dev/223631>`__
  (issue `#301487567 <https://pwbug.dev/301487567>`__)
* `Run tests with asan in CQ
  <https://pwrev.dev/222792>`__
  (issue `#301487567 <https://pwbug.dev/301487567>`__)

Docs
====
The fonts on ``pigweed.dev`` have been updated.

* `Prefer "change" to Google-specific "CL"
  <https://pwrev.dev/226004>`__
* `Minor changelog update
  <https://pwrev.dev/224271>`__
* `Update pigweed.dev fonts
  <https://pwrev.dev/223591>`__
  (issue `#353530954 <https://pwbug.dev/353530954>`__)
* `Minor updates
  <https://pwrev.dev/223571>`__
* `Update changelog
  <https://pwrev.dev/222831>`__

Modules
=======

pw_assert_trap
--------------
:ref:`module-pw_assert_trap` is a new backend for :ref:`module-pw_assert`
that calls ``__builtin_trap()`` when an assert is triggered.

* `Add a new assert backend which traps on assert
  <https://pwrev.dev/220135>`__
  (issues `#351888988 <https://pwbug.dev/https://pwbug.dev/351888988>`__,
  `#351886597 <https://pwbug.dev/https://pwbug.dev/351886597>`__)

pw_async2
---------
The new :cpp:class:`pw::async2::CoroOrElseTask` class lets you run a coroutine
in a task and invokes a handler function on error. The new
:cpp:func:`pw::async2::Task::IsRegistered` method checks if a task is
currently registered with a dispatcher. The new
:cpp:func:`pw::async2::Coro::Empty()` method creates an empty, invalid
coroutine object. The new :cpp:func:`pw::async2::Task::Deregister` method
unlinks a task from a dispatcher and any associated waker values.

* `Fix minor doc issues
  <https://pwrev.dev/226111>`__
* `Add CoroOrElseTask
  <https://pwrev.dev/225778>`__
* `Add Task::IsRegistered
  <https://pwrev.dev/225995>`__
* `Add Coro::Empty
  <https://pwrev.dev/225993>`__
* `Add Task::Deregister
  <https://pwrev.dev/225775>`__

pw_async2_epoll
---------------
* `Fix block on racing wakeups
  <https://pwrev.dev/224291>`__

pw_bluetooth
------------
* `Generate emboss headers in Soong
  <https://pwrev.dev/225152>`__
  (issue `#352364622 <https://pwbug.dev/352364622>`__)
* `Add generic HCI command definition
  <https://pwrev.dev/224931>`__
  (issue `#42167863 <https://pwbug.dev/42167863>`__)

pw_bluetooth_proxy
------------------
* `Create Soong library for pw_bluetooth_proxy
  <https://pwrev.dev/225153>`__
  (issue `#352393966 <https://pwbug.dev/352393966>`__)
* `Update emboss deps
  <https://pwrev.dev/224991>`__
* `Do not pass on NOCP events without credits
  <https://pwrev.dev/224434>`__
  (issue `#353546115 <https://pwbug.dev/353546115>`__)
* `Have functions handle passing on the packet
  <https://pwrev.dev/224433>`__
  (issue `#353546115 <https://pwbug.dev/353546115>`__)
* `Update tests to verify number of sent packets
  <https://pwrev.dev/224300>`__
* `Remove debugging log
  <https://pwrev.dev/224032>`__
  (issue `#353546115 <https://pwbug.dev/353546115>`__)

pw_bluetooth_sapphire
---------------------
The latest ``pw_bluetooth_sapphire`` commits were brought
into Pigweed from the Fuchsia repository.

* `Reduce scope of security_manager
  <https://pwrev.dev/225036>`__
  (issue `#337315598 <https://pwbug.dev/337315598>`__)
* `Cleanup BrEdrConnectionManager test file
  <https://pwrev.dev/225035>`__
* `Fix integer conversion warnings
  <https://pwrev.dev/225034>`__
  (issue `#354057871 <https://pwbug.dev/354057871>`__)
* `Cleanup WritePageScanType into shared test file
  <https://pwrev.dev/225033>`__
* `Add missing optional include
  <https://pwrev.dev/225051>`__
  (issue `#313665184 <https://pwbug.dev/313665184>`__)
* `Remove unused method
  <https://pwrev.dev/224043>`__
  (issue `#42167863 <https://pwbug.dev/42167863>`__)
* `Remove variant from LE SendCommands
  <https://pwrev.dev/224042>`__
  (issue `#42167863 <https://pwbug.dev/42167863>`__)
* `Remove CommandPacketVariant
  <https://pwrev.dev/224041>`__
  (issue `#42167863 <https://pwbug.dev/42167863>`__)
* `Remove variant from QueueCommand
  <https://pwrev.dev/224040>`__
  (issue `#42167863 <https://pwbug.dev/42167863>`__)
* `Remove variant from QueueLeAsyncCommand
  <https://pwrev.dev/224112>`__
  (issue `#42167863 <https://pwbug.dev/42167863>`__)
* `Create IsoDataChannel
  <https://pwrev.dev/224039>`__
  (issue `#311639040 <https://pwbug.dev/311639040>`__)
* `Check legacy adv for rand addr in FakeController
  <https://pwrev.dev/224038>`__
  (issue `#42161900 <https://pwbug.dev/42161900>`__)
* `Use platform-independent format strings
  <https://pwrev.dev/224096>`__
  (issue `#313665184 <https://pwbug.dev/313665184>`__)
* `Processing of CIS Established event
  <https://pwrev.dev/224037>`__
  (issue `#311639432 <https://pwbug.dev/311639432>`__)
* `Use pw_bytes for endianness conversions
  <https://pwrev.dev/224036>`__
  (issue `#313665184 <https://pwbug.dev/313665184>`__)
* `Cleanup WritePageScanActivity
  <https://pwrev.dev/224095>`__
* `Cleanup into shared test file
  <https://pwrev.dev/224035>`__
* `Cleanup WriteLocalNameResponse to shared file
  <https://pwrev.dev/224094>`__
* `Fix camel case for CIS acronym
  <https://pwrev.dev/224034>`__
* `Cleanup Inquiry command packets into shared file
  <https://pwrev.dev/224033>`__
* `Implement AcceptCis()
  <https://pwrev.dev/224093>`__
  (issue `#311639432 <https://pwbug.dev/311639432>`__)
* `Use pw_bytes for endianness conversions
  <https://pwrev.dev/221311>`__
  (issue `#313665184 <https://pwbug.dev/313665184>`__)
* `Use pw_bytes for endianness conversions
  <https://pwrev.dev/221250>`__
  (issue `#313665184 <https://pwbug.dev/313665184>`__)
* `Reorganize test_packets.h/.cc files
  <https://pwrev.dev/221249>`__
* `Cleanup WriteInquiryScanActivity packets
  <https://pwrev.dev/221266>`__
* `Use pw_bytes for endianness conversions
  <https://pwrev.dev/221265>`__
  (issue `#313665184 <https://pwbug.dev/313665184>`__)
* `Use pw_bytes for endianness conversions
  <https://pwrev.dev/221264>`__
  (issue `#313665184 <https://pwbug.dev/313665184>`__)
* `Use pw_bytes for endianness conversions
  <https://pwrev.dev/221248>`__
  (issue `#313665184 <https://pwbug.dev/313665184>`__)
* `Remove unneeded #include
  <https://pwrev.dev/221262>`__
  (issue `#313665184 <https://pwbug.dev/313665184>`__)
* `Use pw_bytes for endianness conversions
  <https://pwrev.dev/221261>`__
  (issue `#313665184 <https://pwbug.dev/313665184>`__)
* `Use pw_bytes for endianness conversions
  <https://pwrev.dev/221247>`__
  (issue `#313665184 <https://pwbug.dev/313665184>`__)
* `Use pw_bytes for endianness conversions
  <https://pwrev.dev/221246>`__
  (issue `#313665184 <https://pwbug.dev/313665184>`__)
* `Use pw_bytes for endianness conversions
  <https://pwrev.dev/221245>`__
  (issue `#313665184 <https://pwbug.dev/313665184>`__)
* `Rename link_initiated to outgoing_connection
  <https://pwrev.dev/221244>`__
* `Cleanup WriteScanEnable packets into shared file
  <https://pwrev.dev/221243>`__
* `Cleanup ReadScanEnable packets into shared file
  <https://pwrev.dev/221260>`__
* `Remove alias for BrEdrConnectionRequest
  <https://pwrev.dev/221242>`__
* `BrEdrConnectionRequest create HCI connection req
  <https://pwrev.dev/221241>`__
* `Add connection role to fake controller
  <https://pwrev.dev/221258>`__
  (issue `#311639432 <https://pwbug.dev/311639432>`__)
* `Remove double std::move in PairingStateManager
  <https://pwrev.dev/221238>`__
* `Fix BrEdrDynamicChannel crash
  <https://pwrev.dev/221257>`__
  (issue `#42076625 <https://pwbug.dev/42076625>`__)
* `Cleanup BrEdrConnectionRequest and Manager
  <https://pwrev.dev/221237>`__
* `Implement IsoStreamServer
  <https://pwrev.dev/221256>`__
  (issue `#311639275 <https://pwbug.dev/311639275>`__)
* `Modernize fuchsia.hardware.bluetooth.Peer API
  <https://pwrev.dev/221255>`__
  (issue `#330591131 <https://pwbug.dev/330591131>`__)
* `Rename to secure_simple_pairing_state
  <https://pwrev.dev/221236>`__
  (issue `#342150626 <https://pwbug.dev/342150626>`__)
* `Use weak hci::BrEdrConnection pointer
  <https://pwrev.dev/221253>`__
* `Create and use PairingStateManager class
  <https://pwrev.dev/221252>`__
  (issues `#342150626 <https://pwbug.dev/342150626>`__,
  `#42173830 <https://pwbug.dev/42173830>`__)
* `Implement GetCodecLocalDelayRange
  <https://pwrev.dev/221251>`__
  (issue `#311639690 <https://pwbug.dev/311639690>`__)
* `Add Bazel rules for FIDL layer
  <https://pwrev.dev/221190>`__
  (issue `#324105856 <https://pwbug.dev/324105856>`__)
* `Refactor ScoConnection
  <https://pwrev.dev/221189>`__
  (issue `#330590954 <https://pwbug.dev/330590954>`__)
* `Fix max connection event length value
  <https://pwrev.dev/221188>`__
  (issue `#323255182 <https://pwbug.dev/323255182>`__)
* `Add AdvertisingData.ToString
  <https://pwrev.dev/221187>`__
  (issue `#42157647 <https://pwbug.dev/42157647>`__)
* `Make UUID string parsing optional
  <https://pwrev.dev/221234>`__
  (issue `#339726884 <https://pwbug.dev/339726884>`__)
* `Add Adapter::GetSupportedDelayRange
  <https://pwrev.dev/221233>`__
  (issue `#311639690 <https://pwbug.dev/311639690>`__)
* `Clean up legacy advertising report parsing API
  <https://pwrev.dev/221185>`__
  (issue `#308500308 <https://pwbug.dev/308500308>`__)
* `Add missing climits include
  <https://pwrev.dev/221183>`__
  (issue `#338408169 <https://pwbug.dev/338408169>`__)
* `Add Emboss support to SendCommand completion
  <https://pwrev.dev/221182>`__
  (issue `#311639690 <https://pwbug.dev/311639690>`__)
* `Improve naming of android namespace aliases
  <https://pwrev.dev/221232>`__
  (issue `#335491380 <https://pwbug.dev/335491380>`__)
* `Migrate emboss aliases to new names
  <https://pwrev.dev/221181>`__
  (issue `#338068316 <https://pwbug.dev/338068316>`__)
* `Add tests for LE Read Max. Adv. Data Length
  <https://pwrev.dev/221180>`__
  (issue `#338058140 <https://pwbug.dev/338058140>`__)
* `Ensure command is supported before issuing it
  <https://pwrev.dev/221178>`__
  (issue `#338058140 <https://pwbug.dev/338058140>`__)
* `Use duration_cast for constants
  <https://pwrev.dev/221029>`__
  (issue `#337928450 <https://pwbug.dev/337928450>`__)
* `Implement extended adv. pdus with fragmentation
  <https://pwrev.dev/221177>`__
  (issue `#312898345 <https://pwbug.dev/312898345>`__,
  `#309013696 <https://pwbug.dev/309013696>`__)
* `Use \`ull\` constants for enums
  <https://pwrev.dev/221028>`__
  (issue `#337928450 <https://pwbug.dev/337928450>`__)
* `Update semantics of \`bredr.Advertise\`
  <https://pwrev.dev/221176>`__
  (issues `#330590954 <https://pwbug.dev/330590954>`__,
  `#327758656 <https://pwbug.dev/327758656>`__)
* `Add the credit-based flow control RxEngine
  <https://pwrev.dev/221175>`__
* `Migrate LEAdvertisingReportSubevent to Emboss
  <https://pwrev.dev/221174>`__
  (issue `#86811 <https://pwbug.dev/86811>`__)
* `Lenient LEGetVendorCapabilitiesCommandComplete
  <https://pwrev.dev/221027>`__
  (issues `#337947318 <https://pwbug.dev/337947318>`__,
  `#332924521 <https://pwbug.dev/332924521>`__,
  `#332924195 <https://pwbug.dev/332924195>`__)
* `Migrate FIDL to bt::testing::TestLoopFixture
  <https://pwrev.dev/221172>`__
  (issue `#324105856 <https://pwbug.dev/324105856>`__)
* `Fix all available lint errors
  <https://pwrev.dev/221171>`__
* `Move LinkKey simple constructors to the h file
  <https://pwrev.dev/221025>`__
* `Add operator!= for LinkKey
  <https://pwrev.dev/221049>`__
* `Add extra diagnostics
  <https://pwrev.dev/221048>`__
* `Improve management of advertising modes
  <https://pwrev.dev/221045>`__
  (issue `#309013696 <https://pwbug.dev/309013696>`__)
* `Add Bazel rules for socket library
  <https://pwrev.dev/221024>`__
  (issue `#324105856 <https://pwbug.dev/324105856>`__)
* `Update LowEnergyAdvertiser to use std::vector
  <https://pwrev.dev/221023>`__
  (issue `#312898345 <https://pwbug.dev/312898345>`__,
  `#309013696 <https://pwbug.dev/309013696>`__)
* `Add more values to LEEventMask
  <https://pwrev.dev/221022>`__
* `Pass hci::AdvertisingIntervalRange as const
  <https://pwrev.dev/221044>`__
* `Shorten pw::bluetooth::emboss to pwemb
  <https://pwrev.dev/221021>`__
* `Pass extended_pdu booleans through the stack
  <https://pwrev.dev/221043>`__
  (issue `#312898345 <https://pwbug.dev/312898345>`__,
  `#309013696 <https://pwbug.dev/309013696>`__)
* `Update AdvertisingHandleMap for extended PDUs
  <https://pwrev.dev/221020>`__
  (issue `#312898345 <https://pwbug.dev/312898345>`__,
  `#309013696 <https://pwbug.dev/309013696>`__)
* `Update missing header
  <https://pwrev.dev/221019>`__
  (issue `#331673100 <https://pwbug.dev/331673100>`__)
* `Add the credit-based flow control TxEngine
  <https://pwrev.dev/221017>`__
* `Prevent protected member access in TxEngine
  <https://pwrev.dev/221016>`__
* `Add Bazel tests
  <https://pwrev.dev/221015>`__
* `Fix conversions from iterators to raw pointers
  <https://pwrev.dev/221014>`__
  (issue `#328282937 <https://pwbug.dev/328282937>`__)
* `Add bt-host Bazel tests
  <https://pwrev.dev/221042>`__
  (issue `#324105856 <https://pwbug.dev/324105856>`__)
* `Low energy advertiser general cleanup
  <https://pwrev.dev/221041>`__
* `Use using aliases for Emboss type references
  <https://pwrev.dev/221012>`__
* `Add Bazel build files
  <https://pwrev.dev/221036>`__
  (issue `#324105856 <https://pwbug.dev/324105856>`__)

pw_build
--------
``pw_py_binary`` is a new wrapper for ``py_binary`` that provides some
defaults, such as marking all Python binaries as incompatible with MCUs.

* `Introduce pw_py_binary
  <https://pwrev.dev/224296>`__
  (issue `#258836641 <https://pwbug.dev/258836641>`__)
* `Use incompatible_with_mcu in pw_py_test
  <https://pwrev.dev/224294>`__

pw_channel
----------
The new :cpp:class:`pw::channel::StreamChannel` adapter makes it easier for
a channel to interact with an underlying reader and writer stream.

* `Add StreamChannel adapter
  <https://pwrev.dev/225651>`__

pw_chrono_freertos
------------------
* `Work around no std::unique_lock in baremetal libc++
  <https://pwrev.dev/223636>`__
  (issue `#353601672 <https://pwbug.dev/353601672>`__)

pw_chrono_stl
-------------
* `Consolidate SystemTimer into a single thread
  <https://pwrev.dev/224295>`__

pw_console
----------
Typing out RPCs to invoke from the web console REPL should now autocomplete
as expected. The web console log viewer now has a default config, defined
at ``//pw_console/py/pw_console/html/defaultconfig.json``. A new boolean
config option, ``recolor_log_lines_to_match_level``, has been added to
allow users to control whether log messages should be restyled to match
their severity level.

* `Fix RPC autocompletion in web kernel
  <https://pwrev.dev/225935>`__
* `Set a default config for web console's log viewer
  <https://pwrev.dev/225751>`__
* `Pass rpc completions to web_kernel
  <https://pwrev.dev/224311>`__
* `Add config option for log message recoloring
  <https://pwrev.dev/224475>`__
* `Default WebHandler.kernel_params to an empty dictionary
  <https://pwrev.dev/223932>`__
* `Handle web logging in a separate thread
  <https://pwrev.dev/223691>`__
* `Cleanup web kernel on page close
  <https://pwrev.dev/223178>`__
* `Replace placeholder page with real web console
  <https://pwrev.dev/223155>`__

pw_cpu_exception_cortex_m
-------------------------
The new ``PW_CPU_EXCEPTION_CORTEX_M_CRASH_ANALYSIS_INCLUDE_PC_LR``
option lets you control whether PC and LR register values are included
in the ``AnalyzeCpuStateAndCrash()`` analysis.

* `Make PC LR optional
  <https://pwrev.dev/221731>`__
* `Fix assembly
  <https://pwrev.dev/223131>`__
  (issue `#261603269 <https://pwbug.dev/261603269>`__)
* `Temporarily disable tests on Cortex-M33
  <https://pwrev.dev/223594>`__
  (issues `#353533678 <https://pwbug.dev/353533678>`__,
  `#323215726 <https://pwbug.dev/323215726>`__)

pw_crypto
---------
* `Don't build micro-ecc
  <https://pwrev.dev/223152>`__
  (issue `#261603269 <https://pwbug.dev/261603269>`__)

pw_env_setup
------------
* `Add luci-cv to environment
  <https://pwrev.dev/222811>`__

pw_grpc
-------
* `Fix shadowed variable warning
  <https://pwrev.dev/225931>`__

.. _docs-changelog-20240725-pw_ide:

pw_ide
------
General updates: ``pw_ide`` now detects Bazel projects based on the presence
of ``MODULE.bazel`` files. The :ref:`module-pw_ide` docs have been revamped.
Shared VS Code settings can now be stored in ``.vscode/settings.shared.json``.
The extension no longer attempts to infer the working directory if a project
root isn't explicitly provided; instead it prompts users to manually specify
the project root. The VS Code extension now has better support for Fish
terminals.

VS Code extension updates related to code intelligence in Bazel-based projects:
The new ``Pigweed: Select Code Analysis Target`` command controls which Bazel
target in your project to use for code intelligence. The new ``Pigweed: Refresh
Compile Commands`` command lets you manually refresh code intelligence data.
There's also a ``Pigweed: Refresh Compile Commands and Select Code Analysis
Target`` that combines these two steps. In the VS Code status bar there's a new
icon to indicate whether ``clangd`` code intelligence is on or off. Code
intelligence data compilation output is now streamed so that you can monitor
the progress of the tool in real-time. The easiest way to access these logs is
by running the new ``Pigweed: Open Output Panel`` command. The extension now
supports a ``.clangd.shared`` file that can be used to control project-wide
``clangd`` settings.

VS Code extension updates related to other Bazel tools: The extension now
bundles Bazelisk and Buildifier. These bundled versions get updated when the
extension itself updates. The extension recommends users to use these bundled
versions by default. You can use the new ``Activate Bazelisk in Terminal``
command to manually specify which Bazelisk version to use.

* `Support bzlmod projects
  <https://pwrev.dev/225913>`__
* `Block on spawned refresh process
  <https://pwrev.dev/225731>`__
* `Status bar item for inactive file disabling
  <https://pwrev.dev/225392>`__
* `Support disabling clangd for inactive files
  <https://pwrev.dev/224893>`__
* `Update vendored tools on extension update
  <https://pwrev.dev/225934>`__
* `Use vendored Bazelisk in recommended config
  <https://pwrev.dev/225933>`__
* `Improve VSC settings interface
  <https://pwrev.dev/225932>`__
* `Add shared settings management
  <https://pwrev.dev/224573>`__
* `Associate target groups with active files
  <https://pwrev.dev/222735>`__
* `Don't unnecessarily infer working dir
  <https://pwrev.dev/224572>`__
* `VSC extension 1.1.1 release
  <https://pwrev.dev/224897>`__
* `Add missing command stubs
  <https://pwrev.dev/224892>`__
* `Stream refresh compile commands output
  <https://pwrev.dev/224313>`__
* `Update dev build configs
  <https://pwrev.dev/224312>`__
* `Add fish to Bazelisk+VSC terminal
  <https://pwrev.dev/224292>`__
* `VSC extension 1.1.0 release
  <https://pwrev.dev/224171>`__
* `Patch Bazelisk into VSC terminal
  <https://pwrev.dev/223823>`__
* `VSC extension 1.0.0 release
  <https://pwrev.dev/223911>`__
* `Revise docs
  <https://pwrev.dev/223157>`__
* `Update VSC extension packaging
  <https://pwrev.dev/223634>`__
* `Add proto extension as dependency
  <https://pwrev.dev/223156>`__
* `Fix VSC troubleshooting links
  <https://pwrev.dev/223576>`__
* `Update compile commands tool version
  <https://pwrev.dev/222575>`__
* `Don't show root comp DB dir as target
  <https://pwrev.dev/223633>`__
* `VSC status bar item for target selection
  <https://pwrev.dev/220134>`__
* `Add IDE support refresh manager
  <https://pwrev.dev/219973>`__
* `Integrate Bazelisk in VSC
  <https://pwrev.dev/219971>`__
* `Bazel comp DB management in VSC
  <https://pwrev.dev/218832>`__
* `VSC/JS project management
  <https://pwrev.dev/222734>`__
* `Create VSC settings interface
  <https://pwrev.dev/222733>`__
* `Use VSC output panel for logging
  <https://pwrev.dev/222732>`__
* `VSC extension cleanup
  <https://pwrev.dev/222731>`__

pw_kvs
------
* `Missing <string> includes
  <https://pwrev.dev/223352>`__
  (issue `#298822102 <https://pwbug.dev/298822102>`__)
* `Update bazel config
  <https://pwrev.dev/223171>`__

pw_libcxx
---------
``pw_libcxx`` now has support for the ``new`` operator.

* `Actually implement operator delete
  <https://pwrev.dev/223692>`__
* `Add operator new
  <https://pwrev.dev/223632>`__

pw_log
------
* `Log decoder timestamp cleanup
  <https://pwrev.dev/223271>`__
* `Fix log_decoder timestamp formatting
  <https://pwrev.dev/222771>`__
  (issue `#351905996 <https://pwbug.dev/351905996>`__)

pw_multibuf
-----------
* `Add more context on chunk regions
  <https://pwrev.dev/222431>`__

pw_presubmit
------------
``pw_presubmit.inclusive_language.check_file`` now accepts an optional
``check_path`` argument which controls whether to check the path for
non-inclusive language.

* `Add attributes to docstring
  <https://pwrev.dev/223095>`__
* `Allow disabling checks in unit tests
  <https://pwrev.dev/223577>`__
  (issue `#352515663 <https://pwbug.dev/352515663>`__)
* `No copyright check in .vscodeignore
  <https://pwrev.dev/223575>`__
* `Skip commit message check for merges
  <https://pwrev.dev/223574>`__
* `Add test for inclusive language check
  <https://pwrev.dev/222311>`__
  (issue `#352515663 <https://pwbug.dev/352515663>`__)

pw_result
---------
* `Fix docs.rst example
  <https://pwrev.dev/223251>`__
* `Add missing header
  <https://pwrev.dev/223176>`__
  (issue `#261603269 <https://pwbug.dev/261603269>`__)
* `Missing <string> includes
  <https://pwrev.dev/223352>`__
  (issue `#298822102 <https://pwbug.dev/298822102>`__)

pw_rpc
------
Soong proto building is now more flexible; genrules can now have protos from
different sources. ``.option`` files can now be used in Soong genrules sources.

* `Build proto path arg list for Soong
  <https://pwrev.dev/225031>`__
* `Pass .proto files to compiler in Soong
  <https://pwrev.dev/222737>`__

pw_rpc_transport
----------------
* `Don't write empty header in StreamRpcFrameSender
  <https://pwrev.dev/220211>`__

pw_sensor
---------
* `Add units to sensor::channels final output
  <https://pwrev.dev/224711>`__
  (issue `#293466822 <https://pwbug.dev/293466822>`__)
* `Implement attributes, channels, triggers, and units
  <https://pwrev.dev/204199>`__
  (issue `#293466822 <https://pwbug.dev/293466822>`__)

pw_stream
---------
* `Update bazel config
  <https://pwrev.dev/223171>`__

pw_stream_uart_mcuxpresso
-------------------------
* `Check init state in Deinit()
  <https://pwrev.dev/224031>`__

pw_string
---------
The new :cpp:func:`pw::utf8::ReadCodePoint`,
:cpp:func:`pw::utf8::EncodeCodePoint`, and
:cpp:func:`pw::utf8::WriteCodePoint` methods provide basic UTF-8 decoding
and encoding.

* `Add utf_codecs
  <https://pwrev.dev/222738>`__
  (issue `#337305285 <https://pwbug.dev/337305285>`__)
* `Disable wchar test for libcpp
  <https://pwrev.dev/223581>`__
  (issue `#353604434 <https://pwbug.dev/353604434>`__)

pw_sync
-------
The condition variable interface has been deprecated. See
:ref:`module-pw_sync-condition-variables`.

* `Document that CV should not be used
  <https://pwrev.dev/162771>`__
  (issue `#294395229 <https://pwbug.dev/294395229>`__)

pw_system
---------
Async ``pw_system`` now supports a crash handling service. When a crash
snapshot is available, ``pw_system`` now logs instructions on how to download
it. The ``pw_system`` console API now accepts an optional
``device_connection`` object, allowing for more flexible connection
management. Device class creation has been refactored to make it easier for
Python scripts to setup connections to devices in the same way that the
``pw_system`` console does. As part of this refactor the ``--output``
and ``--proto-globs`` flags were removed from the ``pw_system`` console.

* `Enable crash handler in async system
  <https://pwrev.dev/225911>`__
* `Improve message when crash snapshot exists
  <https://pwrev.dev/225851>`__
  (issue `#354767156 <https://pwbug.dev/354767156>`__)
* `Add crash handling and device service
  <https://pwrev.dev/224299>`__
  (issue `#350807773 <https://pwbug.dev/350807773>`__)
* `Make pw_system_console work for rp2040
  <https://pwrev.dev/224714>`__
  (issue `#354203490 <https://pwbug.dev/354203490>`__)
* `Console device connection override
  <https://pwrev.dev/223173>`__
* `Add synchronization to pw::System allocator
  <https://pwrev.dev/222794>`__
  (issues `#352592037 <https://pwbug.dev/352592037>`__,
  `#352818465 <https://pwbug.dev/352818465>`__)
* `Reusable DeviceConnection functionality
  <https://pwrev.dev/221752>`__

pw_target_runner
----------------
* `Increase maximum message size for binaries
  <https://pwrev.dev/222736>`__

pw_thread
---------
The new ``pw::thread::TestThreadContext()`` interface makes it easier to
create threads for unit tests.

* `Add TestThreadContext for FreeRTOS
  <https://pwrev.dev/222671>`__

pw_tokenizer
------------
* `Missing <string> includes
  <https://pwrev.dev/223352>`__
  (issue `#298822102 <https://pwbug.dev/298822102>`__)

pw_toolchain
------------
The new ``minimum_cxx_20()`` Bazel helper can be used with
``target_compatible_with`` attributes to express that a target
requires C++20 or newer.

* `Select Bazel C++ version with config_setting
  <https://pwrev.dev/221453>`__
  (issue `#352379527 <https://pwbug.dev/352379527>`__)
* `Hide toolchain path behind variable
  <https://pwrev.dev/224851>`__
  (issue `#258836641 <https://pwbug.dev/258836641>`__)
* `Add linux_sysroot.bzl
  <https://pwrev.dev/223578>`__
  (issue `#258836641 <https://pwbug.dev/258836641>`__)
* `Host clang toolchain for Bazel
  <https://pwrev.dev/223172>`__
* `Pico Bazel build
  <https://pwrev.dev/223312>`__
  (issue `#261603269 <https://pwbug.dev/261603269>`__)
* `Add tsan support for host builds
  <https://pwrev.dev/222891>`__
  (issue `#301487567 <https://pwbug.dev/301487567>`__)
* `Add ubsan support for host builds
  <https://pwrev.dev/222791>`__

pw_trace_tokenized
------------------
* `Fix bazel build
  <https://pwrev.dev/223093>`__
  (issues `#260641850 <https://pwbug.dev/issues/260641850>`__,
  `#258071921 <https://pwbug.dev/issues/258071921>`__)

pw_transfer
-----------
The new ``PW_TRANSFER_EVENT_PROCESSING_TIMEOUT_MS`` lets you control how
long incoming transfer events should block on the previous event being
processed before dropping the new event.

* `Don't block indefinitely on events
  <https://pwrev.dev/224693>`__
* `Fix initial timeout and missing start chunk
  <https://pwrev.dev/222511>`__

pw_unit_test
------------
* `Removed duplicate from forwarded variables list in pw_test
  <https://pwrev.dev/223431>`__
* `Update logging_main compatibility
  <https://pwrev.dev/223579>`__
* `Missing <string> includes
  <https://pwrev.dev/223352>`__
  (issue `#298822102 <https://pwbug.dev/298822102>`__)
* `Introduce googtest_platform
  <https://pwrev.dev/222812>`__
  (issue `#352808542 <https://pwbug.dev/352808542>`__)

pw_web
------
The REPL in the web console now provides a welcome message that lists commonly
used keyboard shortcuts. The log viewer UI is now more dense. The REPL is now
positioned on the left by default. Pressing :kbd:`Shift+Enter` in the REPL
now goes to a new line rather than evaluating.

* `Add icon for info
  <https://pwrev.dev/225413>`__
  (issue `#354282161 <https://pwbug.dev/354282161>`__)
* `Fix keyboard shortcut in repl.rst
  <https://pwrev.dev/226003>`__
* `Add repl intro message and title param
  <https://pwrev.dev/225757>`__
  (issue `#354283703 <https://pwbug.dev/354283703>`__)
* `Increase default log viewer density
  <https://pwrev.dev/225671>`__
  (issues `# 354282977 <https://pwbug.dev/ 354282977>`__,
  `# 342451299 <https://pwbug.dev/ 342451299>`__)
* `Move REPL to left, reduce default division to 40%
  <https://pwrev.dev/225755>`__
* `Fix bug in code editor to not eval empty snippet
  <https://pwrev.dev/224315>`__
* `NPM version bump to 0.0.20
  <https://pwrev.dev/223154>`__
* `Change repl keybinding to eval
  <https://pwrev.dev/223174>`__
* `Bundle console.ts, fix log source handlers
  <https://pwrev.dev/222534>`__
* `Save state on input change and adjust filter logs logic
  <https://pwrev.dev/222774>`__
  (issue `#235253336 <https://pwbug.dev/235253336>`__)
* `Add filter field buttons and placeholder
  <https://pwrev.dev/222711>`__

Third-party software
====================

Emboss
------
Emboss was updated to v2024.0718.173957. Emboss build steps that involve
Python now use an optimized version of Python, resulting in a 15% speedup.

* `Disable using pw_python_action
  <https://pwrev.dev/226009>`__
  (issue `#354195492 <https://pwbug.dev/354195492>`__)
* `Update emboss repo to v2024.0718.173957
  <https://pwrev.dev/224713>`__
  (issue `#354195492 <https://pwbug.dev/354195492>`__)
* `Run python with optimization on
  <https://pwrev.dev/224712>`__
  (issue `#354195492 <https://pwbug.dev/354195492>`__)
* `Make emboss_runner_py Soong target
  <https://pwrev.dev/225151>`__
  (issue `#352364622 <https://pwbug.dev/352364622>`__)
* `Remove -Wno-format-invalid-specifier
  <https://pwrev.dev/213660>`__
* `Update emboss repo to v2024.0716.040724
  <https://pwrev.dev/223592>`__
  (issue `#353533164 <https://pwbug.dev/353533164>`__)

Go
--
* `Create a Pigweed-wide go.mod file
  <https://pwrev.dev/225011>`__
  (issue `#258836641 <https://pwbug.dev/258836641>`__)

Miscellaneous
=============

Rolls
-----
FreeRTOS was updated to version ``10.5.1.bcr.2``. CMake was bumped to version
``3@3.30.1.chromium.8``. Rust was updated to Git revision
``73a228116ae8c8ce73e309eee8c730ce90feac78``.

* `FreeRTOS for upstream
  <https://pwrev.dev/226091>`__
  (issue `#355203454 <https://pwbug.dev/355203454>`__)
* `FreeRTOS
  <https://pwrev.dev/225791>`__
* `cmake
  <https://pwrev.dev/225172>`__
* `rust
  <https://pwrev.dev/225171>`__
* `rules_python
  <https://pwrev.dev/224054>`__
  (issue `#258836641 <https://pwbug.dev/258836641>`__)
* `Fuchsia SDK
  <https://pwrev.dev/223593>`__
  (issues `#258836641 <https://pwbug.dev/258836641>`__,
  `#353749536 <https://pwbug.dev/353749536>`__)
* `310, 311
  <https://pwrev.dev/222650>`__

Targets
=======

RP2040
------
* `Add pw_libcxx as dep for system_async_example
  <https://pwrev.dev/223573>`__
* `Import statement fix
  <https://pwrev.dev/225754>`__
  (issue `#258836641 <https://pwbug.dev/258836641>`__)
* `Enhance on-device testing instructions
  <https://pwrev.dev/225311>`__
* `Fix assert basic termination behavior
  <https://pwrev.dev/223580>`__

------------
Jul 11, 2024
------------
Highlights (Jun 28, 2024 to Jul 11, 2024):

* **Bazel 8 pre-release**: Upstream Pigweed is now using a pre-release version
  of Bazel 8, the first version to include platform-based flags.
* **ARMv6-M support**: :ref:`module-pw_cpu_exception_cortex_m` now supports
  ARMv6-M cores.
* **Browser-based pw_system console**: The new ``--browser`` option lets
  you start a ``pw_system`` console in a web browser rather than the default
  Python-based terminal console.
* **Updated pw_rpc docs**: The :ref:`module-pw_rpc` docs
  have been revamped to make getting started easier and to
  provide more Bazel guidance.

Build systems
=============

Bazel
-----
Pigweed is now using version 8.0.0-pre.20240618.2 of Bazel, the first version
to include platform-based flags. Some backend collection targets are now being
provided as dictionaries to enable downstream projects to use the pattern
described in :ref:`docs-bazel-compatibility-facade-backend-dict`.  The
``incompatible_with_mcu`` Bazel helper has been introduced to help express
whether a target is only compatible with platforms that have a full-featured
OS.

* `Run all tests with googletest backend in CI
  <https://pwrev.dev/222532>`__
  (issue `#352584464 <https://pwbug.dev/352584464>`__)
* `Add missing dependencies
  <https://pwrev.dev/222572>`__
  (issue `#352584464 <https://pwbug.dev/352584464>`__)
* `Don't propagate flags to exec config
  <https://pwrev.dev/220812>`__
  (issues `#234877642 <https://pwbug.dev/234877642>`__,
  `#315871648 <https://pwbug.dev/315871648>`__)
* `Provide backend collections as dicts
  <https://pwrev.dev/219911>`__
  (issue `#344654805 <https://pwbug.dev/344654805>`__)
* `Roll out incompatible_with_mcu
  <https://pwrev.dev/216852>`__
  (issue `#348008794 <https://pwbug.dev/348008794>`__)
* `Introduce incompatible_with_mcu
  <https://pwrev.dev/216851>`__
  (issue `#348008794 <https://pwbug.dev/348008794>`__,
  `#343481391 <https://pwbug.dev/343481391>`__)
* `Add back to CI some building targets
  <https://pwrev.dev/218698>`__
  (issue `#261603269 <https://pwbug.dev/261603269>`__)

Docs
====
* `Fix shortlink URL
  <https://pwrev.dev/221751>`__
* `Add shortlink to pw_enviro draft
  <https://pwrev.dev/221533>`__
* `Update changelog
  <https://pwrev.dev/219131>`__

Modules
=======

pw_allocator
------------
A bug was fixed that caused builds to break when
``-Wmissing-template-arg-list-after-template-kw`` is turned on.

* `Remove unnecessary template
  <https://pwrev.dev/220111>`__

pw_assert
---------
Downstream projects using Bazel now need to set the new backend label flags
``@pigweed//pw_assert:check_backend`` and
``@pigweed//pw_assert:check_backend_impl`` and include them in their link
deps.

* `Split up Bazel assert backend, part 2
  <https://pwrev.dev/219791>`__
  (issue `#350585010 <https://pwbug.dev/350585010>`__)
* `Split up Bazel assert backend, part 1
  <https://pwrev.dev/219611>`__
  (issue `#350585010 <https://pwbug.dev/350585010>`__)

pw_async2
---------
* `Add missing thread_stl dependency
  <https://pwrev.dev/219291>`__

pw_async2_epoll
---------------
* `Use unordered_map; silence persistent warnings
  <https://pwrev.dev/218860>`__

pw_async_basic
--------------
* `Fix size report build error
  <https://pwrev.dev/219691>`__
  (issue `#350780546 <https://pwbug.dev/350780546>`__)

pw_bluetooth
------------
* `Format emboss files
  <https://pwrev.dev/219351>`__

pw_bluetooth_proxy
------------------
``pw::bluetooth::proxy::sendGattNotify()`` (lowercase first letter) has been
removed; use ``pw::bluetooth::proxy::SendGattNotify()`` (uppercase first
letter) instead. The new
``pw::bluetooth::proxy::GetNumSimultaneousAclSendsSupported()`` function
returns the max number of LE ACL sends that can be in-flight at one time.
The new ``pw::bluetooth::proxy::AclDataChannel::Reset()`` and
``pw::bluetooth::proxy::ProxyHost::Reset()`` methods let you reset the internal
state of those classes.

* `Delete "sendGattNotify"
  <https://pwrev.dev/220951>`__
  (issue `#350106534 <https://pwbug.dev/350106534>`__)
* `Include <optional> in proxy_host.h
  <https://pwrev.dev/220657>`__
* `ProxyHost supports multiple sends
  <https://pwrev.dev/220573>`__
  (issues `#348680331 <https://pwbug.dev/348680331>`__,
  `#326499764 <https://pwbug.dev/326499764>`__)
* `Remove unneeded PW_EXCLUSIVE_LOCKS_REQUIRED
  <https://pwrev.dev/219417>`__
  (issue `#350106534 <https://pwbug.dev/350106534>`__)
* `Soft transition to SendGattNotify
  <https://pwrev.dev/219120>`__
  (issue `#350106534 <https://pwbug.dev/350106534>`__)
* `Add ProxyHost/AclDataChannel::Reset()
  <https://pwrev.dev/219119>`__
  (issue `#350497803 <https://pwbug.dev/350497803>`__)
* `Dedup NOCP construction in tests
  <https://pwrev.dev/219118>`__
* `Document which events are expected
  <https://pwrev.dev/219353>`__
  (issue `#326499764 <https://pwbug.dev/326499764>`__)
* `Adjust const for pw::span
  <https://pwrev.dev/218877>`__
  (issue `#326497489 <https://pwbug.dev/326497489>`__)
* `Remove use of <mutex>
  <https://pwrev.dev/218893>`__
  (issue `#350009505 <https://pwbug.dev/350009505>`__)
* `Prevent crash in GattNotifyTest
  <https://pwrev.dev/218834>`__

pw_bluetooth_sapphire
---------------------
* `Remove modulo operator from asserts
  <https://pwrev.dev/222233>`__
* `Add Bazel build files to lib/ packages
  <https://pwrev.dev/222232>`__
* `Use pwemb namespace alias in FakeController
  <https://pwrev.dev/221034>`__
* `Unmask LE Connection Complete
  <https://pwrev.dev/221033>`__
* `Implement ExtendedLowEnergyConnector
  <https://pwrev.dev/221032>`__
  (issue `#305976440 <https://pwbug.dev/305976440>`__)
* `Enable bt-host component
  <https://pwrev.dev/221011>`__
  (issues `#303116559 <https://pwbug.dev/303116559>`__,
  `# b/324109634 <https://pwbug.dev/ b/324109634>`__,
  `#326079781 <https://pwbug.dev/326079781>`__,
  `# b/325142183 <https://pwbug.dev/ b/325142183>`__)
* `Support ISO Channel FIDL Protocol in Drivers
  <https://pwrev.dev/221267>`__
  (issue `#328457492 <https://pwbug.dev/328457492>`__,
  issue `# b/328459391 <https://pwbug.dev/ b/328459391>`__)
* `Add ISO support to controllers
  <https://pwrev.dev/218992>`__
  (issue `#311639690 <https://pwbug.dev/311639690>`__)
* `Tag integration test
  <https://pwrev.dev/220991>`__
  (issue `#344654806 <https://pwbug.dev/344654806>`__)
* `Implement ExtendedLowEnergyScanner
  <https://pwrev.dev/218979>`__
  (issue `#305975969 <https://pwbug.dev/305975969>`__)
* `Add packet filtering consts to vendor protocol
  <https://pwrev.dev/218978>`__
* `Update LEAdvertisers to use EmbossCommandPacket
  <https://pwrev.dev/218991>`__
  (issue `#312896684 <https://pwbug.dev/312896684>`__)
* `Use Emboss for android vendor exts multi advert
  <https://pwrev.dev/218977>`__
  (issue `#312896673 <https://pwbug.dev/312896673>`__)
* `Use Emboss versions of a2dp offload structs
  <https://pwrev.dev/218976>`__
* `Refactor LowEnergyScanResult to its own class
  <https://pwrev.dev/218975>`__
* `Refactor TxEngine to allow queueing SDUs
  <https://pwrev.dev/218915>`__
* `Add incoming CIS request handler
  <https://pwrev.dev/218972>`__
* `Add IsoStreamManager class
  <https://pwrev.dev/218971>`__
* `Add CIS events and commands
  <https://pwrev.dev/218914>`__
  (issue `#311639432 <https://pwbug.dev/311639432>`__)
* `Retrieve sleep clock accuracy for peers
  <https://pwrev.dev/218913>`__
  (issue `#311639272 <https://pwbug.dev/311639272>`__)
* `Fix LowEnergyScanner crash
  <https://pwrev.dev/218974>`__
  (issue `#323098126 <https://pwbug.dev/323098126>`__)
* `Remove unused include
  <https://pwrev.dev/214677>`__
* `Add infrastructure for SCA operations
  <https://pwrev.dev/214676>`__
  (issue `#311639272 <https://pwbug.dev/311639272>`__)
* `Use Write instead of UncheckedWrite
  <https://pwrev.dev/214675>`__
* `Remove now unnecessary use of std::optional
  <https://pwrev.dev/214654>`__
* `Expose connection role to le handle
  <https://pwrev.dev/214653>`__
  (issue `#311639432 <https://pwbug.dev/311639432>`__)
* `Disambiguate comment
  <https://pwrev.dev/214673>`__
* `Fix typo in comment
  <https://pwrev.dev/214652>`__

pw_build
--------
The new macros in ``//pw_build:merge_flags.bzl`` help with using
platform-based flags.

* `Add flags_from_dict
  <https://pwrev.dev/221691>`__
  (issue `#301334234 <https://pwbug.dev/301334234>`__)

pw_cli
------
``pw_cli`` has increased support for letting users select from interactive
prompts. The RP2040 flasher utility uses the new interactive prompting
features to let users select which detected device to flash.

* `Interactive user index prompt
  <https://pwrev.dev/220931>`__

pw_console
----------
* `Headless mode with web/ws server running
  <https://pwrev.dev/215860>`__

pw_cpu_exception
----------------
* `Remove multiplexers
  <https://pwrev.dev/219371>`__
  (issue `#347998044 <https://pwbug.dev/347998044>`__)

pw_cpu_exception_cortex_m
-------------------------
``pw_cpu_exception_cortex_m`` now supports ARMv6-M cores.

* `Add armv6-m support
  <https://pwrev.dev/219132>`__
  (issues `#350747553 <https://pwbug.dev/https://pwbug.dev/350747553>`__,
  `#350747562 <https://pwbug.dev/https://pwbug.dev/350747562>`__)

pw_digital_io_rp2040
--------------------
The new ``enable_pull_up`` and ``enable_pull_down`` fields in the
``pw::digital_io::Rp2040Config`` struct let you configure whether resistors
should be pulled up or down.

* `Add pull up/down resistors to Rp2040Config
  <https://pwrev.dev/219731>`__
* `Remove manual tags
  <https://pwrev.dev/219052>`__
  (issue `#261603269 <https://pwbug.dev/261603269>`__)

pw_env_setup
------------
* `Use full paths for proj action imports
  <https://pwrev.dev/222571>`__

pw_function
-----------
* `Dynamic allocation for upstream host
  <https://pwrev.dev/221871>`__

pw_log_string
-------------
* `Require backend_impl to be set explicitly
  <https://pwrev.dev/221293>`__

pw_malloc_freertos
------------------
* `Fix typo
  <https://pwrev.dev/220751>`__
  (issue `#351945325 <https://pwbug.dev/351945325>`__)

pw_presubmit
------------
* `Narrow copyright notice exclusions
  <https://pwrev.dev/221532>`__
  (issue `#347062591 <https://pwbug.dev/347062591>`__)
* `Add copyright notice to some test data
  <https://pwrev.dev/221395>`__
  (issue `#347062591 <https://pwbug.dev/347062591>`__)
* `Exclude test_data from bazel_lint
  <https://pwrev.dev/221152>`__
* `Exclude files from copyright
  <https://pwrev.dev/221151>`__
* `Don't automatically use exclusions
  <https://pwrev.dev/216355>`__
  (issue `#347274642 <https://pwbug.dev/347274642>`__)

pw_rpc
------
The :ref:`module-pw_rpc` docs have been revamped to make getting started
easier and to provide more Bazel guidance.

* `Expand comment for internal::ClientServerTestComment
  <https://pwrev.dev/220574>`__
* `Update docs
  <https://pwrev.dev/219392>`__
  (issue `#349832019 <https://pwbug.dev/349832019>`__)

pw_stream_uart_mcuxpresso
-------------------------
* `Clean up dma stream comments
  <https://pwrev.dev/222111>`__

pw_sys_io
---------
* `Remove multiplexer, constraints
  <https://pwrev.dev/218736>`__
  (issue `#347998044 <https://pwbug.dev/347998044>`__)

pw_sys_io_baremetal_stm32f429
-----------------------------
Bazel projects should now set the ``--@pigweed//pw_sys_io:backend``
label flag to ``@pigweed//pw_sys_io_baremetal_stm32f429`` and add the
``@pigweed//pw_sys_io_baremetal_stm32f429:compatible`` constraint to their
platform to indicate that the platform is compatible with
``pw_sys_io_baremetal_stm32f429``.

* `Add constraint
  <https://pwrev.dev/218831>`__
  (issue `#347998044 <https://pwbug.dev/347998044>`__)

pw_sys_io_stm32cube
-------------------
* `Remove target_compatible_with
  <https://pwrev.dev/218704>`__
  (issue `#347998044 <https://pwbug.dev/347998044>`__)

pw_system
---------
The new ``--browser`` option lets you start a browser-based ``pw_system``
console instead of a terminal-based one.Thread stack sizes for the new
async version of ``pw_system`` can now be configured with
``PW_SYSTEM_ASYNC_LOG_THREAD_STACK_SIZE_BYTES``,
``PW_SYSTEM_ASYNC_RPC_THREAD_STACK_SIZE_BYTES``,
``PW_SYSTEM_ASYNC_TRANSFER_THREAD_STACK_SIZE_BYTES``, and
``PW_SYSTEM_ASYNC_DISPATCHER_THREAD_STACK_SIZE_BYTES``. The new
``pw::system::AsyncCore::RunOnce()`` method provides a way to run a function
once on a separate thread. The ``--ipython`` option has been removed from the
``pw_system`` console. See :ref:`module-pw_console-embedding-ipython` for
guidance on how to embed IPython.

* `Add web console option
  <https://pwrev.dev/221071>`__
* `Add missing work queue thread
  <https://pwrev.dev/222372>`__
* `Clean up pw_system/threads.cc
  <https://pwrev.dev/222371>`__
* `Configurable thread stack sizes
  <https://pwrev.dev/221394>`__
* `RunOnce function for work queue functionality
  <https://pwrev.dev/218954>`__
* `Remove IPython from pw_system console
  <https://pwrev.dev/218882>`__
* `Organize pw_system:async build targets
  <https://pwrev.dev/218737>`__

pw_thread
---------
* `Remove backend multiplexers
  <https://pwrev.dev/218238>`__
  (issue `#347998044 <https://pwbug.dev/347998044>`__)

pw_thread_stl
-------------
The ``pw::thread::Thread::native_handle()`` method now returns a pointer to
the underlying thread object instead of a reference and the docs have been
updated to make it clear that using this is inherently non-portable.

* `Change NativeThreadHandle to ptr
  <https://pwrev.dev/219251>`__
  (issue `#350349092 <https://pwbug.dev/350349092>`__)

pw_toolchain
------------
Go binaries have been updated to no longer link with position-independent
executables (PIE) on Linux.

* `Disable PIE for Golang
  <https://pwrev.dev/220191>`__
  (issue `#347708308 <https://pwbug.dev/347708308>`__)

pw_toolchain_bazel
------------------
* `Add native binary for clang-tidy
  <https://pwrev.dev/221471>`__
  (issue `#352343585 <https://pwbug.dev/352343585>`__)

pw_trace
--------
* `Remove backend multiplexer
  <https://pwrev.dev/219792>`__
  (issue `#347998044 <https://pwbug.dev/347998044>`__)

pw_transfer
-----------
* `Don't assert on resource status responder
  <https://pwrev.dev/219037>`__

pw_unit_test
------------
* `Fix CMake test runner argument forwarding
  <https://pwrev.dev/218973>`__

pw_web
------
Upstream Pigweed protos are now provided alongside downstream project protos.

* `Include core .proto files in the npm bundle
  <https://pwrev.dev/222533>`__
* `Add newlines, separators, and clear for output
  <https://pwrev.dev/222531>`__
  (issue `#348650028 <https://pwbug.dev/348650028>`__)
* `Set min width for message
  <https://pwrev.dev/221592>`__
  (issue `#351901512 <https://pwbug.dev/351901512>`__)
* `Implement console-level split panels
  <https://pwrev.dev/220691>`__
  (issue `#348649945 <https://pwbug.dev/348649945>`__)
* `Update REPL styles
  <https://pwrev.dev/220971>`__
  (issue `#348650028 <https://pwbug.dev/348650028>`__)
* `Repl kernel interface and litjs component for repl
  <https://pwrev.dev/217311>`__
* `Fix columns on first load
  <https://pwrev.dev/218551>`__
  (issue `#346869281 <https://pwbug.dev/346869281>`__)
* `Debounce grid template calc on resize
  <https://pwrev.dev/218377>`__
  (issues `#346596380 <https://pwbug.dev/346596380>`__,
  `#342450728 <https://pwbug.dev/342450728>`__)

Rolls
=====
Pigweed is now using version 8.0.0-pre.20240618.2 of Bazel, the first version
to include platform-based flags. Go was updated to version ``3@1.22.5``. CMake
was updated to version ``3@3.30.0.chromium.8``.

* `Update Bazel to 8.0 rolling release
  <https://pwrev.dev/220118>`__
  (issue `#344013743 <https://pwbug.dev/344013743>`__)
* `Update Bazel to 7.2
  <https://pwrev.dev/220571>`__
  (issue `#347708308 <https://pwbug.dev/347708308>`__)
* `go
  <https://pwrev.dev/220471>`__
* `cmake
  <https://pwrev.dev/220351>`__
* `310, 311
  <https://pwrev.dev/219173>`__

Targets
=======

RP2040
------
The RP2040 flasher now provides more feedback when a board has been
successfully flashed.

* `Fix FreeRTOS tick rate
  <https://pwrev.dev/220791>`__
  (issue `#351906735 <https://pwbug.dev/351906735>`__)
* `Log on successful flash
  <https://pwrev.dev/220575>`__
  (issue `#352052013 <https://pwbug.dev/352052013>`__)
* `Fix build command in docs
  <https://pwrev.dev/219359>`__

Third-party software
====================

Emboss
------
Emboss was updated to version ``2024.0702.215418``.

* `Add missing ir_data_utils.py to GN build
  <https://pwrev.dev/220114>`__
  (issue `#350970460 <https://pwbug.dev/350970460>`__)
* `Update emboss to v2024.0702.215418
  <https://pwrev.dev/219793>`__
  (issue `#350970460 <https://pwbug.dev/350970460>`__)

FreeRTOS
--------
* `Add missing CM33_NTZ header
  <https://pwrev.dev/222574>`__

GoogleTest
----------
* `Fix the docs
  <https://pwrev.dev/222573>`__
  (issue `#352584464 <https://pwbug.dev/352584464>`__)

ICU
---
* `Update Bazel rules for dependent headers
  <https://pwrev.dev/222231>`__
* `Add Bazel build rules
  <https://pwrev.dev/218702>`__
  (issue `#321300565 <https://pwbug.dev/321300565>`__)

Mbed TLS
--------
* `Remove old build file
  <https://pwrev.dev/220137>`__
* `Rename build file
  <https://pwrev.dev/218709>`__

Miscellaneous
=============

GitHub
------
* `Add copyright notice
  <https://pwrev.dev/221491>`__
  (issue `#347062591 <https://pwbug.dev/347062591>`__)

dotfiles
--------
* `Add copyright notice
  <https://pwrev.dev/221531>`__
  (issue `#347062591 <https://pwbug.dev/347062591>`__)

------------
Jun 27, 2024
------------
Highlights (Jun 14, 2024 to Jun 27, 2024):

* **RP2040 implementation for pw_channel**:
  :cpp:func:`pw::channel::Rp2StdioChannelInit` is a new
  :ref:`module-pw_channel` implementation that reads from and writes
  to RP2040's ``stdio``.
* **Bazel compatibility patterns guide**: The new
  :ref:`docs-bazel-compatibility` guide describes the Bazel patterns that
  Pigweed uses to express that a build target is compatible with a platform.
* **Hex dump helper**: The new :cpp:func:`pw::dump::LogBytes` helper makes
  it easier to log binary data as human-readable hex dumps. The number of
  input bytes to display per line can be configured via the ``kBytesPerLine``
  template parameter.

Build systems
=============

Bazel
-----
The obsolete ``testonly_freertos`` platform has been removed.

.. todo-check: disable

* `Encapsulate rp2040 WORKSPACE deps into deps.bazl
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217219>`__
* `Update pin for rules_libusb
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217212>`__
* `Update TODO in bazelrc
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216274>`__
  (issue `#347317581 <https://pwbug.dev/347317581>`__)
* `Stop using deprecated pw_facade aliases, v3
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216436>`__
  (issue `#328679085 <https://pwbug.dev/328679085>`__)
* `Stop using deprecated pw_facade aliases, v2
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216271>`__
  (issue `#328679085 <https://pwbug.dev/328679085>`__)
* `Remove testonly_freertos platform
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216181>`__
* `Remove unnecessary @pigweed references
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218411>`__
* `Add clippy to CI
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218271>`__
  (issue `#268087116 <https://pwbug.dev/268087116>`__)

.. todo-check: enable

Docs
====
The new :ref:`Bazel installation guide <docs-install-bazel>` provides
Pigweed's recommendations on how to install Bazel. The
:ref:`docs-github-actions` guide was updated. The new :ref:`docs-bazel-compatibility`
guide describes the Bazel patterns that Pigweed uses to express that a build target
is compatible with a platform.

* `Add emboss to packages installed for build_docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216893>`__
* `Remove mention of multiplexers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216672>`__
  (issue `#344654805 <https://pwbug.dev/344654805>`__)
* `Add OWNERS
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216834>`__
* `Add Bazel installation guide
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216531>`__
* `Update GitHub actions tutorial
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216276>`__
* `Update Pigweed Live schedule
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216354>`__
  (issue `#347677570 <https://pwbug.dev/347677570>`__)
* `Bazel compatibility patterns
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214971>`__
  (issue `#344654805 <https://pwbug.dev/344654805>`__)
* `Update changelog
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216153>`__

Modules
=======

pw_allocator
------------
The ``//pw_allocator:block_allocator`` target has been removed. Consumers
are now expected to depend on and include individual block allocator targets.

* `Use specific block allocator headers and targets
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211917>`__
* `Fix Android build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216511>`__
* `Clean up Block interface
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211916>`__
  (issue `#326509341 <https://pwbug.dev/326509341>`__)

pw_assert
---------
* `Remove backend multiplexer
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215914>`__
  (issue `#347998044 <https://pwbug.dev/347998044>`__)

pw_bluetooth
------------
New Emboss structs added: ``NumberOfCompletedPacketsEvent``,
``WritePinTypeCommandCompleteEvent``, ``PinCodeRequestNegativeReplyCommandCompleteEvent``,
``ReadPinTypeCommandCompleteEvent``, ``PinCodeRequestEvent``,
``PinCodeRequestReplyCommandCompleteEvent``, ``WritePinTypeCommand``,
``ReadPinTypeCommand``, ``PinCodeRequestNegativeReplyCommand``,
``PinCodeRequestReplyCommand``.

* `Add NumberOfCompletedPacketsEvent
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216250>`__
  (issue `#326499764 <https://pwbug.dev/326499764>`__)
* `Add AttNotifyOverAcl to att.emb
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218311>`__
* `Add PinCodeRequestEvent
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217791>`__
  (issue `#342151162 <https://pwbug.dev/342151162>`__)
* `Add IoCapability enum field
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217074>`__
* `Add ACL & L2CAP B-frame Emboss definitions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216248>`__
  (issue `#326499764 <https://pwbug.dev/326499764>`__)
* `Add att.emb
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216247>`__
  (issue `#326499764 <https://pwbug.dev/326499764>`__)
* `Add WritePinTypeCommandCompleteEvent Emboss struct
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216923>`__
  (issue `#342151162 <https://pwbug.dev/342151162>`__)
* `Add ReadPinTypeCommandCompleteEvent Emboss struct
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216922>`__
  (issue `#342151162 <https://pwbug.dev/342151162>`__)
* `Add PinCodeRequestNegativeReplyCommandCompleteEvent Emboss
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216921>`__
  (issue `#342151162 <https://pwbug.dev/342151162>`__)
* `Add PinCodeRequestReplyCommandCompleteEvent Emboss struct
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216920>`__
  (issue `#342151162 <https://pwbug.dev/342151162>`__)
* `Add WritePinTypeCommand Emboss struct
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216919>`__
  (issue `#342151162 <https://pwbug.dev/342151162>`__)
* `Add ReadPinTypeCommand Emboss struct
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216918>`__
  (issue `#342151162 <https://pwbug.dev/342151162>`__)
* `Add PinCodeRequestNegativeReplyCommand Emboss struct
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216917>`__
  (issue `#342151162 <https://pwbug.dev/342151162>`__)
* `Add PinCodeRequestReplyCommand Emboss struct
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216916>`__
  (issue `#342151162 <https://pwbug.dev/342151162>`__)
* `Add PinType enum
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216915>`__
  (issue `#342151162 <https://pwbug.dev/342151162>`__)

pw_bluetooth_proxy
------------------
The new ``pw::bluetooth::proxy::AclDataChannel::ProcessDisconnectionCompleteEvent()``
method frees up resources when a connection is removed. The new
``pw::bluetooth::proxy::AclDataChannel::ProcessNumberOfCompletedPacketsEvent()`` method
removes completed packets as necessary to reclaim LE ACL credits. The new
``pw::bluetooth::proxy::ProxyHost::sendGattNotify()`` method is a simple
implementation of sending a GATT notification to a connected peer.

* `Process Disconnection_Complete event
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218652>`__
  (issue `#326499764 <https://pwbug.dev/326499764>`__)
* `Implement basic ACL credit tracking
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216411>`__
  (issue `#326499764 <https://pwbug.dev/326499764>`__)
* `Implement sendGattNotify
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216249>`__
  (issue `#326499764 <https://pwbug.dev/326499764>`__)
* `Have release_fn take buffer*
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217414>`__
  (issue `#326499764 <https://pwbug.dev/326499764>`__)
* `Add GetH4Span
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217413>`__
  (issue `#326499764 <https://pwbug.dev/326499764>`__)
* `Add release_fn to H4PacketWithH4
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216971>`__
  (issue `#326499764 <https://pwbug.dev/326499764>`__)
* `Fix case style for some test variables
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216914>`__
  (issue `#326499764 <https://pwbug.dev/326499764>`__)
* `Move to-controller flow to using h4-based packets
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216897>`__
  (issue `#326499764 <https://pwbug.dev/326499764>`__)
* `Add H4PacketWithH4 ctor that takes type
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216896>`__
  (issue `#326499764 <https://pwbug.dev/326499764>`__)
* `Move H4Packet to using move semantics
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216913>`__
  (issue `#326499764 <https://pwbug.dev/326499764>`__)
* `Fix naming of SetH4Type
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216895>`__
  (issue `#326499764 <https://pwbug.dev/326499764>`__)
* `Move to using H4Packet wrapper classes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215222>`__
  (issues `#326499764 <https://pwbug.dev/326499764>`__,
  `#326497489 <https://pwbug.dev/326497489>`__)

pw_bluetooth_sapphire
---------------------
* `Move LegacyLowEnergyScanner impl to base class
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214674>`__
* `Add spec reference to comment
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217111>`__
  (issue `#311639040 <https://pwbug.dev/311639040>`__)

pw_boot
-------
* `Remove backend multiplexer
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217213>`__
  (issue `#347998044 <https://pwbug.dev/347998044>`__)

pw_build
--------
The ``BuildCommand`` Python class now has an optional ``working_dir`` argument
that allows you to specify the working directory in which a build command
should be executed. The new ``boolean_constraint_value`` syntactic sugar macro
makes it easier to declare a constraint setting with just two possible
constraint values.

* `Add optional working directory arg to BuildCommand
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217831>`__
  (issue `#328083083 <https://pwbug.dev/328083083>`__)
* `Introduce boolean_constraint_value
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216832>`__
  (issue `#344654805 <https://pwbug.dev/344654805>`__)
* `Move host_backend_alias (part 2)
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215741>`__
  (issue `#344654805 <https://pwbug.dev/344654805>`__)

pw_build_android
----------------
Dynamic allocation for ``pw::Function`` is now always enabled in Android
builds to allow ``pw::Function`` to exceed the inline size limit.

* `Enable function dynamic alloc
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218219>`__
  (issue `#349352849 <https://pwbug.dev/349352849>`__)

pw_build_info
-------------
The new ``//pw_build_info:git_build_info`` Bazel rule lets you embed which
Git commit your binary was built from.

* `Add git_build_info.h header for embedding git info
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213854>`__

pw_build_mcuxpresso
-------------------
The type for the ``include`` parameter in the
``pw_build_mcuxpresso.components.Project`` Python class constructor changed
from ``list[str]`` to ``Collection[str]`` and the type for the ``exclude``
parameter changed from  ``list[str]`` to ``Container[str]``.
``pw_build_mcuxpresso.bazel.bazel_output()`` now accepts an optional
``extra_args`` argument, which is a dictionary of additional arguments to be
added to the generated Bazel target.

* `Fix bug in create_project()
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218272>`__
* `Add extra_args to bazel.bazel_output()
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217754>`__

pw_channel
----------
:cpp:func:`pw::channel::Rp2StdioChannelInit` is a new
:ref:`module-pw_channel` implementation that reads from and writes
to RP2040's ``stdio``.

* `Add Rp2StdioChannel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217954>`__
* `Cleanup redundant checks in epoll_channel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218376>`__

pw_chrono
---------
The new ``pw_targets_FREERTOS_BACKEND_GROUP`` GN rule sets FreeRTOS
backends for ``pw_chrono``, ``pw_sync``, and ``pw_thread`` in one go. Each
backend can be individually overridden if needed.

* `Add docs metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217752>`__
* `Group common backends in the GN build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215336>`__

pw_cli
------
* `Add missing modules to Bazel build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217571>`__
* `Improve messaging for GitErrors
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217054>`__

pw_clock_tree
-------------
The new :cpp:func:`pw::clock_tree::ClockTree::AcquireWith` method lets
you acquire a clock tree element while enabling another one.

* `Introduce ClockTree AcquireWith method
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217131>`__
  (issue `#331672574 <https://pwbug.dev/331672574>`__)

pw_clock_tree_mcuxpresso
------------------------
:cpp:func:`AcquireWith` should now be used when enabling clock tree
elements that are sourced from the audio PLL or SYS PLL.

* `Use AcquireWith for audio PLL
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216911>`__
  (issue `#331672574 <https://pwbug.dev/331672574>`__)

pw_cpu_exception_cortex_m
-------------------------
* `rm backend multiplexer
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217214>`__
  (issue `#347998044 <https://pwbug.dev/347998044>`__)

pw_docgen
---------
* `Add bug Docutils role
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215911>`__

pw_emu
------
``pw_emu`` now supports Bazel.

* `Add bazel python build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217053>`__

pw_env_setup
------------
The version of ``cffi`` that ``pw_env_setup`` uses was updated to
``1.16.0`` to fix Windows failures. The "fatal error" that
``pw_env_setup`` used to log when running from a directory that's
outside of a Git repo has been suppressed.

* `Update cffi
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217218>`__
  (issue `#348697900 <https://pwbug.dev/348697900>`__)
* `Suppress error message when running outside git repo
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217216>`__

pw_format
---------
* `Fix and enable disabled Rust tests
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217135>`__

pw_grpc
-------
The C++ module now handles corrupt frames more gracefully.

* `Avoid buffer overflow when processing corrupt frames
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216651>`__

pw_hex_dump
-----------
The ``pw_log_bytes`` target has been renamed to ``log_bytes``. The new
:cpp:func:`pw::dump::LogBytes` helper makes it easier to log binary data
as human-readable hex dumps. The number of input bytes to display per line
can be configured via the ``kBytesPerLine`` template parameter.

* `Remove pw prefix from log_bytes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218531>`__
* `Add LogBytes helper
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216711>`__

pw_i2c
------
* `Handle unaligned buffer reads in register_device_test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217811>`__
  (issue `#325509758 <https://pwbug.dev/325509758>`__)

pw_ide
------
A ``--process-files`` (``-P``) flag was added to ``pw ide cpp`` to process
compilation databases at the provided paths. Bazel support for ``pw_ide`` has
started. ``pw_ide`` now explicitly runs all commands from the ``PW_PROJECT_ROOT``
directory.

* `Point to compile commands extractor fork
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218631>`__
* `Add option to process comp DBs by path
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218334>`__
* `Add Bazel dependencies wrapper
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218320>`__
* `Bazelify
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217572>`__
* `Run commands from PW_PROJECT_ROOT dir
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216471>`__

pw_log
------
* `Remove backend multiplexer
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215892>`__
  (issue `#347998044 <https://pwbug.dev/347998044>`__)

pw_malloc
---------
* `Add docs metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217951>`__

pw_multibuf
-----------
* `Fix Android build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216611>`__

pw_preprocessor
---------------
* `Add docs metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217952>`__

pw_presubmit
------------
* `Add check for rp2040_binary transition
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218273>`__
* `Build STM32F429i baremetal in CI
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217231>`__

pw_rpc
------
* `Restructure Channel / internal::Channel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216037>`__

pw_rust
-------
* `Static Library Linking
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216313>`__

pw_spi_mcuxpresso
-----------------
* `Separate Bazel build targets
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217313>`__
* `Fix unused parameter warning
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217312>`__
  (issue `#348512572 <https://pwbug.dev/348512572>`__)

pw_sync
-------
The new ``pw_targets_FREERTOS_BACKEND_GROUP`` GN rule sets FreeRTOS
backends for ``pw_chrono``, ``pw_sync``, and ``pw_thread`` in one go. Each
backend can be individually overridden if needed.

* `Remove multiplexers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216819>`__
  (issue `#347998044 <https://pwbug.dev/347998044>`__)
* `Group common backends in the GN build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215336>`__

pw_system
---------
``pw_system:async`` is a new version of ``pw_system`` based on
:ref:`module-pw_async2`. The ``pw_system`` console now has a
``--device-tracing`` flag to turn device tracing on or off.

* `pw_system:async
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216239>`__
* `Async packet processing component
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214798>`__
* `Rename target_io.cc
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217051>`__
* `Allow disabling of DeviceTracing RPC calls
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216332>`__

pw_target_runner
----------------
The ``pw_target_runner`` client in Go now supports a ``server_suggestion``
flag, which allows specifying a command to suggest to the user if the server
is unavailable. The Go client's ``RunBinary`` method can now accept a binary
as a byte array instead of a file path.

* `Add suggested server command
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218011>`__
* `Send test binaries over gRPC
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216050>`__

pw_thread
---------
The new ``pw_targets_FREERTOS_BACKEND_GROUP`` GN rule sets FreeRTOS
backends for ``pw_chrono``, ``pw_sync``, and ``pw_thread`` in one go. Each
backend can be individually overridden if needed.

* `Group common backends in the GN build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215336>`__

pw_toolchain
------------
Compiler diagnostics colors are now enabled in Bazel.

* `Color diagnostics in Bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217715>`__
* `Closer align the bazel arm-gcc flags with GN
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215734>`__
* `Add clippy-driver to rust toolchains
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217211>`__
  (issue `#268087116 <https://pwbug.dev/268087116>`__)

pw_transfer
-----------
* `Java style fixes; remove unused variable and dependencies
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215858>`__

pw_watch
--------
``pw_watch`` now ignores ``bazel-*`` directories.

* `Do not watch bazel-* symlinks
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217931>`__

pw_web
------
The function signature for ``createLogViewer`` changed to
``createLogViewer(logSource, root, { columnOrder })``. The ``columnOrder``
field in the optional third parameter lets you control the ordering of
columns in the log viewer.The new ``useShoelaceFeatures`` boolean lets you
control whether the log viewer uses Shoelace components. The log viewer's
toolbar is now responsive.

* `Add optional parameters to createLogViewer
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217052>`__
  (issue `#333537914 <https://pwbug.dev/333537914>`__)
* `Add Shoelace component flag
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217093>`__
  (issue `#347966938 <https://pwbug.dev/347966938>`__)
* `Implement responsive toolbar behavior
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215591>`__
  (issue `#309650360 <https://pwbug.dev/309650360>`__)

Languages
=========

Python
------
* `Add python targets for pw_i2c, pw_digital_io protos
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216736>`__

Rust
----

* `Fix clippy lints
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217136>`__

Miscellaneous
=============

OWNERS
------
* `Add gwsq
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207731>`__

Targets
=======
References to ``configGENERATE_RUN_TIME_STATS`` have been removed because
the implementations are incomplete.

* `Remove configGENERATE_RUN_TIME_STATS functions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218711>`__

RP2040
------
The new ``flash_rp2040`` rule makes it easier to flash Raspberry Pi RP2040s
in Bazel.The new ``flash`` Bazel rule makes it easier to flash RP2040s from
a Python script.

* `Add pw_system_async example
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218691>`__
* `Mark rp2040_binary as a non-executable target for host
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218394>`__
* `Update docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217039>`__
* `Add debugprobe version detection and warning
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217055>`__
* `Switch to use upstream develop branch of Pico SDK
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216234>`__
* `Add IFTT to keep the rp2040 transition and config in sync
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218431>`__
* `Add missing backends
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218373>`__
* `Unify board selection cmdline args
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/217716>`__
  (issue `#348067379 <https://pwbug.dev/348067379>`__)
* `Add a bazel rule for flashing
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216314>`__
* `Add interrupt and freertos backends
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216493>`__
* `Add flash main target
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216492>`__
* `Add rp2040_binary transition
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216491>`__
* `Refactor test runner and extract flashing
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216152>`__

Third-party
===========

FreeRTOS
--------
``configUSE_MALLOC_FAILED_HOOK`` can now be enabled to detect out-of-memory
errors when using FreeRTOS's heap implementation.

* `Add failed malloc hook to support lib
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218380>`__
* `Fix Bazel build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/218351>`__

Nanopb
------
* `Import proto module
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216991>`__

Pico SDK
--------
* `Fix exception names in GN build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216435>`__
  (issue `#347355069 <https://pwbug.dev/347355069>`__)

------------
Jun 13, 2024
------------
.. _bootstrap.fish: https://cs.opensource.google/pigweed/pigweed/+/main:bootstrap.fish

Highlights (May 30, 2024 to Jun 13, 2024):

* **pw_allocator support in pw_mallc**: ``pw_malloc`` now supports
  :ref:`pw_allocator <module-pw_allocator>`-based backends.
* **New pw_build Bazel rules**: ``pw_py_test`` rule wraps ``py_test``,
  :ref:`pw_elf_to_dump <module-pw_build-bazel-pw_elf_to_dump>` takes a
  binary executable and produces a text file containing the full binary layout,
  and :ref:`pw_elf_to_bin <module-pw_build-bazel-pw_elf_to_bin>` rule takes
  a binary executable and produces a file with all ELF headers removed.
* **Improved Fish shell support**: The ``pw`` and ``pw build``
  commands now support `Fish <https://fishshell.com/>`__ shell completion.
  The new `bootstrap.fish`_ script demonstrates how to bootstrap a Pigweed
  project from a Fish shell and makes it easier for Fish users to contribute
  to upstream Pigweed.
* **More informative modules index**: The :ref:`modules index <docs-module-guides>`
  now shoes useful metadata for each module, such as a summary of the
  module's purpose and the programming languages that the module supports.

Active SEEDs
============
Help shape the future of Pigweed! Please visit :ref:`seed-0000`
and leave feedback on the RFCs (i.e. SEEDs) marked
``Open for Comments``.

Modules
=======

pw_allocator
------------
The ``pw::allocator::Layout`` constructor is now marked ``explicit`` to
ensure that functions that take ``Layout`` instances as arguments don't
silently accept and convert other types. The ``pw::allocator::FreeList`` and
``pw::allocator::FreeListHeap`` interfaces have been removed.

* `Make Layout constructor explicit
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211915>`__
* `Remove FreeList and FreeListHeap
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211914>`__
  (issue `#328076428 <https://issues.pigweed.dev/issues/328076428>`__)
* `Refactor Bucket chunk list
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215488>`__
  (issue `#345526413 <https://issues.pigweed.dev/issues/345526413>`__)

pw_analog
---------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214500>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_android_toolchain
--------------------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214501>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_arduino_build
----------------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214173>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_assert
---------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214539>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_async
--------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214499>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_async2
---------
* `Fix location of backends in sitenav
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213914>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_base64
---------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214540>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_bloat
--------
* `Build and run \`pw bloat\` CLI command in Bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215456>`__
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214177>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_blob_store
-------------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214502>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_bluetooth
------------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214575>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)
* `Add HCI StatusCode values
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213553>`__

pw_bluetooth_hci
----------------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214576>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_bluetooth_profiles
---------------------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214612>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_bluetooth_proxy
------------------
The new ``pw::bluetooth::proxy::HasSendAclCapability()`` function indicates
whether the proxy has the capability to send ACL packets.

* `Remove H4HciPacketSendFn alias
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215221>`__
  (issue `#326499764 <https://issues.pigweed.dev/issues/326499764>`__)
* `Fix const on sendGattNotify param
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214864>`__
  (issue `#326499764 <https://issues.pigweed.dev/issues/326499764>`__)
* `Mark unused parameters with [[maybe_unused]]
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214637>`__
  (issue `#345526399 <https://issues.pigweed.dev/issues/345526399>`__)
* `Add maybe_unused to make downstream happy
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214360>`__
  (issue `#344031126 <https://issues.pigweed.dev/issues/344031126>`__)
* `Add ProxyHost::HasSendAclCapability()
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214101>`__
  (issue `#344030724 <https://issues.pigweed.dev/issues/344030724>`__)
* `Update H4HciPacket construction
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214233>`__
* `Add sendGattNotify stub
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214106>`__
  (issue `#344031126 <https://issues.pigweed.dev/issues/344031126>`__)
* `Fix compilation errors
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213733>`__
* `Pass H4 as event type plus an HCI span
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213664>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)
* `Tweak CreateNonInteractingToHostBuffer to take array
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213663>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)

pw_boot_cortex_m
----------------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215458>`__

pw_build
--------
The new ``pw_py_test`` rule wraps ``py_test`` and defaults to setting
``target_compatible_with`` to ``host`` only. The new
:ref:`pw_elf_to_dump <module-pw_build-bazel-pw_elf_to_dump>` rule takes a
binary executable and produces a text file containing the full binary layout.
The new :ref:`pw_elf_to_bin <module-pw_build-bazel-pw_elf_to_bin>` rule takes
a binary executable and produces a file with all ELF headers removed.

* `Move host_backend_alias (part 1)
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215740>`__
  (issue `#344654805 <https://issues.pigweed.dev/issues/344654805>`__)
* `Add python.install into the default GN group
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215791>`__
* `Introduce pw_py_test to bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215258>`__
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214506>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)
* `Clarify docs on pw_elf_to_bin
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213616>`__
* `Update intro
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214062>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)
* `Add pw_elf_to_dump rule
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212851>`__
* `Add pw_elf_to_bin rule
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212671>`__
* `Populate executable field in return from link_cc utility
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212631>`__

pw_build_info
-------------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214616>`__

pw_build_mcuxpresso
-------------------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214507>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_bytes
--------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214509>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_channel
----------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214621>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)
* `Update function documentation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213712>`__
* `Remove manual registration from epoll channel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213653>`__

pw_checksum
-----------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214623>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_chre
-------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214633>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_chrono
---------
:ref:`libc time wrappers <module-pw_chrono-libc-time-wrappers>` are now
provided to improve compatibility with software not written for embedded
systems that depends on ``gettimeofday`` and ``time`` from POSIX.

* `Group common backends in the GN build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215336>`__
* `Introduce SystemClock backed link time wrappers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213551>`__

pw_cli
------
.. _Fish: https://fishshell.com/

The ``pw`` and ``pw build`` commands now support `Fish`_ shell completion.

* `Fish shell completion
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213734>`__

pw_clock_tree
-------------
The new ``pw::clock_tree::ClockSourceNoOp`` class can be used to satisfy
the dependency of a source clock tree element for other clock source classes.
The new ``pw::clock_tree::ElementController`` class provides easier integration
of optional clock tree logic into existing drivers.

* `Introduce ClockSourceNoOp class
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213851>`__
  (issue `#331672574 <https://issues.pigweed.dev/issues/331672574>`__)
* `Introduce ElementController class
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212095>`__
  (issue `#331672574 <https://issues.pigweed.dev/issues/331672574>`__)
* `Fix source set name and visibility
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212151>`__
  (issue `#331672574 <https://issues.pigweed.dev/issues/331672574>`__)

pw_clock_tree_mcuxpresso
------------------------
* `Comment clean up
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214573>`__
  (issue `#331672574 <https://issues.pigweed.dev/issues/331672574>`__)
* `Add ClockMcuxpressoRtc support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214572>`__
  (issue `#331672574 <https://issues.pigweed.dev/issues/331672574>`__)
* `Remove unnecessary pw::
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214494>`__
  (issue `#331672574 <https://issues.pigweed.dev/issues/331672574>`__)
* `Add ClockMcuxpressoAudioPll support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214332>`__
  (issue `#331672574 <https://issues.pigweed.dev/issues/331672574>`__)
* `Configure ClkIn as source for osc_clk
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214234>`__
  (issue `#331672574 <https://issues.pigweed.dev/issues/331672574>`__)
* `Make Mclk and ClkIn dependent elements
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213853>`__
  (issue `#331672574 <https://issues.pigweed.dev/issues/331672574>`__)
* `Move example code out of docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213852>`__
  (issue `#331672574 <https://issues.pigweed.dev/issues/331672574>`__)
* `Fix source set name and visibility
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212152>`__
  (issue `#331672574 <https://issues.pigweed.dev/issues/331672574>`__)

pw_console
----------
Theme colors are now correctly applied when running ``pw console`` with the
``--config-file`` option.

* `Reload theme when using a config-file
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214891>`__

pw_containers
-------------
The ``pw::Vector::at()`` function signature was changed to take ``size_t``
instead of ``size_type``.  The new ``pw::InlineVarLenEntryQueue::try_push()``
function is similar to ``pw::InlineVarLenEntryQueue::push_overwrite()`` but it
drops entries instead of overwriting old ones. The new
``pw::InlineVarLenEntryQueue::max_size()`` function returns the maximum number
of empty entries.

* `Make Vector::at() use size_t
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215859>`__
* `Disallow deletion from InlineVarLenEntryQueue base
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210639>`__
* `InlineVarLenEntryQueue::try_push() function
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213612>`__

pw_digital_io
-------------
* `Remove invalid digital_io_controller reference
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214691>`__

pw_digital_io_linux
-------------------
``pw_digital_io_linux`` now supports input interrupts. The new
:ref:`watch <module-pw_digital_io_linux-cli-watch>` command in the CLI
tool configures a GPIO line as an input and watches for interrupt events.

* `Add trigger option to CLI watch command
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213131>`__
* `Add "watch" command
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209596>`__
* `Add support for input interrupts
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209595>`__
* `Move examples out to compiled source files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209771>`__
* `Update mock_vfs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209594>`__
* `Add log_errno.h
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209593>`__

pw_docgen
---------
* `Update module metadata status badge colors
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214574>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214182>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)
* `Fix search results increasing in width
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215332>`__

pw_env_setup
------------
The new `bootstrap.fish`_ script demonstrates how to bootstrap a Pigweed
project from a Fish shell and makes it easier for Fish users to contribute
to upstream Pigweed.

* `Update clang next version
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212432>`__
* `Bootstrap fish-shell support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/56840>`__

pw_grpc
-------
* `Remove send queue timeout
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214099>`__
  (issue `#345088816 <https://issues.pigweed.dev/issues/345088816>`__)

pw_ide
------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215512>`__
* `Preserve modified editor settings
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213670>`__
  (issue `#344681641 <https://issues.pigweed.dev/issues/344681641>`__)
* `Fix constant Pylance crashes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213668>`__
  (issue `#338607100 <https://issues.pigweed.dev/issues/338607100>`__)

pw_log
------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214184>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)
* `Cast log level to int32_t
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212092>`__
  (issue `#343518613 <https://issues.pigweed.dev/issues/343518613>`__)

pw_log_string
-------------
* `Introduce link time assert() wrapper for newlib
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213072>`__
* `Set default log backend
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212832>`__

pw_malloc
---------
``pw_malloc`` now supports :ref:`pw_allocator <module-pw_allocator>`-based
backends.

* `Add allocator backends
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208417>`__

pw_multibuf
-----------
The new ``pw::multibuf::MultiBuf::IsContiguous()`` method checks if a multibuf
is contiguous and the new ``pw::multibuf::MultiBuf::ContiguousSpan()`` method
provides a way to access contiguous data as a span. The new ``CopyTo()``,
``CopyFrom()``, and ``CopyFromAndTruncate()`` methods also simplify
interactions with contiguous byte spans.

* `Contiguous span functions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214859>`__
* `Functions for copying into and out of a MultiBuf
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214858>`__
* `Truncate after an iterator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214857>`__
* `AdvanceToData in iterator constructor
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214503>`__
* `Fix Truncate(0) on empty MultiBuf
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213661>`__
* `SimpleAllocatorForTest
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212312>`__

pw_multisink
------------
The new ``pw::multisink::MultiSink::GetUnreadEntriesSize()`` and
``pw::multisink::MultiSink::UnsafeGetUnreadEntriesSize()`` methods
implement :ref:`seed-0124`. The new ``PW_MULTISINK_CONFIG_LOCK_TYPE``
macro configures the underlying lock that's used to guard multisink
reads or writes.

* `Add GetUnreadEntriesSize to Drain
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213472>`__
* `Add option to inject a user defined lock
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213211>`__

pw_presubmit
------------
* `Add --fresh to cmake presubmits
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215736>`__
* `Add coverage of rp2040 build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215231>`__
  (issue `#342638018 <https://issues.pigweed.dev/issues/342638018>`__)
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214571>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)
* `Remove shellcheck from lintformat program
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213617>`__

pw_protobuf
-----------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214187>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_ring_buffer
--------------
* `Add EntriesSize API to Reader
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213471>`__
  (issue `#337150071 <https://issues.pigweed.dev/issues/337150071>`__)

pw_router
---------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214190>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_rpc
------
The Python client API now uses positional-only arguments.

* `Use positional-only arguments in Python client API
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215532>`__
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214032>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)
* `Fix hyperlink
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213556>`__

pw_sensor
---------
* `Add Bazel support for Python package
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214534>`__

pw_spi
------
* `Fix sitenav location for RP2040 backend
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214031>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_spi_rp2040
-------------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214232>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_stream
---------
* `Fix include in mpsc_stream
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215075>`__

pw_stream_uart_mcuxpresso
-------------------------
* `Make dma_stream Write of size 0 succeed
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214151>`__
* `InterruptSafeWriter example
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212513>`__
  (issue `#343773769 <https://issues.pigweed.dev/issues/343773769>`__)
* `Stream example
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212512>`__
  (issue `#343773769 <https://issues.pigweed.dev/issues/343773769>`__)
* `Use clock tree
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209534>`__
  (issue `#331672574 <https://issues.pigweed.dev/issues/331672574>`__)
* `DMA stream example
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212391>`__
  (issue `#343773769 <https://issues.pigweed.dev/issues/343773769>`__)

pw_sync
-------
The new ``pw_targets_FREERTOS_BACKEND_GROUP`` GN rule sets up multiple
modules to use FreeRTOS backends.

* `Group common backends in the GN build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215336>`__

pw_system
---------
``pw-system-console`` now connects to the first detected port if ``--device``
isn't provided and only one port is detected or it shows an interactive
prompt if multiple ports are detected. :ref:`target-host-device-simulator-demo`
now shows how to run ``pw-system-console`` with ``host_device_simulator`` in
Bazel-based projects.

* `Console interactive serial port selection
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214912>`__
  (issue `#343949763 <https://issues.pigweed.dev/issues/343949763>`__)
* `Host device simulator entrypoint
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214972>`__
* `Move config variables to config.h
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213811>`__
* `Mention that extra libs need alwayslink
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212831>`__
* `Add host_device_simulator transitions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212414>`__

pw_target_runner
----------------
* `Switch to Bazel build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214059>`__
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214317>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_thread
---------
* `Group common backends in the GN build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215336>`__

pw_thread_freertos
------------------
* `Expand comment to help when debugging linker errors
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213669>`__

pw_toolchain
------------
* `Enable PIC on host
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214495>`__
* `Add bazel toolchain for cortex-m0plus
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215479>`__
  (issue `#346609655 <https://issues.pigweed.dev/issues/346609655>`__)
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214351>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)
* `Add IOKit and Security headers to mac toolchains
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214013>`__

pw_toolchain_bazel
------------------
* `Add cortex-a32 mcpu value
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213855>`__
  (issue `#342510882 <https://issues.pigweed.dev/issues/342510882>`__)

pw_transfer
-----------
* `Remove unused imports
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215478>`__
* `Fix ConcurrentModificationException in handleTimeouts()
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214065>`__
  (issue `#322919275 <https://issues.pigweed.dev/issues/322919275>`__)
* `Always terminate transfers on stream reopen
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212953>`__

pw_uart
-------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214252>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_varint
---------
* `Add module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214352>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)

pw_watch
--------
Watching now works in directories other than ``PW_ROOT``.

* `Enable watching from non-PW_ROOT
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215735>`__
  (issue `#328083083 <https://issues.pigweed.dev/issues/328083083>`__)

pw_web
------
* `Get icon fonts via Google Fonts URL
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212871>`__
  (issue `#332587834 <https://issues.pigweed.dev/issues/332587834>`__)
* `Fix last column filling space in log table
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212154>`__

Build systems
=============

Bazel
-----
* `Stop using deprecated pw_facade aliases
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/216151>`__
  (issue `#328679085 <https://issues.pigweed.dev/issues/328679085>`__)
* `Don't use llvm_toolchain for fuchsia_clang
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215651>`__
  (issue `#346354914 <https://issues.pigweed.dev/issues/346354914>`__)
* `Fix reference to nonexistent file
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215511>`__
* `Roll latest rules_libusb
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214792>`__
* `No integration tests in wildcard build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214056>`__
  (issue `#344654806 <https://issues.pigweed.dev/issues/344654806>`__)
* `Move integration build config in-repo
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214092>`__
  (issue `#344654806 <https://issues.pigweed.dev/issues/344654806>`__)

Hardware targets
================

host_device_simulator
---------------------
* `Make host_device_simulator_binary \`bazel run\`-able
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214693>`__

rp2040
------
* `Add bazel picotool support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214861>`__
* `Add bazel support for rp2040_utils pylib
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211591>`__
  (issue `#342634966 <https://issues.pigweed.dev/issues/342634966>`__)
* `Support running tests using the debug probe
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211363>`__
* `Add pico/debug probe filtering flags
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212611>`__
* `Temporarily disable remaining failing rp2040 tests
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215490>`__
* `Fix test runner scripts to correct check if args are specified
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214794>`__
* `Support bazel wildcard build on rp2040
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213071>`__
  (issue `#343467774 <https://issues.pigweed.dev/issues/343467774>`__)

stm32f429i
----------
* `Add baremetal bazel build support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214626>`__
  (issue `#344661765 <https://issues.pigweed.dev/issues/344661765>`__)

OS support
==========

Zephyr
------
* `Fix typo
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213271>`__

FreeRTOS
--------
FreeRTOS application function implementations like ``vApplicationStackOverflowHook()``
are now shared between multiple hardware targets.

* `Share common FreeRTOS function implementations
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213737>`__

Docs
====
The :ref:`modules index <docs-module-guides>` now includes metadata for each
module such as a summary of each module and what languages each module
supports. The ``pigweed.dev`` sitenav was simplified.
:ref:`docs-blog-02-bazel-feature-flags` was published. A "skip to main
content" accessibility feature was added to ``pigweed.dev``.

* `Fix Python package dependencies for sphinx
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/215852>`__
* `Auto-generate modules index from metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214711>`__
  (issue `#339741960 <https://issues.pigweed.dev/issues/339741960>`__)
* `Update sitenav
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214797>`__
* `Update homepage tagline
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213951>`__
* `Add "skip to main content" a11y feature
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213659>`__
  (issue `#344643289 <https://issues.pigweed.dev/issues/344643289>`__)
* `blog: Bazel feature flags
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209922>`__
* `Update changelog
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212491>`__

Third-party software support
============================
* `Add @libusb to bazel workspace
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214094>`__
* `Symlink probe-rs binary into common location
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214057>`__
* `Add probe-rs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213693>`__

mimxrt595
---------
* `Upgrade to SDK_2_14_0_EVK-MIMXRT59
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212451>`__
  (issue `#343775421 <https://issues.pigweed.dev/issues/343775421>`__)

GitHub
------
* `Fix step name
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213692>`__

io_bazel_rules_go
-----------------
* `Update to fork which disables warnings
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/214851>`__

pigweed.json
============
* `Add config for Bazel builders
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/213991>`__
  (issue `#275107504 <https://issues.pigweed.dev/issues/275107504>`__)

------------
May 30, 2024
------------
Highlights (May 17, 2024 to May 30, 2024):

* **Clock management**: The new :ref:`module-pw_clock_tree` module manages
  generic clock tree elements such as clocks, clock selectors, and clock
  dividers.
* **GitHub Actions**: The new :ref:`docs-github-actions` guide shows you
  how to set up GitHub Actions to build and test a Bazel-based Pigweed
  project when a pull request is received.
* **pw_system and Bazel**: :ref:`module-pw_system` usage in Bazel has been
  simplified by gathering all required ``pw_system`` components into one
  target and providing label flags that can set platform-dependent
  dependencies.
* **Channels and Linux Epoll**: The new :cpp:class:`pw::channel::EpollChannel`
  class sends and receives data through a file descriptor, with read and write
  notifications backed by Linux's epoll system.

Active SEEDs
============
Help shape the future of Pigweed! Please visit :ref:`seed-0000`
and leave feedback on the RFCs (i.e. SEEDs) marked
``Open for Comments``.

Modules
=======

pw_allocator
------------
* `Add BlockAllocator::MeasureFragmentation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209933>`__
* `Fix Android build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211871>`__
* `Refactor optional Allocator methods
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210402>`__
* `Make Init methods infallible
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209044>`__
  (issue `#338389412 <https://issues.pigweed.dev/issues/338389412>`__)
* `Reduce Block fragmentation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209538>`__
  (issue `#328831791 <https://issues.pigweed.dev/issues/328831791>`__)
* `Fix #if PW_HAVE_FEATURE(__cpp_rtti)
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210816>`__
  (issue `#341975367 <https://issues.pigweed.dev/issues/341975367>`__)
* `Track requested sizes in blocks
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210395>`__
* `Fix Android build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210571>`__
  (issue `#A <https://issues.pigweed.dev/issues/N/A>`__)
* `Move large values to test fixtures
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210398>`__
* `Add AsPmrAllocator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207170>`__

pw_assert
---------
* `Add missing dep in Android.bp
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211271>`__

pw_build
--------
The new ``PW_MUST_PLACE`` macro ensures that linker script inputs are
non-zero sized.

* `Introduce PW_MUST_PLACE linker script macro
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211924>`__
* `Add deps support to pw_linker_script
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211194>`__
  (issue `#331927492 <https://issues.pigweed.dev/issues/331927492>`__)

pw_channel
----------
The new :cpp:class:`pw::channel::EpollChannel` class sends and receives
data through a file descriptor, with read and write notifications backed
by Linux's epoll system.

* `Only open read/write if channel is readable/writable
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212212>`__
* `Add EpollChannel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210813>`__
* `Consistent datagram/byte channel aliases
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210796>`__

pw_chre
-------
* `Update CHRE revision
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210011>`__
  (issue `#341137451 <https://issues.pigweed.dev/issues/341137451>`__)

pw_clock_tree
-------------
The new :ref:`module-pw_clock_tree` module manages generic clock tree
elements such as clocks, clock selectors, and clock dividers.

* `Introduce new ClockDivider class
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211292>`__
  (issue `#331672574 <https://issues.pigweed.dev/issues/331672574>`__)
* `ClockTree support Element ops
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211149>`__
  (issue `#331672574 <https://issues.pigweed.dev/issues/331672574>`__)
* `Introduce Element may_block()
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211148>`__
  (issue `#331672574 <https://issues.pigweed.dev/issues/331672574>`__)
* `Generic clock tree management
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204310>`__
  (issue `#331672574 <https://issues.pigweed.dev/issues/331672574>`__)

pw_clock_tree_mcuxpresso
------------------------
The new :ref:`module-pw_clock_tree_mcuxpresso` module is an NXP
MCUXPresso backend for :ref:`module-pw_clock_tree`.

* `Mcuxpresso module
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204245>`__
  (issue `#331672574 <https://issues.pigweed.dev/issues/331672574>`__)

pw_config_loader
----------------
* `Remove unnecessary dep
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210286>`__

pw_cpu_exception_cortex_m
-------------------------
The new crash analysis API provides data about CPU state during exceptions.
See :ref:`module-pw_cpu_exception_cortex_m-crash-facade-setup`.

* `Add crash analysis API
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204248>`__
* `Fix incorrect inputs to util_test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211551>`__
* `Fix checks for ARMv8
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210272>`__

pw_emu
------
* `Exclude tests module from the Pigweed Python package
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211977>`__

pw_env_setup
------------
* `PyPI version bump to 0.0.16
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211136>`__
* `Build an extra example pypi distribution
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211134>`__
* `Remove f-strings from github_visitor
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210274>`__

pw_grpc
-------
* `Fix warnings in format strings
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211391>`__

pw_hex_dump
-----------
* `Add Android.bp
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212231>`__

pw_libcxx
---------
* `Only enable in clang builds
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211941>`__

pw_log_basic
------------
* `Fixing sign conversion error for logging
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210331>`__

pw_multisink
------------
* `Fix compiler warnings for tests
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212051>`__
  (issue `#343480404 <https://issues.pigweed.dev/issues/343480404>`__)

pw_preprocessor
---------------
* `Remove PW_HAVE_FEATURE
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211016>`__
  (issue `#341975367 <https://issues.pigweed.dev/issues/341975367>`__)

pw_rpc
------
* `Include FakeChannelOutput in soong target
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209991>`__
  (issue `#340350973 <https://issues.pigweed.dev/issues/340350973>`__)

pw_spi_mcuxpresso
-----------------
* `Remove unnecessary debug messages
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210711>`__

pw_stream_uart_mcuxpresso
-------------------------
* `Fix unused parameter warnings
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211571>`__

pw_system
---------
:ref:`module-pw_system` usage in Bazel has been simplified by gathering
all required ``pw_system`` components into one target and providing label
flags that can set platform-dependent dependencies.

* `Simplify pw_system usage in Bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210116>`__
  (issue `#341144405 <https://issues.pigweed.dev/issues/341144405>`__)

pw_thread
---------
* `Fix thread snapshot service test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/212111>`__

pw_toolchain
------------
* `Add manual tag to Rust toolchains
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211934>`__
  (issue `#342695883 <https://issues.pigweed.dev/issues/342695883>`__)
* `Add clang-apply-replacements plugin
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210405>`__
  (issue `#339294894 <https://issues.pigweed.dev/issues/339294894>`__)

pw_transfer
-----------
* `Fix transfer_thread_test initialization order
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210352>`__
* `End active transfers when RPC stream changes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209876>`__

pw_uart
-------
* `Add pw_uart to CMakeLists
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210731>`__

pw_unit_test
------------
* `Don't execute multiple test suites at the same time
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210531>`__
* `Ensure alignment of test fixtures
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210278>`__

pw_web
------
* `Move shoelace split panel import
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211940>`__
* `Add split/resize view guide in docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211471>`__
* `Implement split log views with resize
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209471>`__
  (issue `#333891204 <https://issues.pigweed.dev/issues/333891204>`__)

Build
=====

Bazel
-----
Clang's ``AddressSanitizer`` is now supported. See
:ref:`docs-automatic-analysis-sanitizers`.

* `Support asan in Bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211369>`__
  (issue `#301487567 <https://issues.pigweed.dev/issues/301487567>`__)
* `Update rules_rust to 0.45.1
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211939>`__
  (issue `#342673389 <https://issues.pigweed.dev/issues/https://pwbug.dev/342673389>`__)
* `Explicitly load rules_python in BUILD.bazel files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211361>`__

Targets
=======

rp2040
------
* `Additional bazel build file coverage
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211351>`__
  (issue `#303255049 <https://issues.pigweed.dev/issues/303255049>`__,
  issue `#305746219 <https://issues.pigweed.dev/issues/305746219>`__)

* `Enable thread high water accounting
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210831>`__
* `Update docs to add missing picotool dependency
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210811>`__
* `Save test run log files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210814>`__
* `Increase RPC thread stack size to 16K
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210832>`__
* `Allow using rp2040 devices in bootloader mode
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210525>`__
* `Refresh target docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210404>`__
* `Update openocd flashing instructions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210400>`__
* `Increase device_detector verbosity
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210411>`__
* `Add upstream Bazel platform definition
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211592>`__
* `Renable tests that pass with larger stack frames
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211147>`__
* `Raise minimal stack size to 1KB
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210517>`__
* `Replace exceptions with logging
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210396>`__

OS support
==========

freertos
--------
* `Clarify Bazel documentation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211052>`__

Docs
====
The new :ref:`docs-github-actions` guide shows you how to set up GitHub Actions
to build and test a Pigweed project when a pull request is received.

* `Add notes about GitHub Actions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211135>`__
  (issue `#338083578 <https://issues.pigweed.dev/issues/338083578>`__)
* `Instructions for freertos.BUILD.bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211193>`__
* `Create files in current directory
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206196>`__
* `Update changelog
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210285>`__

Miscellaneous
=============
.. common_typos_disable

* `Fix typo succesfully->successfully
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210671>`__
* `Update Android.bp after GetAlignedSubspan move
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210518>`__
* `Add link to format
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/211011>`__

.. common_typos_enable

------------
May 16, 2024
------------
Highlights (May 2, 2024 to May 16, 2024):

* **Coroutines**: You can now create asynchronous tasks using C++20
  :ref:`coroutines <module-pw_async2-coro>`.
* **Rust with Bazel**: The Rust toolchain can now be used by downstream projects
  using Bazel.
* **More MCUXpresso support**: Several modules have additional support for
  projects built using the NXP MCUXpresso SDK, including multiple core support
  in :ref:`module-pw_build_mcuxpresso`, a new initiator in
  :ref:`module-pw_i2c_mcuxpresso`, a new responder in
  :ref:`module-pw_spi_mcuxpresso`, and a new :ref:`module-pw_dma_mcuxpresso`
  module.

Active SEEDs
============
Help shape the future of Pigweed! Please visit :ref:`seed-0000`
and leave feedback on the RFCs (i.e. SEEDs) marked
``Open for Comments``.

.. Note: There is space between the following section headings
.. and commit lists to remind you to write a summary for each
.. section. If a summary is not needed, delete the extra
.. space.

Modules
=======

pw_allocator
------------
* `Fix data race
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208412>`__
  (issue `#333386065 <https://issues.pigweed.dev/issues/333386065>`__)
* `Add BucketBlockAllocator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198155>`__
* `Improve namespacing
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206153>`__
* `Use singletons for stateless allocators
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207337>`__

pw_assert
---------
* `Ensure condition does not contain stray %
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208140>`__
  (issue `#337268540 <https://issues.pigweed.dev/issues/337268540>`__)

pw_async2
---------
C++20 users can now define asynchronous tasks using
:ref:`module-pw_async2-coro`.

* `Move PW_CO_TRY functions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209911>`__
* `Add Coro<T> coroutine API
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207690>`__

pw_bloat
--------
Padding is now included as part of utilization in code size reports. This allows
developers to monitor changes in application size that smaller than the used
space alignment defined in the linker script.

* `Add padding to utilization
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209972>`__
  (issue `#276370736 <https://issues.pigweed.dev/issues/276370736>`__)

pw_bluetooth
------------
* `Add a constant for max controller delay
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209271>`__
* `Put cmake tests in modules group
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208897>`__
* `Disable emboss enum traits
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208591>`__
  (issue `#339029458 <https://issues.pigweed.dev/issues/339029458>`__)
* `Remove hci_vendor
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208313>`__
  (issue `#338269786 <https://issues.pigweed.dev/issues/338269786>`__)

pw_bluetooth_proxy
------------------
* `Also support V2 of LE read buffer event
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209879>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)
* `Use LE read buffer event
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209878>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)
* `Removing trailing comma in PW_LOG call
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209231>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)
* `Allow setting the # of credits to reserve
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208895>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)
* `Add cmake build rules
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208653>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)
* `Update tests to remove RVNO assumptions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208652>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)
* `Reserve ACL LE slots from host
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207671>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)

pw_boot_cortex_m
----------------
* `Emit pw_boot_Entry without prologue
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208931>`__
  (issue `#339107121 <https://issues.pigweed.dev/issues/339107121>`__)

pw_build
--------
* `pw_cc_test.lib fixup
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210231>`__
  (issue `#307825072 <https://issues.pigweed.dev/issues/307825072>`__,
  issue `#341109859 <https://issues.pigweed.dev/issues/341109859>`__)
* `Fix type hint
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208137>`__
  (issue `#338462905 <https://issues.pigweed.dev/issues/338462905>`__)

pw_build_mcuxpresso
-------------------
:ref:`module-pw_build_mcuxpresso` now can support multiple device cores.

* `Support multiple cores
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208654>`__

pw_config_loader
----------------
* `Add missing types dep
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210236>`__

pw_cpu_exception_cortex_m
-------------------------
* `Add error flag masks
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210072>`__
* `Fix PSP unit test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210071>`__

pw_digital_io_linux
-------------------
* `Add test_utils.h for ASSERT_OK and friends
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209592>`__
* `Minor updates to OwnedFd
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209591>`__
* `Refactor test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196051>`__

pw_dma_mcuxpresso
-----------------
:ref:`module-pw_dma_mcuxpresso` is a new module for working with an MCUXpresso
DMA controller.

* `Module for working with NXP DMA controller
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208655>`__

pw_docs
-------
* `Add inline search to sidebar
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207674>`__

pw_env_setup
------------
* `Remove f-strings from github_visitor
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210274>`__
* `Change Bazel library name
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210198>`__
  (issue `#340328100 <https://issues.pigweed.dev/issues/340328100>`__)
* `Add GitHub environment visitor
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210045>`__
  (issue `#340900493 <https://issues.pigweed.dev/issues/340900493>`__)
* `Bazel support for config_file
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209913>`__
  (issue `#340328100 <https://issues.pigweed.dev/issues/340328100>`__)

pw_format
---------
* `Add Rust support for field width and zero padding
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208898>`__
* `Add Rust support for formatting integers in hex
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208415>`__
* `Add test for escaped curly brackets
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208291>`__
* `Refactor format string parsing for better core::fmt support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208656>`__

pw_i2c
------
* `Update OWNERS
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208413>`__

pw_i2c_mcuxpresso
-----------------
The new ``I3cMcuxpressoInitiator`` implements the ``pw_i2c`` initiator interface
using the MCUXpresso I3C driver, allowing normal I2C API's to work after setup.

* `Fix Clang compilation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209191>`__
* `Add i3c initiator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208136>`__
* `Remove swatiwagh from OWNERS
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208671>`__

pw_ide
------
* `Add .pw_ide.user.yaml to .gitignore
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208894>`__
* `Raise specific error on bad settings file
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208132>`__
  (issue `#336799314 <https://issues.pigweed.dev/issues/336799314>`__)

pw_libcxx
---------
Added initial support for using LLVM's `libcxx <https://libcxx.llvm.org/>`__ as
a standard C++ library implementation.

* `Minimal implementation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201918>`__

pw_log
------
Logging messages with untyped string arguments is now supported in the Rust
implementation.

.. todo-check: disable

* `Make TODO actionable
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209571>`__
* `Add core::fmt style format string support to Rust API
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207331>`__
* `Rename Rust logging API to be less verbose
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207330>`__
* `Add Rust support for untyped strings
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206673>`__

.. todo-check: enable

pw_multibuf
-----------
* `Remove deprecated Chunk::DiscardFront
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206676>`__

pw_package
----------
* `Suppress package progress messages
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208331>`__

pw_presubmit
------------
* `Auto fix unsorted-dict-items
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209881>`__
  (issue `#340637744 <https://issues.pigweed.dev/issues/340637744>`__)
* `Fix missing pico-sdk for docs_build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209931>`__
* `Add repo tool API
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179230>`__
* `Exclude all OWNERS from copyright
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209031>`__

pw_proto
--------
* `Create genrule for raw rpc with prefix
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209992>`__
  (issue `#340749161 <https://issues.pigweed.dev/issues/340749161>`__)

pw_protobuf
-----------
* `Build common.proto with Nanopb+Soong
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209851>`__
  (issue `#340350973 <https://issues.pigweed.dev/issues/340350973>`__)

pw_protobuf_compiler
--------------------
* `Disable layering check less
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209111>`__
  (issue `#323749176 <https://issues.pigweed.dev/issues/323749176>`__)
* `Fix name collisions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208658>`__
* `Disable layering_check
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208932>`__
  (issue `#339280821 <https://issues.pigweed.dev/issues/339280821>`__)
* `Fix bazel failure if proto dir is empty
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208135>`__
  (issue `#328311416 <https://issues.pigweed.dev/issues/328311416>`__)
* `Add a no_prefix test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208138>`__
  (issue `#328311416 <https://issues.pigweed.dev/issues/328311416>`__)
* `Tests fail to build under cmake
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208271>`__
  (issue `#338622044 <https://issues.pigweed.dev/issues/338622044>`__)

pw_result
---------
* `Fix typo in template member
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209667>`__
  (issue `#339794389 <https://issues.pigweed.dev/issues/339794389>`__)

pw_rpc
------
* `Include FakeChannelOutput in soong target
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209991>`__
  (issue `#340350973 <https://issues.pigweed.dev/issues/340350973>`__)
* `Update docs for channel ID remapping
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209431>`__
* `Build compatibility fixes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208893>`__
* `Fix macro name in docs and comments
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208311>`__

pw_spi_mcuxpresso
-----------------
The :ref:`module-pw_spi_mcuxpresso` module now includes an
``McuxpressoResponder`` useing the SPI and DMA drivers from the NXP MCUXpresso
SDK.

* `Add responder implementation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208657>`__

pw_stm32cube_build
------------------
* `Fix a label flag name in documentation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208231>`__

pw_sys_io_rp2040
----------------
* `Bazel build file update
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209877>`__
  (issue `#261603269 <https://issues.pigweed.dev/issues/261603269>`__,
  issue `#300318025 <https://issues.pigweed.dev/issues/300318025>`__)

pw_sys_io_stdio
---------------
* `Expand allowed uses beyond host
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208511>`__

pw_sys_io_stm32cube
-------------------
* `Fix build for f1xx family
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208471>`__

pw_tokenizer
------------
* `Clean up rust docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208416>`__

pw_toolchain
------------
Pigweed's Rust toolchain can now be used by downstream projects.

* `Fix typos in newlib_os_interface_stubs.cc
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209952>`__
* `Add clang-apply-replacements plugin
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209539>`__
  (issue `#339294894 <https://issues.pigweed.dev/issues/339294894>`__)
* `Support Rust toolchains in downstream projects
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209871>`__
* `Add clang-apply-replacements plugin
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208896>`__
  (issue `#339294894 <https://issues.pigweed.dev/issues/339294894>`__)
* `Fix CMake build on macOS
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208139>`__
* `Add no-canonical-system-headers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208156>`__
  (issue `#319665090 <https://issues.pigweed.dev/issues/319665090>`__)

pw_trace_tokenized
------------------
* `Build compatibility fixes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208893>`__

pw_transfer
-----------
The transfer service now ends active transfers when the underlying stream
changes, avoiding a case where a transfer could become stuck.

* `End active transfers when RPC stream changes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209876>`__
* `Add tests for GetResourceStatus
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209753>`__
* `Add py_proto_library target for update_bundle_proto
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209731>`__
* `GetResoureStatus fix missing return
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208491>`__
* `Lock resource_responder_ access
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208251>`__

pw_unit_test
------------
* `Add missing :config dependency for gtest backend
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208472>`__

Build
=====

bazel
-----
* `Ignore reformatting change in git blame
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210111>`__
  (issue `#340637744 <https://issues.pigweed.dev/issues/340637744>`__)
* `Fix unsorted-dict-items instances
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209880>`__
  (issue `#340637744 <https://issues.pigweed.dev/issues/340637744>`__)
* `Remove unnecessary .bazelrc flag
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208659>`__
  (issue `#319665090 <https://issues.pigweed.dev/issues/319665090>`__)
* `Re-enable sandbox_hermetic_tmp
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208312>`__
  (issue `#319665090 <https://issues.pigweed.dev/issues/319665090>`__)

Targets
=======

targets/rp2040
--------------
* `pre_init and freertos config
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209554>`__
  (issue `#261603269, 300318025 <https://issues.pigweed.dev/issues/261603269, 300318025>`__)

Docs
====
* `Update changelog
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208171>`__

SEEDs
=====

SEED-0116
---------
* `Set status to On Hold
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208831>`__

Third party
===========

third_party/emboss
------------------
* `Use absolute paths in source dependencies
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/209131>`__
  (issue `#339467547 <https://issues.pigweed.dev/issues/339467547>`__)
* `Update emboss repo to tag v2024.0501.215421
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208314>`__
  (issue `#338675057 <https://issues.pigweed.dev/issues/338675057>`__)
* `Add additional owners for emboss
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208631>`__

third_party/freertos
--------------------
* `Add CM3 support to Bazel build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210211>`__

third_party/perfetto
--------------------
* `Rename proto targets
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/210040>`__
* `Add third party perfetto repo
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203618>`__
* `Copybara import
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207490>`__

Miscellaneous
=============
* `Run clang-format
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208911>`__
* `Add roller as WORKSPACE owner
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/208900>`__
  (issue `#245397913 <https://issues.pigweed.dev/issues/245397913>`__)

-----------
May 1, 2024
-----------

Highlights (Apr 19, 2024 to May 1, 2024):

* **Thread kickoff via pw::Function:** Revamped the Thread API to use
  pw::Function. The original Thread API was created before pw::Function was
  stable; this change modernizes and increases usability of the Thread API.
* **Thread creation SEED:** Creating threads in Pigweed is difficult due to our
  strict adherence to portability. We're considering creating an additional API
  that is more usable but less portable than the current approach.
* **Transfer:** Adaptive windowing got a Java implementation, and improvements
  to adaptive windowing in C++.
* **Bluetooth:** Initial CLs towards a Bluetooth proxy.

Active SEEDs
============
Help shape the future of Pigweed! Please visit :ref:`seed-0000`
and leave feedback on the RFCs (i.e. SEEDs) marked
``Open for Comments``.

Modules
=======

pw_allocator
------------
Added two new allocators: BuddyAlloctor and BumpAllocator (also known as an
arena allocator). Various cleanups and fixes, including splitting the
Block-based allocators to their own files.

* `Add missing return statement
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207171>`__
  (issue `#337761967 <https://issues.pigweed.dev/issues/337761967>`__)
* `Remove conflict marker
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206590>`__
* `Move FallbackAllocator implementation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206174>`__
* `Clean up LibCAllocatorTest
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206173>`__
* `Move block allocators to separate files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198154>`__
* `Add BuddyAllocator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195952>`__
* `Make AllMetrics internal
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205737>`__
* `Fix SynchonizedAllocator data race
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205753>`__
  (issue `#333386065 <https://issues.pigweed.dev/issues/333386065>`__)
* `Add BumpAllocator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195953>`__

pw_assert
---------
* `Fix support for print_and_abort in Bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206853>`__
  (issue `#337271435 <https://issues.pigweed.dev/issues/337271435>`__)

pw_async
--------
* `Add missing dispatcher facades to CMake
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204873>`__
  (issue `#335866562 <https://issues.pigweed.dev/issues/335866562>`__)

pw_bluetooth
------------
Continue filling out Bluetooth packet definitions; as well as some API
extensions for e.g. RSSI control.

* `Add header alias in command complete events
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207552>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)
* `Add Event Codes to emboss
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207510>`__
  (issue `#338068316 <https://issues.pigweed.dev/issues/338068316>`__)
* `Add opcode_enum to command and response event
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207335>`__
  (issue `#338068316 <https://issues.pigweed.dev/issues/338068316>`__)
* `Sync with recent APCF changes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205921>`__
  (issue `#336608891 <https://issues.pigweed.dev/issues/336608891>`__)
* `Define Common Data Types
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207470>`__
  (issue `#336608891 <https://issues.pigweed.dev/issues/336608891>`__)
* `Add LoopbackCommandEvent
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205920>`__
  (issue `#336579564 <https://issues.pigweed.dev/issues/336579564>`__)
* `Support Read RSSI command and event
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205919>`__
  (issue `#336566041 <https://issues.pigweed.dev/issues/336566041>`__)
* `Comment why we include all emboss headers in emboss_test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203637>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)
* `Correct emboss path in doc example
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204811>`__
  (issue `#326499650 <https://issues.pigweed.dev/issues/326499650>`__)

pw_bluetooth_*
--------------
* `Formatting fixes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204750>`__

pw_bluetooth_proxy
------------------
Start building the Bluetooth proxy subsystem.

* `Rename ProcessH4 to HandleH4
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207651>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)
* `Rename passthrough_test.cc to proxy_host_test.cc
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207670>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)
* `Move ProxyHost methods to .cc
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207333>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)
* `Rename HciProxy to ProxyHost
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207450>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)
* `Delete policies functionality
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207332>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)
* `Add some emboss helper functions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205741>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)
* `Template test emboss packet creation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205740>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)
* `Fix ordering of TEST arguments
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205739>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)

pw_build
--------
* `Add a `test_main` param to `pw_cc_test`
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206851>`__
  (issue `#337277617 <https://issues.pigweed.dev/issues/337277617>`__)
* `Fix using pw_cc_blob_library target in external repos
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206552>`__

pw_build_info
-------------
* `Make the python module importable
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206030>`__

pw_cli
------
New SEED creation tool reduces the burden to create SEEDs. This will open the
door to more contributors creating SEEDs for Pigweed changes and enhancements.

* `Add SEED creation to CLI tool
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186762>`__
* `Handle custom arguments in tools
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204192>`__
* `Add git_repo.py and test to Bazel build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204871>`__

pw_containers
-------------
* `Omit size on FlatMap construction
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201030>`__

pw_cpu_exception_cortex_m
-------------------------
* `Add util_test to tests
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207791>`__

pw_digital_io_linux
-------------------
* `Move OwnedFd to its own header file
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207831>`__

pw_env_setup
------------
* `Update default sysroot version
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204244>`__
  (issue `#335438711 <https://issues.pigweed.dev/issues/335438711>`__)

pw_format
---------
* `Better explain core::fmt whitespace parsing in ccomments
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206671>`__
* `Add support for core::fmt style format strings
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203830>`__

pw_grpc
-------
* `Support fragmented gRPC messages if an allocator is provided
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204431>`__
  (issue `#323924487 <https://issues.pigweed.dev/issues/323924487>`__)

pw_module
---------
* `Include :authors: in generated SEED file
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206674>`__

pw_presubmit
------------
* `Separate 'bazel info' stderr
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206350>`__
  (issue `#332357274 <https://issues.pigweed.dev/issues/332357274>`__)
* `Save 'bazel info output_base'
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206270>`__
* `Remove --verbose_explanations flag
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205933>`__
* `Use _LOG global for logging
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205913>`__
* `RST format updates
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204241>`__
* `Allow disabling hook creation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205912>`__
* `Drop '.' from Bazel symlinks
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204870>`__
  (issue `#332357274 <https://issues.pigweed.dev/issues/332357274>`__)

pw_protobuf
-----------
* `Fix support for import_prefix on protos with options
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204671>`__

pw_rpc
------
Enhancements in pw_rpc to better integrate with pw_grpc.

* `Add private method for sending internal::Packet directly
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185669>`__
  (issue `#319162657 <https://issues.pigweed.dev/issues/319162657>`__)

pw_rust
-------
* `Build examples in presubmit
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207338>`__
  (issue `#337951363 <https://issues.pigweed.dev/issues/337951363>`__)
* `Fix Rust tokenized logging example
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206672>`__

pw_sensors
----------
Sensors subsystem continues moving along; note that most of the discussion and
development is happening in the SEEDs.

* `Add support for triggers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203860>`__
  (issue `#293466822 <https://issues.pigweed.dev/issues/293466822>`__)

pw_snapshot
-----------
* `Add python processor tests
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205761>`__
* `Fix Bazel builds
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205090>`__

pw_spi_mcuxpresso
-----------------
* `Rename flexspi to flexio_spi
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205711>`__
* `Fix Bazel build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205710>`__

pw_stream_uart_mcuxpresso
-------------------------
* `Add interrupt safe write-only UART stream
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207414>`__

pw_string
---------
* `Add ToString for iterables
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206650>`__
* `Add missing array include
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204211>`__

pw_thread
---------
API change to bring pw_thread up to date with pw::Function; see overview.

* `Use pw::Function to start threads
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205760>`__
  (issue `#243018475 <https://issues.pigweed.dev/issues/243018475>`__)
* `Fix pw_thread_zephyr compilability
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206970>`__

pw_thread_threadx
-----------------
* `Remove unused dependency
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207191>`__

pw_tls_client
-------------
* `Add CMake facades
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204874>`__
  (issue `#335878898 <https://issues.pigweed.dev/issues/335878898>`__)

pw_tokenizer
------------
Enhance the C++ host-side decoder to better handle the full suite of
capabilities, in particulaur, recursive decoding; also some build fixes.

* `Add DecodeOptionallyTokenizedData
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206070>`__
* `Switch detokenize.h docs to Doxygen
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205751>`__
* `Support arbitrary recursion in C++ detokenizer
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205770>`__
* `Add missing CMake dep
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204316>`__

pw_toolchain
------------
* `Add clang-tidy suggestion
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206675>`__

pw_transfer
-----------
Adaptive windowing improvements, including C++ enhancementsn to better handle
small window sizes, as well as adding a Java implementation.

* `Add resource_id to all GetResourceStatus responses
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207350>`__
  (issue `#336364832 <https://issues.pigweed.dev/issues/336364832>`__)
* `Assume a minimum window size when reserving space
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206890>`__
* `Attempt to recover when receiving invalid size
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206632>`__
* `Implement adaptive windowing in Java
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/147511>`__

pw_unit_test
------------
* `Add support for a test_main in CMake
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204872>`__
  (issue `#335865646 <https://issues.pigweed.dev/issues/335865646>`__)
* `Standardize ASSERT_OK_AND_ASSIGN
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205946>`__
* `Clarify status macros are gunit-only
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206050>`__
* `Add IWYU export/private pragmas
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204151>`__
  (issue `#335291547 <https://issues.pigweed.dev/issues/335291547>`__)

pw_web
------
* `Fix icons in packaged version
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207551>`__
* `Fix text download format
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206551>`__
* `NPM version bump to 0.0.19
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206330>`__
* `Add user guide page for features and filter syntax
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203632>`__
  (issue `#307560371 <https://issues.pigweed.dev/issues/307560371>`__)

Docs
====

docs
----
* `Mention @deprecated in the Doxygen style guide
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205762>`__
* `Update module docs contributor guidelines
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205742>`__
* `Add GitHub pull request template
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205810>`__
* `Require Bazel+GN+CMake for new contributions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204650>`__
* `Update changelog
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204203>`__

SEEDs
=====
* SEED-0124: `Getting Used Size from Multisink
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188670>`__
  (issue `#326854807 <https://issues.pigweed.dev/issues/326854807>`__) landed
* SEED-0128: `Easier thread creation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206631>`__ started
* SEED-0129: `Support PW_ASSERT with non-argument message
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207150>`__ started

Miscellaneous
=============
* `Run clang-format
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207412>`__
* `Remove remaining usages of legacy thread entry
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206856>`__
* `Replace `string_view&` with `string_view`
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204591>`__

.bazelversion
-------------
* `Add file
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205918>`__
  (issue `#336617748 <https://issues.pigweed.dev/issues/336617748>`__)

Third party
===========
* boringssl: `Add CMake integration
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204875>`__
  (issue `#335880025 <https://issues.pigweed.dev/issues/335880025>`__)
* emboss: `Update emboss repo to tag v2024.0419.155605
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204202>`__
  (issue `#335724776 <https://issues.pigweed.dev/issues/335724776>`__)
* emboss: `Use COMPILE_LANGUAGE:CXX
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/205670>`__
  (issue `#336267050 <https://issues.pigweed.dev/issues/336267050>`__)
* fuchsia: `Add defer.h to Bazel build defs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/206854>`__
  (issue `#337275846 <https://issues.pigweed.dev/issues/337275846>`__)
* npm: `Update package-lock.json
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/207336>`__

------------
Apr 18, 2024
------------

.. changelog_highlights_start

Highlights (Apr 4, 2024 to Apr 18, 2024):

* The Python and C++ interfaces of ``pw_transfer`` now support
  :ref:`adaptive windowing <module-pw_transfer-windowing>`.
* :ref:`SEED 0117: I3C <seed-0117>` was accepted.
* The new :ref:`docs-quickstart-zephyr` shows you how to set up a
  C++-based Zephyr project that's ready to use Pigweed.

.. changelog_highlights_end

Active SEEDs
============
Help shape the future of Pigweed! Please visit :ref:`seed-0000`
and leave feedback on the RFCs (i.e. SEEDs) marked
``Open for Comments``.

Modules
=======

pw_allocator
------------
The new :cpp:class:`pw::allocator::TypedPool` class is a slab allocator
that can allocate a specific object with very low overhead.
``pw::allocator::TypedPool`` is implemented using the new
:cpp:class:`pw::allocator::Pool` interface.
``pw::allocator::TrackingAllocatorImpl`` was renamed to
``pw::allocator::TrackingAllocator``.

* `Add missing soong deps
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203613>`__
* `Add Pool
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195540>`__
* `Update OWNERS
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203211>`__
* `Rename TrackingAllocatorImpl
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203150>`__
  (issue `#326509341 <https://issues.pigweed.dev/issues/326509341>`__)

pw_async2
---------
The new :cpp:class:`pw::async2::PendableAsTask` class is a ``Task`` that
delegates to a type with a ``Pend`` method.

* `Add PendableAsTask
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201920>`__

pw_blob_store
-------------
The ``pw_add_library()`` call for the ``pw_blob_store`` target now compiles
as ``STATIC`` instead of ``INTERFACE`` to be more in line with the Bazel
build.

* `Fix CMakeLists.txt pw_add_library()
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203134>`__

pw_bluetooth
------------
* `Formatting fixes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204315>`__
* `LEGetVendorCapabilitiesCommandCompleteEvent v1.04 fields
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204198>`__
* `Add versions - LEGetVendorCapabilitiesCommandCompleteEvent
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203795>`__
  (issue `#332924521 <https://issues.pigweed.dev/issues/332924521>`__)
* `Add EventMask and temp field in SetEventMaskCommand
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192256>`__
  (issue `#42068631 <https://issues.pigweed.dev/issues/42068631>`__)
* `Store length max in virtual field
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201794>`__
* `Define LEReadMaximumAdvertisingDataLengthCommandComplete
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201951>`__
  (issue `#312898345 <https://issues.pigweed.dev/issues/312898345>`__)

pw_bluetooth_sapphire
---------------------
In CIPD ``bt-host`` artifacts are now uploaded to ``fuchsia/prebuilt/bt-host``.

* `Iterators
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203855>`__
  (issue `#333448202 <https://issues.pigweed.dev/issues/333448202>`__)
* `Change CIPD upload path
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202690>`__
  (issue `#321267610 <https://issues.pigweed.dev/issues/321267610>`__)
* `Bump @fuchsia_sdk
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202490>`__
  (issue `#329933586 <https://issues.pigweed.dev/issues/329933586>`__,
  issue `#321267476 <https://issues.pigweed.dev/issues/321267476>`__)

pw_build
--------
* `Disable deprecated pragma warnings
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203856>`__
  (issue `#333448202 <https://issues.pigweed.dev/issues/335328444>`__,
  issue `#333448202 <https://issues.pigweed.dev/issues/333448202>`__)
* `Iterators
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203330>`__
  (issue `#333448202 <https://issues.pigweed.dev/issues/333448202>`__,
  issue `#335024633 <https://issues.pigweed.dev/issues/335024633>`__,
  issue `#335021928 <https://issues.pigweed.dev/issues/335021928>`__)
* `Collect wheel fix
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202921>`__
* `Disable C23 extension warnings
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202830>`__
  (issue `#333712899 <https://issues.pigweed.dev/issues/333712899>`__)

pw_build_android
----------------
* `Update module guidance
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203910>`__
* `Update cc_defaults guidance
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203651>`__
* `Make Common Backends static
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202090>`__
  (issue `#331458726 <https://issues.pigweed.dev/issues/331458726>`__)

pw_build_info
-------------
* `Add missing header file for cmake
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202210>`__

pw_cli
------
The following interfaces were moved from ``pw_presubmit`` to ``pw_cli``
to make them more widely available: :py:class:`pw_cli.file_filter.FileFilter`,
:py:mod:`pw_cli.git_repo`, and :py:class:`pw_cli.tool_runner.ToolRunner`.
The new :py:func:`pw_cli.decorators.deprecated` decorator emits a
deprecation warning when the annotated function is used.

* `Fix argument handling for GitRepo.has_uncommitted_changes()
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204232>`__
* `Fix subprocess runner arg concatenation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202844>`__
* `Move FileFilter
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194617>`__
* `Fix commit fallback handling for GitRepo.list_files()
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203790>`__
* `Move git_repo to pw_cli
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201279>`__
* `Add Python deprecation decorator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202929>`__
* `Update ToolRunner type hints
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202737>`__
* `Move ToolRunner
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201278>`__

pw_containers
-------------
* `Iterators
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203330>`__
  (issue `#335021928 <https://issues.pigweed.dev/issues/333448202, b/335024633, b/335021928>`__)

pw_cpu_exception_risc_v
-----------------------
The new :ref:`module-pw_cpu_exception_risc_v` backend lays the foundation for
RISC-V CPU exception handling.

* `Add initial backend structure
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188230>`__

pw_env_setup
------------
* `clang
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202591>`__
  (issue `#333448202 <https://issues.pigweed.dev/issues/333448202>`__)

pw_hdlc
-------
* `Iterators
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203330>`__
  (issue `#335021928 <https://issues.pigweed.dev/issues/333448202, b/335024633, b/335021928>`__)

pw_ide
------
* `Fixes to support changing working_dir
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204430>`__
  (issue `#335628872 <https://issues.pigweed.dev/issues/335628872>`__)
* `Enable cmake.format.allowOptionalArgumentIndentation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203638>`__

pw_kvs
------
* `Make Key an alias for string_view
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204590>`__
* `Depend on libraries for fake flash and store tests for Bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202212>`__
* `Add libraries to reuse partition and store tests for Bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202211>`__

pw_log_zephyr
-------------
* `Tokenize Zephyr shell fprintf
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202734>`__

pw_presubmit
------------
The following interfaces were moved from ``pw_presubmit`` to ``pw_cli``
to make them more widely available: :py:class:`pw_cli.file_filter.FileFilter`,
:py:mod:`pw_cli.git_repo`, and :py:class:`pw_cli.tool_runner.ToolRunner`.

* `Move FileFilter
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194617>`__
* `Add bthost_package step
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203797>`__
  (issue `#332357274 <https://issues.pigweed.dev/issues/332357274>`__)
* `Don't overwrite Bazel stdout files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203796>`__
  (issue `#332357274 <https://issues.pigweed.dev/issues/332357274>`__)
* `Remove cmake_clang from quick presubmit
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198050>`__
* `Move git_repo to pw_cli
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201279>`__
* `Fix copy/paste bug in _value()
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202913>`__
* `Move ToolRunner
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201278>`__

pw_protobuf
-----------


* `Access raw proto values; change RPC packet channel ID
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204152>`__

pw_rpc
------
The new :cpp:func:`pw::rpc::ChangeEncodedChannelId` function lets you rewrite
an encoded packet's channel ID in place. See :ref:`module-pw_rpc-remap`.

* `Access raw proto values; change RPC packet channel ID
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204152>`__
* `Iterators
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203330>`__
  (issue `#335021928 <https://issues.pigweed.dev/issues/333448202, b/335024633, b/335021928>`__)

pw_rpc_transport
----------------
* `Soong lib names now follow style
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203650>`__

pw_sensor
---------
The new :ref:`module-pw_sensor` module is the start of the implementation
of :ref:`SEED 0119: Sensors <seed-0119>`.

* `Fix Python install
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204130>`__
* `Add attribute support to sensor-desc CLI
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203791>`__
  (issue `#293466822 <https://issues.pigweed.dev/issues/293466822>`__)
* `Create a sensor-desc CLI
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203670>`__
  (issue `#293466822 <https://issues.pigweed.dev/issues/293466822>`__)
* `Update validator schema to JSON schema
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202925>`__
  (issue `#293466822 <https://issues.pigweed.dev/issues/293466822>`__)
* `Provide a validator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202912>`__
  (issue `#293466822 <https://issues.pigweed.dev/issues/293466822>`__)
* `Add module stub
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202911>`__
  (issue `#293466822 <https://issues.pigweed.dev/issues/293466822>`__)

pw_snapshot
-----------
* `Process snapshots based on CPU architecture
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188232>`__

pw_spi_linux
------------
``pw_spi_linux`` now has a basic :ref:`module-pw_spi_linux-cli` that lets
you read from and write to devices.

* `Add pw_spi_linux_cli
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201391>`__

pw_thread_freertos
------------------
* `Use TCB for running stack pointer
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188231>`__

pw_tls_client
-------------
* `Only include <sys/time.h> if available
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202555>`__

pw_transfer
-----------
The Python and C++ interfaces now support
:ref:`adaptive windowing <module-pw_transfer-windowing>`.

* `Iterators
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203330>`__
  (issue `#335021928 <https://issues.pigweed.dev/issues/333448202, b/335024633, b/335021928>`__)
* `Implement adaptive windowing in Python
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/147510>`__
* `Implement adaptive windowing in C++
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/146392>`__
* `Convert arguments to std::fstream constructors
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203151>`__
  (issue `#333957637 <https://issues.pigweed.dev/issues/333957637>`__)

pw_web
------
* `Support creating client without using proto descriptor
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203654>`__
* `NPM version bump to 0.0.18
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203636>`__
* `Fix string manipulation in download logs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/203612>`__
  (issue `#331480903 <https://issues.pigweed.dev/issues/331480903>`__)
* `Use existing col data when adding new View
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200290>`__
  (issue `#331439176 <https://issues.pigweed.dev/issues/331439176>`__)
* `Enable column order on init
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201530>`__
  (issue `#329712468 <https://issues.pigweed.dev/issues/329712468>`__)
* `Fix test format of log-source.test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202392>`__
  (issue `#333379333 <https://issues.pigweed.dev/issues/333379333>`__)

Build
=====

Bazel
-----
* `Use remote cache in infra
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202754>`__
  (issue `#312215590 <https://issues.pigweed.dev/issues/312215590>`__)

Docs
====
The new :ref:`docs-quickstart-zephyr` shows you how to set up a C++-based
Zephyr project that's ready to use Pigweed. The API references for all
functions or methods that return a set of ``pw_status`` codes have been
refactored for consistency. The :ref:`docs-style-doxygen` has been revamped.

* `Add pw_status table for API references
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202739>`__
* `Revamp Doxygen style guide
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202590>`__
* `Add Zephyr quickstart
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196671>`__
* `Update changelog
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202150>`__

SEEDs
=====
* (SEED-0117) `pw_i3c
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178350>`__

Miscellaneous
=============
* (clang) `Fix \`std::array\` iterators
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202834>`__
  (issue `#333448202 <https://issues.pigweed.dev/issues/333448202>`__)
* (emboss) `Loosen Emboss cmake dependency tracking
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202831>`__
  (issue `#333735460 <https://issues.pigweed.dev/issues/333735460>`__)
* (many) `Move maxDiff to be a class attribute
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/204150>`__

-----------
Apr 4, 2024
-----------
.. _epoll: https://man7.org/linux/man-pages/man7/epoll.7.html

Highlights (Mar 21, 2024 to Apr 4, 2024):

* **New modules**: :ref:`module-pw_i2c_rp2040` is a Pico SDK implementation of
  the ``pw_i2c`` interface, :ref:`module-pw_async2_epoll` is an
  `epoll`_-based backend for ``pw_async2``, :ref:`module-pw_spi_linux`
  is a Linux backend for ``pw_spi``, :ref:`module-pw_uart` provides
  core methods for UART communication, and :ref:`module-pw_bluetooth_proxy`
  provides a lightweight proxy host that can be placed between a Bluetooth
  host and a Bluetooth controller to add functionality or inspection.
* **Docs updates**: Pigweed's main docs builder now builds the
  :ref:`examples <seed-0122-examples>` repo; the examples will be available
  at ``https://pigweed.dev/examples``. An experimental complete Doxygen API
  reference is now being published to ``https://pigweed.dev/doxygen``. The
  :ref:`module-pw_i2c` docs, :ref:`docs-style-rest`, and
  :ref:`docs contributors homepage <docs-contrib-docs>` have been revamped.
* **Android platform updates**: Many modules were refactored to follow the
  guidance in :ref:`module-pw_build_android` to make it easier to build them in
  Soong.

Active SEEDs
============
Help shape the future of Pigweed! Please visit :ref:`seed-0000`
and leave feedback on the RFCs (i.e. SEEDs) marked
``Open for Comments``.

Modules
=======

pw_allocator
------------
The new :ref:`module-pw_allocator-api-capabilities` API lets derived allocators
describe what optional features they support. ``pw::Allocator::GetLayout()``
has begun to be deprecated and replaced by ``pw::Allocator::GetRequestedLayout``,
``pw::Allocator::GetUsableLayout()``, and ``pw::Allocator::GetAllocatedLayout()``
to make it easier to distinguish between requested memory, usable memory, and
already used memory. Methods that took ``Layout`` arguments, such as
``pw::Allocator::GetRequestedLayout()``, have been deprecated.

* `Restore DoDeallocate with Layout
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201670>`__
  (issue `#332510307 <https://issues.pigweed.dev/issues/332510307>`__)
* `Move Layout and UniquePtr to their own header files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199534>`__
* `Remove Layout from Deallocate and Resize
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198153>`__
* `Add allocation detail storage to TrackingAllocator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198152>`__
* `Distinguish between requested, usable, and allocated sizes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198150>`__
* `Add Capabilities
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197133>`__

pw_assert_log
-------------
* `Follow Soong guidelines
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197536>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)

pw_async2
---------
The new :cpp:class:`pw::async2::PendFuncTask` class delegates a task to a
provided function.

* `Add Poll::Readiness helper
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201910>`__
* `Fix TSAN for dispatcher_thread_test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201850>`__
* `Add PendFuncTask
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199714>`__

pw_async2_epoll
---------------
.. _epoll: https://man7.org/linux/man-pages/man7/epoll.7.html

The new :ref:`module-pw_async2_epoll` module is an `epoll`_-based backend
for  :ref:`module-pw_async2`.

* `Epoll-backed async2 dispatcher
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200233>`__

pw_bluetooth
------------
The :ref:`module-pw_bluetooth-emboss-usage` section now shows CMake usage and
the new :ref:`module-pw_bluetooth-emboss-contributing` section shows how to
contribute Emboss code.

* `Define LEReadMaximumAdvertisingDataLengthCommandComplete
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201951>`__
  (issue `#312898345 <https://issues.pigweed.dev/issues/312898345>`__)
* `Add more opcodes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201130>`__
* `Add example of using to_underlying
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200970>`__
  (issue `#326499650 <https://issues.pigweed.dev/issues/326499650>`__)
* `Emboss formatting tweak
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200230>`__
  (issue `#331195584 <https://issues.pigweed.dev/issues/331195584>`__)
* `Add cmake to usage guide
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200012>`__
  (issue `#326499587 <https://issues.pigweed.dev/issues/326499587>`__)
* `Add opcode_full field to emboss HCI headers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199553>`__
  (issue `#326499650 <https://issues.pigweed.dev/issues/326499650>`__)
* `Add enum for opcodes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199671>`__
  (issue `#326499650 <https://issues.pigweed.dev/issues/326499650>`__)
* `Update cmake targets to be consistent
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200011>`__
  (issue `#326499587 <https://issues.pigweed.dev/issues/326499587>`__)
* `Update build files to be consistent
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200010>`__
  (issue `#326499587 <https://issues.pigweed.dev/issues/326499587>`__)
* `Add emboss ReadBufferSize v1 event
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199070>`__
  (issue `#326499650 <https://issues.pigweed.dev/issues/326499650>`__)
* `Add emboss contributing section to docs.rst
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199531>`__
  (issue `#331195584 <https://issues.pigweed.dev/issues/331195584>`__)
* `protocol.h comments tweak
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199532>`__
  (issue `#326499650 <https://issues.pigweed.dev/issues/326499650>`__)

pw_bluetooth_proxy
------------------
The new :ref:`module-pw_bluetooth_proxy` module provides a lightweight
proxy host that can be placed between a Bluetooth host and a Bluetooth
controller to add functionality or inspection.

* `Move to cpp23::to_underlying
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200971>`__
  (issue `#331281133 <https://issues.pigweed.dev/issues/331281133>`__)
* `Use emboss OpCode enum
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199554>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)
* `Bluetooth proxy module and initial classes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197290>`__
  (issue `#326496952 <https://issues.pigweed.dev/issues/326496952>`__)

pw_bluetooth_sapphire
---------------------
``pw_bluetooth_sapphire`` now supports emulation, Fuchsia unit testing, and
ARM64 build targets.

* `Add arm64 release variant
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202050>`__
  (issue `#332928957 <https://issues.pigweed.dev/issues/332928957>`__)
* `Stub bt-host CIPD manifest
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201390>`__
  (issue `#332357274, 321267610 <https://issues.pigweed.dev/issues/332357274, 321267610>`__)
* `Fuchsia testing support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198270>`__
  (issue `#331692493, 42178254 <https://issues.pigweed.dev/issues/331692493, 42178254>`__)
* `Add emulator start workflow
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200270>`__
  (issue `#321267689 <https://issues.pigweed.dev/issues/321267689>`__)

pw_build
--------
Modules can now be nested in subdirectories, which paves the way for
refactoring how modules are organized in the upstream Pigweed repo.
:ref:`module-pw_build-project_builder` is a new lightweight build command
for projects that need to run multiple commands to perform a build.

* `Allow nesting Pigweed modules in subdirectories
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201114>`__
* `Add alwayslink option to pw_cc_blob_library
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201110>`__
* `ProjectBuilder documentation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200791>`__
* `BuildRecipe auto_create_build_dir option
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200830>`__
* `Defer build directory existence check
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200771>`__

pw_build_android
----------------
* `Define rule with static libs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200351>`__
  (issue `#331458726 <https://issues.pigweed.dev/issues/331458726>`__)

pw_build_info
-------------
The new ``pw::build_info::LogBuildId()`` function lets you print a GNU
build ID as hex.

* `Add log function of GNU build ID
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199471>`__
* `Fix Bazel baremetal compatibility
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199470>`__

pw_bytes
--------
* `Add example to docs of using _b suffix
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201350>`__

pw_channel
----------
The new :cpp:type:`pw::channel::LoopbackDatagramChannel` and
:cpp:type:`pw::channel::LoopbackByteChannel` aliases provide channel
implementations that read their own writes. The new
:cpp:class:`pw::channel::ForwardingChannelPair` class lets you connect two
subsystems with datagram channels without implementing a custom channel.

* `Return status from PollReadyToWrite
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200995>`__
* `Rename methods to Pend prefix
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201090>`__
* `Add loopback channel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199150>`__
* `Split open bits for read and write
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199712>`__
* `Seek is not async
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199713>`__
* `Set closed bit on FAILED_PRECONDITION
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199710>`__
* `Respect sibling closure
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199035>`__
* `Introduce forwarding channels
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197353>`__

pw_chrono
---------
* `Update OWNERS
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200714>`__
* `Follow Soong guidelines
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198290>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)

pw_cli
------
The new ``pw_cli.alias`` Python module lets you create ``pw`` subcommands
that are effectively command line aliases. See :ref:`module-pw_cli-aliases`.

* `Move plural()
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201630>`__
* `Move status_reporter to pw_cli
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201113>`__
* `Add pw ffx alias
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200770>`__
  (issue `#329933586 <https://issues.pigweed.dev/issues/329933586>`__)

pw_digital_io
-------------
* `Update OWNERS
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200752>`__

pw_emu
------
* `Fix a TypeError in TemporaryEmulator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200832>`__
  (issue `#316080297 <https://issues.pigweed.dev/issues/316080297>`__)

pw_env_setup
------------
* `Run npm log viewer setup script after install
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200211>`__

pw_function
-----------
* `Define as cc_static_library
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199092>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)

pw_hdlc
-------
The new :cpp:class:`pw::hdlc::Router` class is an experimental async HDLC
router that uses :ref:`module-pw_channel`.

* `Document members of router
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201115>`__
* `Add async router
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195538>`__
* `Fix sitenav
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196499>`__

pw_i2c
------
The :ref:`module-pw_i2c` docs have been revamped.

* `Revamp docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196330>`__
* `Update OWNERS
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200752>`__
* `Update OWNERS
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200714>`__

pw_i2c_rp2040
-------------
The new :ref:`module-pw_i2c_rp2040` module implements the :ref:`module-pw_i2c`
interface using the Raspberry Pi Pico SDK.

* `Initiator implementation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173552>`__
  (issue `#303255049 <https://issues.pigweed.dev/issues/303255049>`__)

pw_ide
------
:py:func:`pw_ide.settings.PigweedIdeSettings.compdb_searchpaths` now accepts
globs. The new :py:func:`pw_ide.settings.PigweedIdeSettings.targets_exclude`
method lets you specify a list of GN targets that code analysis should ignore.

* `Support comp DB search path globs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200908>`__
* `Move status_reporter to pw_cli
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201113>`__
* `Support including and/or excluding targets
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195975>`__

pw_libc
-------
* `Include strncpy
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199110>`__
  (issue `#316936782 <https://issues.pigweed.dev/issues/316936782>`__)

pw_log
------
* `Follow Soong guidelines
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197536>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)

pw_log_basic
------------
* `Fix Soong definitions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199034>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)

pw_log_null
-----------
* `Define as cc_static_library
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199090>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)

pw_log_tokenized
----------------
* `Define as cc_static_library
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198735>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)

pw_minimal_cpp_stdlib
---------------------
* `Clarify purpose
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200792>`__

pw_module
---------
* `Jinja template refactor
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201751>`__
* `Overwrite prompt with diff display
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201851>`__
* `Allow nesting Pigweed modules in subdirectories
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201114>`__
* `Add OWNERS file during module creation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200831>`__

pw_multibuf
-----------
* `Replace Mutex with ISL
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200996>`__
* `Define as cc_static_library
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199091>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)

pw_polyfill
-----------
* `Define as cc_static_library
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199094>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)
* `Simplify backported features table
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197891>`__

pw_preprocessor
---------------
* `Define as cc_static_library
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199031>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)

pw_presubmit
------------
Pigweed's main docs builder now builds the :ref:`examples <seed-0122-examples>`
repo; the examples will be available at ``https://pigweed.dev/examples``.
An experimental complete Doxygen API reference is now being published to
``https://pigweed.dev/doxygen``.

* `Include examples repo docs in docs_builder
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201990>`__
  (issue `#300317433 <https://issues.pigweed.dev/issues/300317433>`__)
* `Move plural()
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201630>`__
* `Include doxygen html in docs_build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198553>`__
* `Refactor Python Black formatter support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194417>`__
* `Refactor Bazel formatter support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194416>`__
* `Refactor GN formatting support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194415>`__
  (issue `#326309165 <https://issues.pigweed.dev/issues/326309165>`__)
* `Make ToolRunner capture stdout/stderr by default
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200972>`__
* `Update buildifier invocation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200350>`__
* `Switch format test data to importlib
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200790>`__
* `Skip gn_teensy_build on mac-arm64
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199670>`__

pw_protobuf
-----------
* `Support full java protos
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200530>`__
  (issue `#329445249 <https://issues.pigweed.dev/issues/329445249>`__)

pw_python
---------
* `Update setup.sh requirements
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200994>`__

pw_result
---------
* `Avoid duplicate symbols with Soong
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201277>`__
  (issue `#331458726 <https://issues.pigweed.dev/issues/331458726>`__)
* `Define as cc_static_library
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199033>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)

pw_router
---------
* `Define as cc_static_library
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199130>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)
* `Add Android common backends as dep
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198390>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)

pw_rpc
------
* `List dependencies directly
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199533>`__
  (issue `#331226283 <https://issues.pigweed.dev/issues/331226283>`__)

pw_rpc_transport
----------------
* `Define as cc_static_library
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199093>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)

pw_span
-------
* `Define as cc_static_library
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199032>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)

pw_spi
------
* `Update OWNERS
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200752>`__
* `Update OWNERS
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200714>`__

pw_spi_linux
------------
Linux functionality that was previously in :ref:`module-pw_spi` has been
moved to its own module, :ref:`module-pw_spi_linux`.

* `Move linux_spi from pw_spi to its own module
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201351>`__

pw_status
---------
The new ``pw::StatusWithSize::size_or()`` convenience method lets you return
a default size in place of ``pw::StatusWithSize::size()`` when the status is
not OK.

* `Add StatusWithSize::size_or
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198151>`__

pw_stream_shmem_mcuxpresso
--------------------------
* `Fix interrupt pending check
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198070>`__
  (issue `#330225861 <https://issues.pigweed.dev/issues/330225861>`__)

pw_sync
-------
* `Follow Soong guidelines
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197872>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)

pw_sys_io
---------
* `Update OWNERS
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200714>`__
* `Fix Soong definitions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199034>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)

pw_thread
---------
* `Follow Soong guidance
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199030>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)

pw_toolchain
------------


* `Fix Rust GN host build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201831>`__
* `Define as cc_static_library
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198734>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)
* `Remove unusued source set
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200190>`__
  (issue `#331260098 <https://issues.pigweed.dev/issues/331260098>`__)
* `LLVM compiler-rt builtins
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198593>`__

pw_trace_tokenized
------------------
* `Fix static initialization
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200232>`__

pw_transfer
-----------
New ``pw_transfer`` macros:
:c:macro:`PW_TRANSFER_LOG_DEFAULT_CHUNKS_BEFORE_RATE_LIMIT`,
:c:macro:`PW_TRANSFER_LOG_DEFAULT_RATE_PERIOD_MS`,
:c:macro:`PW_TRANSFER_CONFIG_LOG_LEVEL`, and
:c:macro:`PW_TRANSFER_CONFIG_DEBUG_DATA_CHUNKS`.

* `Make numerous logging adjustments
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194750>`__

pw_uart
-------
The new :ref:`module-pw_uart` module defines core methods for UART
communication.

* `Create OWNERS
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200750>`__
* `Added UART interface
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181710>`__

pw_unit_test
------------
* `Add failing results test record
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197852>`__

pw_web
------
* `NPM version bump to 0.0.17
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201091>`__
* `Fix logs not appearing in pw_console server
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200793>`__
  (issue `#331483789 <https://issues.pigweed.dev/issues/331483789>`__)

Build
=====

Bazel
-----
* `Add missing Python deps
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199850>`__
  (issue `#331267896 <https://issues.pigweed.dev/issues/331267896>`__)
* `Localize remaining backend label flags
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199036>`__
  (issue `#329441699 <https://issues.pigweed.dev/issues/329441699>`__)

Docs
====
* `Mention that Windows flow needs admin rights
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/202030>`__
* `Update reST style guide
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201650>`__
* `Organize the documentation style guides
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201116>`__
* `Update references to quickstart/bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200131>`__
  (issue `#325472122 <https://issues.pigweed.dev/issues/325472122>`__)
* `Simplify module creation docs using script
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200231>`__
* `Generate doxygen html output locally
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199711>`__
* `Update changelog
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198810>`__

SEEDs
=====
* (SEED-0117) `Update status to Last Call
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200710>`__
* (SEED-0126) `Claim SEED number
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200911>`__
* (SEED-0127) `Reading sensor data
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198134>`__

Third party
===========
* (Emboss) `Assume newer emboss version 2/2
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197363>`__
  (issue `#329872338 <https://issues.pigweed.dev/issues/329872338>`__)
* (FreeRTOS) `Fix typo in docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/201330>`__
* (Fuchsia) `Copybara import
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200712>`__
  (issue `#331281133 <https://issues.pigweed.dev/issues/331281133>`__)

Miscellaneous
=============
* `Delete move constructors of buffer wrappers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200753>`__
* (Soong) `Remove _headers from lib names
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198330>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)
* (mbedtls) `Avoid the use of unsupported libc functions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/199131>`__
  (issue `#316936782 <https://issues.pigweed.dev/issues/316936782>`__)
* (nanopb) `Fix nanopb_pb2.py generation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/200772>`__

------------
Mar 22, 2024
------------
Highlights (Mar 7, 2024 to Mar 22, 2024):

* Pigweed's minimum supported Python version was changed to 3.10.
* Setting the new ``pw_build_TEST_TRANSITIVE_PYTHON_DEPS`` flag to ``false``
  in your project's ``.gn`` file turns off testing and linting of transitive
  dependencies in ``pw_python_package`` rules, which can speed up build
  times significantly.
* The new :ref:`module-pw_log_android` module is a ``pw_log`` backend for
  Android and the new :ref:`module-pw_build_android` module provides tools to
  help build Pigweed in Android platform applications.
* :ref:`seed-0120` introduces ``pw_sensor``, a module that will handle
  Pigweed's upcoming sensor framework.
* The new :c:macro:`PW_LOG_EVERY_N` and :c:macro:`PW_LOG_EVERY_N_DURATION`
  macros provide rate-limited logging.

Active SEEDs
============
Help shape the future of Pigweed! Please visit :ref:`seed-0000`
and leave feedback on the RFCs (i.e. SEEDs) marked
``Open for Comments``.

Modules
=======

pw_allocator
------------
.. _//pw_allocator/examples: https://cs.opensource.google/pigweed/pigweed/+/main:pw_allocator/examples

The :ref:`module-pw_allocator` docs have been revamped. Code examples from
the docs are now extracted from complete examples that are built and tested
alongside the rest of the main Pigweed repo; see `//pw_allocator/examples`_.

* `Improve size report accuracy
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196492>`__
* `Add buffer utilities
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195353>`__
* `Improve UniquePtr ergonomics
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196181>`__
* `Various API modifications
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195973>`__
* `Add IsEqual
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195954>`__
* `Soft-deprecate heap_viewer.py
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195253>`__
  (issue `#328648868 <https://issues.pigweed.dev/issues/328648868>`__)
* `Move code snippets from docs to examples
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195190>`__
  (issue `#328076428 <https://issues.pigweed.dev/issues/328076428>`__)
* `Clean up sources files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194948>`__
* `Remove erroneous quotes around tagline
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195514>`__
* `Refactor code size reports
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194947>`__
* `Remove metrics.cc
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195450>`__
  (issue `#277108894 <https://issues.pigweed.dev/issues/277108894>`__)
* `Fix move semantics for UniquePtr
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195470>`__
* `Refactor docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194871>`__
  (issue `#328076428 <https://issues.pigweed.dev/issues/328076428>`__)
* `Make metrics configurable
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193621>`__
  (issue `#326509341 <https://issues.pigweed.dev/issues/326509341>`__)

pw_assert
---------
* `Add keep_dep tags to backend_impl
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197531>`__
  (issue `#329441699 <https://issues.pigweed.dev/issues/329441699>`__)
* `Introduce :backend, :backend_impl
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196531>`__
  (issue `#329441699 <https://issues.pigweed.dev/issues/329441699>`__)
* `Apply formatting fixes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195951>`__

pw_assert_basic
---------------
* `Fix BUILD.bazel file
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196292>`__
  (issue `#328679085 <https://issues.pigweed.dev/issues/328679085>`__)

pw_bluetooth
------------
* `Add command complete event
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196030>`__
  (issue `#311639690 <https://issues.pigweed.dev/issues/311639690>`__)
* `Add command complete event
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195451>`__
  (issue `#311639690 <https://issues.pigweed.dev/issues/311639690>`__)
* `Add H4 packet indicators in emboss
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195535>`__
  (issue `#326499682 <https://issues.pigweed.dev/issues/326499682>`__)
* `Reformat l2cap_frames.emb
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194790>`__

pw_bluetooth_sapphire
---------------------
* `Use amd64 SDK
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197510>`__
  (issue `#330214852 <https://issues.pigweed.dev/issues/330214852>`__)
* `Fuchsia SDK example
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196450>`__
  (issue `#42178254 <https://issues.pigweed.dev/issues/42178254>`__)

pw_build
--------
Setting the new ``pw_build_TEST_TRANSITIVE_PYTHON_DEPS`` flag to ``false``
in your project's ``.gn`` file turns off testing and linting of transitive
dependencies in ``pw_python_package`` rules, which can speed up build
times significantly.

* `Option to not transitively run py .tests and .lint deps
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186195>`__

pw_build_android
----------------
The new :ref:`module-pw_build_android` module provides tools to help build
Pigweed in Android platform applications.

* `Add new utils module
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195139>`__
  (issue `#328503970 <https://issues.pigweed.dev/issues/328503970>`__)

pw_bytes
--------
The new :cpp:func:`pw::bytes::ExtractBits` helper extracts bits between
specified left bit and right bit positions. New Rust helpers were added;
see `Crate pw_bytes <./rustdoc/pw_bytes>`_.

* `Add ExtractBits template
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196310>`__
  (issue `#329435173 <https://issues.pigweed.dev/issues/329435173>`__)
* `Add Rust helpers for contcatenating const slices and strs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187651>`__

pw_channel
----------
Datagram-to-byte conversions must now be explicit.

* `Enable GetWriteAllocator function
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197534>`__
* `Require explicit datagram-to-byte conversions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197650>`__
* `Remove max_bytes argument from ReadPoll
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197352>`__
* `Support datagram-to-byte conversions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196210>`__
* `Handle closed channels in base
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194739>`__

pw_checksum
-----------
* `Add missing #include <array>
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196530>`__
  (issue `#329594026 <https://issues.pigweed.dev/issues/329594026>`__)

pw_chre
-------
* `Remove unused files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170792>`__

pw_containers
-------------
* `ConstexprSort in FlatMap takes an iterator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197217>`__
  (issue `#330072104 <https://issues.pigweed.dev/issues/330072104>`__)
* `Add move constructors to queues
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197072>`__

pw_digital_io_linux
-------------------
The new ``pw_digital_io_linux`` CLI tool lets you configure a GPIO line as an
input and gets its value, or configure a line as an output and set its value.

* `Add Android.bp
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194432>`__
* `Add test CLI
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194431>`__

pw_docgen
---------
* `Single-source the module metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193333>`__
  (issue `#292582625 <https://issues.pigweed.dev/issues/292582625>`__)

pw_env_setup
------------
Pigweed's minimum supported Python version was changed to 3.10.

* `Update min Python version to 3.10
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197332>`__
  (issue `#248257406 <https://issues.pigweed.dev/issues/248257406>`__)
* `Update CIPD rust version
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194620>`__
* `Use amd64 SDK
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197510>`__
  (issue `#330214852 <https://issues.pigweed.dev/issues/330214852>`__)
* `Fuchsia SDK example
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196450>`__
  (issue `#42178254 <https://issues.pigweed.dev/issues/42178254>`__)

pw_format
---------
Initital support for untyped specifiers (``%v``) was added.

* `Enhance Rust tests to check for arguments
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196433>`__
  (issue `#329507809 <https://issues.pigweed.dev/issues/https://pwbug.dev/329507809>`__)
* `Add initial support for untyped specifiers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187650>`__

pw_hdlc
-------
The newly public :cpp:class:`pw::hdlc::Encoder` class supports gradually
encoding frames without ever holding an entire frame in memory at once.

* `Expose Encoder
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197535>`__

pw_hex_dump
-----------
CMake support was added.

* `Add CMake support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198170>`__

pw_json
-------
* `Move examples outside of the pw namespace
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195890>`__

pw_libc
-------
* `Define LIBC_FAST_MATH for the faster integral fixed point sqrt
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196350>`__
  (issue `#323425639 <https://issues.pigweed.dev/issues/323425639>`__)
* `Add uksqrtui to stdfix
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196230>`__
  (issue `#323425639 <https://issues.pigweed.dev/issues/323425639>`__)
* `Include sqrtur and expk in stdfix
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195256>`__
  (issue `#323425639 <https://issues.pigweed.dev/issues/323425639>`__)

pw_log
------
The new :c:macro:`PW_LOG_EVERY_N` and :c:macro:`PW_LOG_EVERY_N_DURATION`
macros provide rate-limited logging.

* `Add rate limit log statements
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183870>`__
* `Add keep_dep tags to backend_impl
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197531>`__
  (issue `#329441699 <https://issues.pigweed.dev/issues/329441699>`__)
* `Introduce localized backend label flags
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196498>`__
  (issue `#329441699 <https://issues.pigweed.dev/issues/329441699>`__)
* `Run bpfmt on all Android.bp
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195490>`__
  (issue `#277108894 <https://issues.pigweed.dev/issues/277108894>`__)

pw_log_android
--------------
The new :ref:`module-pw_log_android` module is a ``pw_log`` backend for
Android.

* `Fix Soong build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197830>`__
* `Add pw_log_android_stderr
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195050>`__
  (issue `#328281789 <https://issues.pigweed.dev/issues/328281789>`__)
* `Add module documentation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196410>`__

pw_malloc
---------
* `Add backend label flags
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196610>`__
  (issue `#329441699 <https://issues.pigweed.dev/issues/329441699>`__)

pw_multibuf
-----------
:cpp:class:`pw::multibuf::Stream` is a new multibuf-backed ``pw_stream``
implementation that can read from and write to a multibuf.
:cpp:class:`pw::multibuf::SimpleAllocator` is a simple, first-fit variant
of :cpp:class:`pw::multibuf::MultiBufAllocator`.

* `Add stream implementation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196354>`__
* `Add empty() function
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197351>`__
* `Pass reference instead of pointer
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197132>`__
* `Add SimpleAllocator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195129>`__

pw_package
----------
* `Match Emboss version used by Bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197359>`__
  (issue `#329872338 <https://issues.pigweed.dev/issues/329872338>`__)

pw_polyfill
-----------
* `Update __cplusplus macro for C++23; support C
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196113>`__
* `Remove PW_INLINE_VARIABLE
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196122>`__
* `Detect C23
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195266>`__
  (issue `#326499611 <https://issues.pigweed.dev/issues/326499611>`__)

pw_presubmit
------------
* `Suppress stdout option for rst_format
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197890>`__
* `Begin formatter modularization
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193714>`__
  (issue `#326309165 <https://issues.pigweed.dev/issues/326309165>`__)
* `Switch default formatter to black
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190453>`__
  (issue `#261025545 <https://issues.pigweed.dev/issues/261025545>`__)
* `Fuchsia SDK example
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196450>`__
  (issue `#42178254 <https://issues.pigweed.dev/issues/42178254>`__)

pw_rpc
------
* `Support full java protos
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196231>`__
  (issue `#329445249 <https://issues.pigweed.dev/issues/329445249>`__)
* `Move some headers from srcs to hdrs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196850>`__
  (issue `#323749176 <https://issues.pigweed.dev/issues/323749176>`__)
* `Add TryFinish API for pw_rpc stream
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195126>`__
  (issue `#328462705 <https://issues.pigweed.dev/issues/328462705>`__)
* `Remove deprecated functions from Java client
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193503>`__

pw_rust
-------
* `Tweak docs for Rust tokenized logging example
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195351>`__

pw_sensor
---------
:ref:`seed-0120` introduces ``pw_sensor``, a module that will handle
Pigweed's upcoming sensor framework.

* `Add configuration SEED
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183150>`__

pw_spi
------
* `Update Android.bp to conform with new style
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197410>`__

pw_stream_shmem_mcuxpresso
--------------------------
* `Fix interrupt pending check
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198070>`__
  (issue `#330225861 <https://issues.pigweed.dev/issues/330225861>`__)

pw_sync_stl
-----------
* `Android.bp: Add missing dependency on pw_sync_headers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197030>`__

pw_sys_io
----------
* `Add backend label flags
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196610>`__
  (issue `#329441699 <https://issues.pigweed.dev/issues/329441699>`__)

pw_sys_io_rp2040
----------------
* `Use callbacks to block on input
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/191490>`__
  (issue `#324633376 <https://issues.pigweed.dev/issues/324633376>`__)

pw_sys_io_stm32cube
-------------------
* `Fix build for f0xx, f1xx and f3xx families
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195871>`__

pw_thread_zephyr
-----------------
* `Apply formatting fixes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195951>`__

pw_tokenizer
------------
The Rust library's hashing code was updated to support multi-input hashing.

* `Refactor Rust hash code to allow multi-input hashing
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186310>`__

pw_transfer
-----------
The TypeScript client now has an ``abort()`` method for ending a transfer
without a completion chunk and a ``terminate()`` method for ending a transfer
with a completion chunk.

* `Inline TRANSFER_CLIENT_DEPS
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198592>`__
* `Support full java protos
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196231>`__
  (issue `#329445249 <https://issues.pigweed.dev/issues/329445249>`__)
* `Add abort() and terminate() apis
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194910>`__
* `Update the proxy to only consider transfer chunks
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196170>`__
* `Fix Java client timeouts in terminating state
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195123>`__

pw_unit_test
------------
The new :cpp:class:`pw::unit_test::TestRecordEventHandler` class is a
predefined event handler that outputs a test summary in Chromium JSON Test
Results format.

* `Flag to disable cmake pw_add_test calls
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197530>`__
  (issue `#330205620 <https://issues.pigweed.dev/issues/330205620>`__)
* `Add duplicate test case
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197170>`__
* `Localize the label flags
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196670>`__
  (issue `#329441699 <https://issues.pigweed.dev/issues/329441699>`__)
* `Add test record event handler
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194050>`__
* `Adding googletest_handler_adapter to cmake
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195070>`__

pw_watch
--------
Changes to Emboss files now trigger rebuilds.

* `Add emboss files to default watch patterns
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195387>`__

pw_web
------
* `NPM version bump to 0.0.16
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198291>`__
* `Update createLogViewer to use union types
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198210>`__
  (issue `#330564978 <https://issues.pigweed.dev/issues/330564978>`__)
* `Include file information in browser logs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196510>`__
  (issue `#329680229 <https://issues.pigweed.dev/issues/329680229>`__)
* `NPM version bump to 0.0.15
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196120>`__

Build
=====

Bazel
-----
The new :ref:`module-pw_build-bazel-pw_facade` Bazel macro makes it easier
to create a :ref:`facade <docs-facades>`.

* `Remove the deprecated pw_cc_facade macro
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196121>`__
  (issue `#328679085 <https://issues.pigweed.dev/issues/328679085>`__)
* `Treat Rust warnings as errors
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196436>`__
  (issue `#329685244 <https://issues.pigweed.dev/issues/https://pwbug.dev/329685244>`__)
* `Localize backend label flags
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196232>`__
  (issue `#329441699 <https://issues.pigweed.dev/issues/329441699>`__)
* `Use pw_facade
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195383>`__
  (issue `#328679085 <https://issues.pigweed.dev/issues/328679085>`__)
* `Introduce pw_facade
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193648>`__
  (issue `#328679085 <https://issues.pigweed.dev/issues/328679085>`__)
* `Fix bazel query
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195138>`__

Language support
================

Python
------
* `Use future annotations
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198570>`__
* `Use future annotations
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/198051>`__
* `Remove PathOrStr variables
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197538>`__
* `Switch from typing.Optional[...] to "... | None"
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197347>`__
  (issue `#248257406 <https://issues.pigweed.dev/issues/248257406>`__)
* `Switch from typing.Union to "|"
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197346>`__
  (issue `#248257406 <https://issues.pigweed.dev/issues/248257406>`__)
* `Use argparse.BooleanOptionalAction
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197345>`__
  (issue `#248257406 <https://issues.pigweed.dev/issues/248257406>`__)
* `Use pathlib.Path.is_relative_to()
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197344>`__
  (issue `#248257406 <https://issues.pigweed.dev/issues/248257406>`__)
* `Switch from typing.Dict to dict
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197343>`__
  (issue `#248257406 <https://issues.pigweed.dev/issues/248257406>`__)
* `Switch from typing.Tuple to tuple
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197342>`__
  (issue `#248257406 <https://issues.pigweed.dev/issues/248257406>`__)
* `Switch from typing.List to list
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197341>`__
  (issue `#248257406 <https://issues.pigweed.dev/issues/248257406>`__)

OS support
==========

Zephyr
------
* `Add action for installing Zephyr SDK
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194629>`__

Docs
====
The new :ref:`CLI style guide <docs-pw-style-cli>` outlines how CLI utilities
in upstream Pigweed should behave.

.. todo-check: disable

* `Add CLI style guide
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197010>`__
  (issue `#329532962 <https://issues.pigweed.dev/issues/329532962>`__)
* `Move TODO style guide
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197730>`__
* `Fix redirect paths
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197368>`__
  (issue `#324241028 <https://issues.pigweed.dev/issues/324241028>`__)
* `Fix code-block indentation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197533>`__
* `Add redirects infrastructure and docs contributor section
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197339>`__
  (issue `#324241028 <https://issues.pigweed.dev/issues/324241028>`__)
* `Fix some incorrect target names
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/196495>`__
  (issue `#329441699 <https://issues.pigweed.dev/issues/329441699>`__)
* `Fix mentions of sample_project
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195974>`__
  (issue `#322859039 <https://issues.pigweed.dev/issues/322859039>`__)
* `TOC entry for API documentation from source
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195970>`__
* `Update changelog
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195354>`__

.. todo-check: enable

Third party
===========
* `Minor build file formatting fixes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197369>`__
* (Emboss) `Assume newer emboss version 1/2
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/197362>`__
  (issue `#329872338 <https://issues.pigweed.dev/issues/329872338>`__)
* (STM32Cube) `Fix bazel hal driver build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195870>`__

-----------
Mar 7, 2024
-----------
Highlights (Feb 22, 2024 to Mar 7, 2024):

* The new :ref:`module-pw_digital_io_linux` module is a
  :ref:`module-pw_digital_io` backend for Linux userspace.
* :cpp:class:`pw::multibuf::MultiBufAllocator` class is a new interface
  for allocating ``pw::multibuf::MultiBuf`` objects.
* The ``pw_web`` log viewer now captures browser console logs. It also
  now supports creating log stores and downloading logs from stores.

Active SEEDs
============
Help shape the future of Pigweed! Please visit :ref:`seed-0000`
and leave feedback on the RFCs (i.e. SEEDs) marked
``Open for Comments``.

Modules
=======

pw_allocator
------------
* `Remove split_free_list_allocator.cc from Android.bp
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194551>`__
* `Add missing dep
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194231>`__
* `Use BlockAllocator instead of alternatives
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188354>`__
* `Make TrackingAllocator correct by construction
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193330>`__

pw_assert_log
-------------
* `Depend on pw_log_headers in Android.bp
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194434>`__
* `Fix PW_HANDLE_CRASH to handle 0 args
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194450>`__
  (issue `#327201811 <https://issues.pigweed.dev/issues/327201811>`__)

pw_async2
---------
* `Address post-submit comments
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194693>`__
* `Add converting constructors to Poll
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193593>`__

pw_blob_store
-------------
* `Set module name to BLOB
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195170>`__

pw_bluetooth
------------
* `Add hci_data.emb
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194631>`__
  (issue `#311639690 <https://issues.pigweed.dev/issues/311639690>`__)
* `Add ISO feature bit to controllers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194550>`__
* `Add ISO definitions to Controller
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194470>`__

pw_build
--------
* `Remove FUZZTEST_OPTS
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189317>`__
* `Fix ProjectBuilder recipe percentage
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194392>`__

pw_config_loader
----------------
* `Support custom overloading rules
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190833>`__

pw_containers
-------------
* `Rename VariableLengthEntryQueue
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187311>`__
* `Rename VariableLengthEntryQueue files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187310>`__
* `VariableLengthEntryQueue C++ API
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169910>`__

pw_digital_io_linux
-------------------
The new :ref:`module-pw_digital_io_linux` module is a
:ref:`module-pw_digital_io` backend for Linux userspace.

* `Introduce new module
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194430>`__

pw_json
-------
* `Update example; fix typo and declaration order
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194411>`__

pw_libc
-------
* `Add stdfix target
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194628>`__
  (issue `#323425639 <https://issues.pigweed.dev/issues/323425639>`__)
* `Facilitate next llvm-libc roll
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194603>`__

pw_log
------
* `Fix stdout race in println_backend_test_test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195122>`__
  (issue `#328498798 <https://issues.pigweed.dev/issues/328498798>`__)
* `Add tests for Rust printf and println backends
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194952>`__
* `Show child docs in site nav
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193830>`__
* `Fix the Pigweed Soong build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193690>`__
  (issue `#277108894 <https://issues.pigweed.dev/issues/277108894>`__)

pw_module
---------
* `Remove README.md check
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194934>`__
  (issue `#328265397 <https://issues.pigweed.dev/issues/328265397>`__)

pw_multibuf
-----------
The new :cpp:class:`pw::multibuf::MultiBufAllocator` class is an interface
for allocating ``pw::multibuf::MultiBuf`` objects.

* `Add MultiBufAllocator interface
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180840>`__
* `Deduplicate const+non_const iterators
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194405>`__
* `Clean up API
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194600>`__
  (issue `#327673957 <https://issues.pigweed.dev/issues/327673957>`__)
* `Add +=N and +n operators to iterator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194310>`__
* `Add slicing operations to MultiBuf
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192895>`__

pw_presubmit
------------
* `Remove unused presubmit step
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194940>`__

pw_proto_compiler
-----------------
* `strip_import_prefix + options
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194949>`__
  (issue `#328311416 <https://issues.pigweed.dev/issues/328311416>`__)

pw_result
---------
* `Add constructor deduction guide
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194612>`__

pw_rpc
------
The newly public :cpp:class:`pw::rpc::CloseAndWaitForCallbacks` function
abandons an RPC and blocks on the completion of any running callbacks.

* `Expose CloseAndWaitForCallbacks in client call API
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194697>`__

pw_rpc_transport
----------------
* `Add log for no packet available
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194592>`__

pw_rust
-------
* `Add Rust tokenized logging example
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181268>`__

pw_stream_uart_mcuxpresso
-------------------------
``pw_stream_uart_mcuxpresso`` now supports direct memory access reads and
writes.

* `Fix code examples in docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194630>`__
* `Implement DoRead DMA
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192893>`__
  (issue `#325514698 <https://issues.pigweed.dev/issues/325514698>`__)
* `Implement DoWrite DMA
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192892>`__
  (issue `#325514698 <https://issues.pigweed.dev/issues/325514698>`__)
* `Implement init / deinit
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192891>`__
  (issue `#325514698 <https://issues.pigweed.dev/issues/325514698>`__)
* `USART DMA scaffolding
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192890>`__
  (issue `#325514698 <https://issues.pigweed.dev/issues/325514698>`__)

pw_string
---------
``pw::InlineByteString`` is a new alias of ``pw::InlineBasicString<std::byte>``
that can be used as a simple and efficient byte container.

* `Add ToString for Result and Poll
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194621>`__
* `Support InlineBasicString<std::byte>; InlineByteString alias
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194710>`__
  (issue `#327497061 <https://issues.pigweed.dev/issues/327497061>`__)

pw_sync
-------
* `Allow implict conversion when moving BorrowedPointer
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194394>`__

pw_tokenizer
------------
* `Fix missing bazel filegroup
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194399>`__
* `Add Detokenizer constructor with elf binary section
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190650>`__
* `Add code size optimization to Rust implementation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193504>`__

pw_toolchain_bazel
------------------
* `Apply more common attrs to cc_toolchain targets
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194890>`__
* `Support Windows in toolchain template build files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194591>`__

pw_transfer
-----------
* `Respect user-specified resource size
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194935>`__
* `Temporarily disable broken integration test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194850>`__
* `Only request a single chunk in test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194151>`__
  (issue `#323386167 <https://issues.pigweed.dev/issues/323386167>`__)
* `Fix WindowPacketDropper proxy filter
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194030>`__
  (issue `#322497823 <https://issues.pigweed.dev/issues/322497823>`__)

pw_unit_test
------------
The :ref:`module-pw_unit_test` docs have been revamped. Using the full upstream
GoogleTest backend with ``simple_printing_main`` in Bazel has been fixed.

* `Update docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193671>`__
* `Fix googletest backend
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190593>`__
  (issue `#324116813 <https://issues.pigweed.dev/issues/324116813>`__)

pw_watch
--------
``pw_watch`` now rebuilds when Bazel files are changed.

* `Add bazel files to default watch patterns
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/195310>`__
  (issue `#328619290 <https://issues.pigweed.dev/issues/328619290>`__)

pw_web
------
The ``pw_web`` log viewer now captures browser console logs. It also
now supports creating log stores and downloading logs from stores.

* `Capture browser logs in the log viewer
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194451>`__
  (issue `#325096768 <https://issues.pigweed.dev/issues/325096768>`__)
* `Create log store and enable download logs from it
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186874>`__
  (issue `#316966729 <https://issues.pigweed.dev/issues/316966729>`__)

Bazel
-----
* `Update rust_crates in Bazel WORKSPACE
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194938>`__
* `Mark more targets testonly
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193626>`__
  (issue `#324116813 <https://issues.pigweed.dev/issues/324116813>`__)

Docs
====
The new :ref:`protobuf style guide <docs-pw-style-protobuf>` describes how
protobufs should be styled throughout Pigweed.

* `Add protobuf style guide
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190723>`__
  (issue `#232867615 <https://issues.pigweed.dev/issues/232867615>`__)
* `Clarify that Pigweed doesn't support msan
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194877>`__
  (issue `#234876100 <https://issues.pigweed.dev/issues/234876100>`__)
* `Clarify rvalue docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194696>`__
* `Reorder tocdepth and title
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193970>`__
* `Prefer rvalue references
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193647>`__
* `Update changelog
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193620>`__

Third party
===========
* `Android.bp: Export fuchsia_sdk_lib_stdcompat headers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194370>`__
* (Emboss) `Support latest version of Emboss in GN
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194876>`__
* (Emboss) `Add CMake support for emboss
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194400>`__
  (issue `#326500136 <https://issues.pigweed.dev/issues/326500136>`__)
* (FreeRTOS) `Tidy up Bazel build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193633>`__
  (issue `#326625641 <https://issues.pigweed.dev/issues/326625641>`__)
* (FreeRTOS) `Create Bazel build template
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193645>`__
  (issue `#326625641 <https://issues.pigweed.dev/issues/326625641>`__)
* (STM32Cube) `Build template formatting fixes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194723>`__

Miscellaneous
=============
* `Unrecommendify
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/194852>`__
* `Clean up Python proto imports
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193641>`__
  (issue `#241456982 <https://issues.pigweed.dev/issues/241456982>`__)

------------
Feb 23, 2024
------------
Highlights (Feb 9, 2024 to Feb 23, 2024):

* The new :ref:`module-pw_json` module provides classes for serializing JSON.
* Raspberry Pi RP2040 support was expanded, including a new
  ``pw::digital_io::Rp2040Config`` struct enables you to configure polarity for
  RP2040 GPIO pins, and a new ``pw::spi::Rp2040Initiator`` class which is a
  Pico SDK userspace implementation of Pigweed's SPI ``Initiator`` class.
* The new ``pw::spi::DigitalOutChipSelector`` class sets the state of a
  :ref:`module-pw_digital_io` output when activated.
* The :ref:`module-pw_kvs` docs were overhauled.

Active SEEDs
============
Help shape the future of Pigweed! Please visit :ref:`seed-0000`
and leave feedback on the RFCs (i.e. SEEDs) marked
``Open for Comments``.

Modules
=======

pw_allocator
------------
* `Remove total_bytes metric
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193251>`__
* `Expose TrackingAllocator's initialization state
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192570>`__

pw_assert
---------
* `Apply formatting changes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193496>`__

pw_assert_log
-------------
* `Fix Soong rules
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190602>`__
  (issue `#324266698 <https://issues.pigweed.dev/issues/324266698>`__)
* `Fix missing lib in soong rule
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192259>`__

pw_bluetooth
------------
* `Add Emboss rules to BUILD.bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192513>`__
* `Update emboss imports to match Bazel rule
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192392>`__

pw_build
--------
* `Silence warnings from linker script test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/191850>`__

pw_bytes
--------
* `Add missing export in soong rule
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192257>`__

pw_chrono_rp2040
----------------
* `Minor tweaks to documentation and test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183730>`__
  (issue `#303297807 <https://issues.pigweed.dev/issues/303297807>`__)

pw_cli
------
* `Add exit codes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192897>`__

pw_config_loader
----------------
The new ``skip_files__without_sections`` option enables you to just move on
to the next file rather than raise an exception if a relevant section doesn't
exist in a config file.

* `Allow skipping files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/191970>`__

pw_containers
-------------
* `Add default move operator for FilteredView
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192830>`__
* `Add move constructor to FiltertedView
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/191832>`__

pw_digital_io_rp2040
--------------------
The new ``pw::digital_io::Rp2040Config`` struct enables you to configure
polarity for RP2040 GPIO pins.

* `Config with polarity
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176290>`__
  (issue `#303255049 <https://issues.pigweed.dev/issues/303255049>`__)

pw_env_setup
------------
* `Use amd64 host tools on arm64
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192763>`__
  (issue `#325498131 <https://issues.pigweed.dev/issues/325498131>`__)

pw_function
-----------
The new ``//third_party/fuchsia:fit`` label flag enables Bazel-based projects
to provide an alternate implementation for ``fit()`` when needed.

* `//third_party/fuchsia:fit label_flag
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192391>`__

pw_fuzzer
---------
* `Fix Bazel example, add presubmit test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/191310>`__
  (issue `#324617297 <https://issues.pigweed.dev/issues/324617297>`__)

pw_grpc
-------
* `Fix some minor bugs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/191831>`__

pw_hdlc
-------
* `Add android_library targets
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190930>`__
  (issue `#321155919 <https://issues.pigweed.dev/issues/321155919>`__)

pw_i2c
------
The API reference for :cpp:class:`pw::i2c::RegisterDevice` is now published on
``pigweed.dev``.

* `Doxygenify RegisterDevice
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/191833>`__

pw_ide
------
* `Fix environment inference
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/191371>`__

pw_json
-------
The new :ref:`module-pw_json` module provides classes for serializing JSON.

* `Classes for serializing JSON
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190458>`__

pw_kvs
------
The :ref:`module-pw_kvs` docs were overhauled.

* `Follow new module docs guidelines
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189430>`__

pw_log
------
* `Add android_library targets
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190930>`__
  (issue `#321155919 <https://issues.pigweed.dev/issues/321155919>`__)

pw_log_android
--------------
* `Fix missing libs in soong rule
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192258>`__

pw_metric
---------
* `Fix Bazel build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190971>`__
  (issue `#258078909 <https://issues.pigweed.dev/issues/258078909>`__)

pw_module
---------
* `Add exit codes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192897>`__

pw_multibuf
-----------
* `Fix soong support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192894>`__
  (issue `#325320103 <https://issues.pigweed.dev/issues/325320103>`__)

pw_package
----------
* `Update GoogleTest
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193250>`__
* `Remove capture_output=True
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192530>`__

pw_preprocessor
---------------
The :ref:`module-pw_preprocessor` reference is now being generated via Doxygen.

* `Do not check for __VA_OPT__ on older compilers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193434>`__
  (issue `#326135018 <https://issues.pigweed.dev/issues/326135018>`__)
* `Switch to Doxygen
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192730>`__
* `Use __VA_OPT__ when available
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187099>`__

pw_presubmit
------------
.. todo-check: disable

* `Allow markdown style TODOs and adjust rustdocs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192253>`__
  (issue `#315389119 <https://issues.pigweed.dev/issues/315389119>`__)
* `Log format --fix output
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192394>`__
* `Fix formatting of TypeScript code
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192393>`__
* `Disallow FIXME and recommend TODO
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188367>`__
* `Allow pwbug.dev in TODOs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/191795>`__
* `Allow Bazel issues in TODOs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190970>`__

.. todo-check: disable

.. _docs-changelog-20240226-pw_rpc:

pw_rpc
------
``pw_rpc`` clients will once again accept unsolicited responses from ``pw_rpc``
servers that were built prior to September 2022. Unsolicited responses, also
known as "open" requests, let a server send a message to a client prior to the
client sending a request. This change fixed an incompatibility in which
``pw_rpc`` clients built after September 2022 would not accept unsolicited
responses from servers built before September 2022 (specifically,
change `#109077 <https://pwrev.dev/109077>`_).

* `Remove use of deprecated Python API
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187491>`__
  (issue `#306195999 <https://issues.pigweed.dev/issues/306195999>`__)
* `Add android_library targets
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190930>`__
  (issue `#321155919 <https://issues.pigweed.dev/issues/321155919>`__)
* `Support legacy unsolicited responses
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192311>`__

pw_software_update
------------------
* `Add java build objects
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193410>`__

pw_spi
------
The new ``pw::spi::DigitalOutChipSelector`` class is an implementation of
``pw::spi::ChipSelector`` that sets the state of a :ref:`module-pw_digital_io`
output when activated.

* `Add Android.bp
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192590>`__
  (issue `#316067629 <https://issues.pigweed.dev/issues/316067629>`__)
* `DigitalOutChipSelector
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192790>`__
  (issue `#303255049 <https://issues.pigweed.dev/issues/303255049>`__)
* `Correct full-duplex behavior of linux_spi
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192591>`__
  (issue `#316067628 <https://issues.pigweed.dev/issues/316067628>`__)

pw_spi_rp2040
-------------
The new ``pw::spi::Rp2040Initiator`` class is a Pico SDK userspace
implementation of Pigweed's SPI ``Initiator`` class.

* `Initiator implementation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192791>`__
  (issue `#303255049 <https://issues.pigweed.dev/issues/303255049>`__)

pw_stream
---------
* `Fix Pigweed build after sync
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/191250>`__
  (issue `#277108894 <https://issues.pigweed.dev/issues/277108894>`__)

pw_string
---------
Debug error messages for assertions containing ``std::optional`` types have
been improved.

* `Add ToString for std::optional
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192514>`__

pw_sync
-------
* `Add missing lib in soong rule
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192260>`__

pw_tokenizer
------------
The :ref:`module-pw_tokenizer` and :ref:`module-pw_snapshot` Python
libraries can now be used from Bazel as a result of the proto
migration. See issue `#322850978 <https://issues.pigweed.dev/issues/322850978>`__).

* `Fix link breakage on linux
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192511>`__
  (issue `#321306079 <https://issues.pigweed.dev/issues/321306079>`__)
* `Proto migration stage 5/5
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/191270>`__
  (issue `#322850978 <https://issues.pigweed.dev/issues/322850978>`__)
* `Proto migration stage 3/5
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189696>`__
  (issue `#322850978 <https://issues.pigweed.dev/issues/322850978>`__)
* `Proto migration stage 1.5/5
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/191834>`__
  (issue `#322850978 <https://issues.pigweed.dev/issues/322850978>`__)
* `Proto migration stage 1/5
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/191135>`__
  (issue `#322850978 <https://issues.pigweed.dev/issues/322850978>`__)

pw_toolchain
------------
* `Add missing #include
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193430>`__
* `Rename `action_config_flag_sets` to `flag_sets`
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192911>`__
* `Simplify macOS -nostdlib++ usage
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192898>`__
* `Remove unnecessary toolchain arguments
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192896>`__
* `Add missing macOS cxx_builtin_include_directories
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192270>`__
  (issue `#324652164 <https://issues.pigweed.dev/issues/324652164>`__)

pw_toolchain_bazel
------------------
* `Use llvm-libtool-darwin on macOS
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190896>`__
  (issue `#297413805 <https://issues.pigweed.dev/issues/297413805>`__)
* `Explicitly depend on rules_cc
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/191430>`__

pw_transfer
-----------
* `Add an android_library for the Java client
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193534>`__
* `Change class to parser
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193550>`__
* `Fix integration test START packet issue
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192755>`__
  (issue `#322497491 <https://issues.pigweed.dev/issues/322497491>`__)
* `Add GetResourceStatus method
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192810>`__
* `Limit test to sending a single chunk
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192510>`__
* `Remove/hide deprecated handle interfaces
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190972>`__

pw_watch
--------
* `Add exit codes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192897>`__

pw_web
------
The log viewer now supports multiple log sources.

* `NPM version bump to 0.0.14
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193510>`__
* `Enable multiple log sources
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192370>`__
  (issue `#325096310 <https://issues.pigweed.dev/issues/325096310>`__)

Build
=====
* `Fix docs build on mac-arm64
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192910>`__
* `Use correct host_cpu when disabling docs on Arm Macs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192757>`__
* `Remove docs from default build on Arm Macs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192754>`__
  (issue `#315998985 <https://issues.pigweed.dev/issues/315998985>`__)
* `Build for Java 11
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/191830>`__

Bazel
-----
* `Remove shallow_since attributes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193473>`__
* `Upgrade protobuf to 4.24.4
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187431>`__
  (issue `#319717451 <https://issues.pigweed.dev/issues/319717451>`__)

Targets
=======
* (rp2040) `Replace rp2040 target with rp2040_pw_system
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192516>`__
* (rp2040) `Custom libusb backend
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192630>`__

OS support
==========
* (Zephyr) `Change the pinned Zephyr commit
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192690>`__
* (Zephyr) `Add zephyr's west CLI
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190547>`__

Docs
====
`Breadcrumbs <https://en.wikipedia.org/wiki/Breadcrumb_navigation>`_ are now
shown at the top of all docs pages except the homepage.

* `Add breadcrumbs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193508>`__
* `Fix incorrect module name in changelog
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193331>`__
* `Fix canonical URLs for all */docs.html pages
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/193431>`__
* `Fix typo in facades documentation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192710>`__
* `Fix 404s
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192251>`__
  (issue `#325086274 <https://issues.pigweed.dev/issues/325086274>`__)

SEEDs
=====
* (SEED-0125) `Claim SEED number
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192110>`__
* `Mark legacy Sensor SEED as Rejected
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192250>`__

Third party
===========
* `Roll FuzzTest and Abseil
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189419>`__

Miscellaneous
=============
* `Disable tests incompatible with rp2040
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192764>`__
  (issue `#260624583 <https://issues.pigweed.dev/issues/260624583>`__)
* `Fix uses of std::chrono literals
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192515>`__
* (pigweed.json) `Disallow Rosetta
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188251>`__
  (issue `#315998985 <https://issues.pigweed.dev/issues/315998985>`__)
* (renode) `Update renode to latest daily build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/192254>`__

-----------
Feb 9, 2024
-----------
Highlights (Jan 26, 2024 to Feb 9, 2024):

* The new :ref:`module-pw_grpc` module provides classes that map between
  ``pw_rpc`` packets and HTTP/2 gRPC frames, allowing ``pw_rpc`` services to
  be exposed as gRPC services.
* A lot of the remaining ``pw_toolchain_bazel`` feature work from
  :ref:`seed-0113` was finished and rough edges were polished up.
* The new generic ``pw::allocator::BlockAllocator`` interface supported several
  derived types that enable fine-grained control over how a block satisfies an
  allocation request.

* ``pw_transfer`` now supports :ref:`resumable transfers
  <pw_transfer-nonzero-transfers>`.

Active SEEDs
============
Help shape the future of Pigweed! Please visit :ref:`seed-0000`
and leave feedback on the RFCs (i.e. SEEDs) marked
``Open for Comments``.

Modules
=======

pw_alignment
------------
* `Add CMake & Soong support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189360>`__

pw_allocator
------------
The new generic :cpp:class:`pw::allocator::BlockAllocator` interface supports
several derived types that enable fine-grained control over how a block
satisfies an allocation request. The new :cpp:func:`pw::allocator::GetLayout()`
method retrieves the layout that was used to allocate a given pointer. The new
:cpp:class:`pw::allocator::AllocatorSyncProxy` interface synchronizes access to
another allocator, allowing it to be used by multiple threads.

* `Refactor metric collection for tests
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190105>`__
* `Add Allocator::GetLayout
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187764>`__
* `Add BlockAllocator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187657>`__
* `Fix SynchronizedAllocator typo
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190721>`__
* `Streamline Block and improve testing
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187656>`__
* `Separate metrics from Fallback-, MultiplexAllocator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190253>`__
* `Make TrackingAllocator::Init optional
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190454>`__
* `Check for integer overflow in Layout
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187654>`__
* `Add additional metrics
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190250>`__
* `Rename AllocatorMetricProxy to TrackingAllocator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190230>`__
* `Add SynchronizedAllocator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189690>`__
* `Fix typo
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189717>`__
* `Fix Allocator::Reallocate
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189691>`__

pw_assert
---------
* `Break out compatibility backend target
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189106>`__
  (issue `#322057191 <https://issues.pigweed.dev/issues/322057191>`__)

pw_async_basic
--------------
* `Remove unneeded locks from test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190238>`__
* `Test flake fix
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189938>`__
  (issue `#323251704 <https://issues.pigweed.dev/issues/323251704>`__)
* `Fix data race in newly added test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189879>`__
* `Fix ordering of tasks posted at same time
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189784>`__

pw_bloat
--------
The new boolean argument ``ignore_unused_labels`` for ``pw_size_report()``
enables you to remove labels from the JSON size report that have a size of 0.

* `Allow removal of zero sized labels
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190599>`__
  (issue `#282057969 <https://issues.pigweed.dev/issues/282057969>`__)

pw_bluetooth
------------
* `Add l2cap_frames.emb
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185751>`__
* `Fix typo in SbcAllocationMethod
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190130>`__

pw_boot
-------
* `Update status and general doc cleanup
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189298>`__

pw_build
--------
The new ``PW_USE_COLOR``, ``NO_COLOR``, and ``CLICOLOR_FORCE`` OS environment
variables enable you to control whether output in CI/CQ is color formatted.

* `Add Fuchsia to TARGET_COMPATIBLE_WITH_HOST_SELECT
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190714>`__
* `Enable fixed point types for clang builds
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190097>`__
  (issue `#323425639 <https://issues.pigweed.dev/issues/323425639>`__)
* `Support disabling colors
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189611>`__
  (issue `#323056074 <https://issues.pigweed.dev/issues/323056074>`__)

pw_bytes
--------
* `Check for integer overflow
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187655>`__

pw_channel
----------
The initial ``pw::channel::Channel`` class from :ref:`seed-0114` has been
introduced but it is experimental and should not be used yet.

* `Docs fix
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190624>`__
* `Module for async data exchange with minimal copying
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189284>`__

pw_cli
------
* `Add json_config_loader_mixin
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190540>`__

pw_compilation_testing
----------------------
* `Do not expand regexes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/191030>`__
* `Minor improvements
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189788>`__

pw_config_loader
----------------
The code from ``pw_cli`` related to looking up user-specific configuration
files has been moved to this separate module.

* `Add support for nested keys
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190673>`__
* `Add tests
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190672>`__
* `Initial commit
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190671>`__

pw_cpu_exception_cortex_m
-------------------------
* `Handle ARM v8.1 case
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189828>`__
  (issue `#311766664 <https://issues.pigweed.dev/issues/311766664>`__)

pw_digital_io
-------------
* `[[nodiscard]] on as() conversion functions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189815>`__
* `Use pw::internal::SiblingCast
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189015>`__

pw_env_setup
------------
* `Roll cipd
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190737>`__
  (issue `#315378787 <https://issues.pigweed.dev/issues/315378787>`__)
* `Make npm actions more robust
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189358>`__
  (issues `#323378974 <https://issues.pigweed.dev/issues/305042957>`__,
  `#322437881 <https://issues.pigweed.dev/issues/322437881>`__,
  `#323378974 <https://issues.pigweed.dev/issues/323378974>`__)
* `Only add mingw to PATH once
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190030>`__
  (issue `#322437881 <https://issues.pigweed.dev/issues/322437881>`__)

pw_format
---------
* `Fix safe buildifier warnings
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189716>`__
  (issue `#242181811 <https://issues.pigweed.dev/issues/242181811>`__)

pw_function
-----------
* `Follow new docs guidelines
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188362>`__

pw_fuzzer
---------
* `Fix Bazel run target instructions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188363>`__

pw_grpc
-------
The new :ref:`module-pw_grpc` module is an implementation of the gRPC HTTP/2
protocol. It provides classes that map between :ref:`module-pw_rpc` packets
and ``pw_grpc`` HTTP/2 frames, allowing ``pw_rpc`` services to be exposed as
gRPC services.

* `Fix off-by-one error when handling DATA frames
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190470>`__
  (issue `#323924487 <https://issues.pigweed.dev/issues/323924487>`__)
* `Add new module
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186796>`__

pw_ide
------
* `Disable Python terminal activation in VSC
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190592>`__
* `Remove terminal env vars from VSC settings
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188470>`__
* `VSC extension 0.1.4 release
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188742>`__

pw_presubmit
------------
Color formatting in CI/CQ has been improved for readability.

* `Simplify 'gn gen' color logic
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190239>`__
* `Use color logic in gn gen call
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189827>`__
* `Apply color logic in more cases
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189829>`__
* `Support disabling colors
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189611>`__
  (issue `#323056074 <https://issues.pigweed.dev/issues/323056074>`__)

pw_protobuf
-----------
* `Use pw::internal::SiblingCast
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189015>`__

pw_random
---------
* `Clean up build files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189779>`__

pw_result
---------
* `Add missing libs in Soong blueprint
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190950>`__

pw_rpc
------
* `Avoid undefined behavior when casting to rpc::Writer
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189648>`__

pw_stream
---------
* `Use pw::internal::SiblingCast
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189015>`__

pw_target_runner
----------------
* `Remove .dev from path name
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190450>`__

pw_thread
---------
* `Incease the sleep duration in tests
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190270>`__
  (issue `#321832803 <https://issues.pigweed.dev/issues/321832803>`__)

pw_tokenizer
------------
:ref:`Troubleshooting docs <module-pw_tokenizer-gcc-template-bug>` were added
that explain how to workaround GCC's template function tokenization bug in GCC
releases prior to 14.

* `Mention GCC template bug in the docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189820>`__

pw_toolchain
------------
* `Disable unstable features in rust toolchains
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189948>`__
* `Fix divergent configuration in arm_clang M0+ toolchain
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190196>`__
* `Use less generic names for B1-B5
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189732>`__
* `Use LLVM compiler-rt builtins
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186232>`__
* `Internal wrapper for casting between siblings
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189014>`__
  (issue `#319144706 <https://issues.pigweed.dev/issues/319144706>`__)
* `Small docs update
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189278>`__
  (issue `#300471936 <https://issues.pigweed.dev/issues/300471936>`__)

pw_toolchain_bazel
------------------
A lot of the remaining ``pw_toolchain_bazel`` feature work from
:ref:`seed-0113` was finished and rough edges were polished up.

* `Remove support for *_files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190810>`__
  (issue `#323448214 <https://issues.pigweed.dev/issues/323448214>`__)
* `Add support for setting environment variables
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190154>`__
  (issue `#322872628 <https://issues.pigweed.dev/issues/322872628>`__)
* `Implement per-action files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190152>`__
  (issue `#322872628 <https://issues.pigweed.dev/issues/322872628>`__)
* `Migrate to PwToolInfo
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190150>`__
  (issue `#322872628 <https://issues.pigweed.dev/issues/322872628>`__)
* `Pull file collection into config rule
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189971>`__
  (issue `#322872628 <https://issues.pigweed.dev/issues/322872628>`__)
* `Implement pw_cc_provides
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189112>`__
  (issue `#320177248 <https://issues.pigweed.dev/issues/320177248>`__)
* `Implement PwActionConfigInfo
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189990>`__
  (issue `#322872628 <https://issues.pigweed.dev/issues/322872628>`__)
* `Create temporary variable
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189970>`__
  (issue `#322872628 <https://issues.pigweed.dev/issues/322872628>`__)
* `Add a concept of well-known features
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189050>`__
  (issue `#320177248 <https://issues.pigweed.dev/issues/320177248>`__)
* `Implement requires_any_of for flag sets
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189111>`__
  (issue `#320177248 <https://issues.pigweed.dev/issues/320177248>`__)
* `Migrate to custom PwFeatureInfo
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189030>`__
  (issue `#320177248 <https://issues.pigweed.dev/issues/320177248>`__)
* `Add custom PwFlagSetInfo
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188990>`__
  (issue `#322872628 <https://issues.pigweed.dev/issues/322872628>`__)
* `Replace bazel_tools providers with PW providers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189110>`__
  (issue `#322872628 <https://issues.pigweed.dev/issues/322872628>`__)
* `Support regular binaries as tools
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190151>`__
  (issue `#322872628 <https://issues.pigweed.dev/issues/322872628>`__)

pw_transfer
-----------
``pw_transfer`` now supports :ref:`resumable transfers
<pw_transfer-nonzero-transfers>`.

* `Account for remaining_bytes in payload buffer
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190572>`__
* `Rename TransferHandle -> Handle
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189097>`__
* `Add resumeable transfers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182830>`__
* `Make cancellation a method on handles
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189096>`__
* `Allow setting a transfer resource size in C++
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189095>`__
  (issue `#319731837 <https://issues.pigweed.dev/issues/319731837>`__)

pw_unit_test
------------
* `Remove obsolete label flag
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190557>`__

Build
=====
* (Bazel) `Update clang version
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190256>`__

OS support
==========
* (Zephyr) `Fix default logging in chromium CQ
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190696>`__

Docs
====
* `Nest backends under respective facades in sitenav
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190245>`__
* `Add doxygengroup to the style guide
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190625>`__
* `Update homepage
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190018>`__
* `Update README links
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189731>`__
* `Fix module homepage canonical URLs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189773>`__
  (issue `#323077749 <https://issues.pigweed.dev/issues/323077749>`__)
* `Update Bazel quickstart output sample
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189412>`__
  (issue `#300471936 <https://issues.pigweed.dev/issues/300471936>`__)

SEEDs
=====
* (SEED-0122) `Update status, add bug reference
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189612>`__
* `Fix pw_seed_index template deps
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190675>`__
* `Add authors to SEED document headers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189870>`__
* (SEED-0119) `Add sensors SEED
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182653>`__

Miscellaneous
=============
* `Update the bootstrap script to have start/end guards
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190451>`__
* `Migrate bug numbers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189353>`__
  (issue `#298074672 <https://issues.pigweed.dev/issues/298074672>`__)
* `Upgrade mbedtls to 3.5.0
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189273>`__
  (issue `#319289775 <https://issues.pigweed.dev/issues/319289775>`__)
* `Fix clang-format findings
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/190090>`__

------------
Jan 26, 2024
------------
Highlights (Jan 12, 2024 to Jan 26, 2024):

* The new :ref:`docs-bazel-integration` guide shows you how to integrate a
  single Pigweed module into an existing Bazel project.
* Initial support for :py:class:`pw_cc_feature` has been added, which completes
  the initial set of rules required for building toolchains with
  :ref:`module-pw_toolchain_bazel`.
* A longstanding GCC bug that caused tokenized logging within a function
  template to not work has been fixed.

Active SEEDs
============
Help shape the future of Pigweed! Please visit :ref:`seed-0000`
and leave feedback on the RFCs (i.e. SEEDs) marked
``Open for Comments``.

Modules
=======

pw_allocator
------------
* `Add SplifFreeListAllocator fuzzer
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178216>`__

pw_bluetooth
------------
* `Add advertising packet content filter emboss definitions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188314>`__
* `Add android multiple advertising emboss structures
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188313>`__
* `Add a2dp remaining offload emboss structures
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188311>`__
* `Move emboss structures from hci_commands to hci_common
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188312>`__
* `Reorganize hci_vendor.emb
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188310>`__
* `Add new event definitions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188150>`__
  (issue `#311639432 <https://issues.pigweed.dev/issues/311639432>`__)

pw_build
--------
The new :ref:`module-pw_build-bazel-pw_cc_binary_with_map` Bazel rule enables
you to generate a ``.map`` file when building a binary.

* `Add pw_cc_binary variant to generate .map files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187710>`__
  (issue `#319746242 <https://issues.pigweed.dev/issues/319746242>`__)

pw_bytes
--------
The :cpp:class:`pw::ByteBuilder` API reference is now being auto-generated
via Doxygen. The new :cpp:func:`pw::bytes::SignExtend` template enables
expanding the nth bit to the left up to the size of the destination type.

* `Fix compilation error occured with Werror=all
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188890>`__
* `Update documentation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188462>`__
* `SignExtend template
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188456>`__
  (issue `#321114167 <https://issues.pigweed.dev/issues/321114167>`__)
* `Make _b literals error on values >255
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188208>`__

pw_compilation_testing
----------------------
* `Skip tests excluded by the preprocessor
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188650>`__
  (issue `#321088147 <https://issues.pigweed.dev/issues/321088147>`__)

pw_console
----------
* `Upgrade to ptpython 3.0.25
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188146>`__
  (issue `#320509105 <https://issues.pigweed.dev/issues/320509105>`__)

pw_containers
-------------
The destructors for ``pw::InlineQueue``, ``pw::InlineDeque``, and
``pw::Vector`` are now protected to prevent use with ``delete`` or
``std::unique_ptr`` and to prevent unusable declarations.

* `Protected InlineQueue/Deque<T> destructor
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187802>`__
* `Make Vector<T> destructor protected
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187801>`__

pw_digital_io
-------------
The private virtual API requirements for
:cpp:class:`pw::digital_io::DigitalIoOptional` are now documented because
they are needed when implementing concrete backends for ``pw_digital_io``.

* `Document the private virtual API requirements
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187669>`__
* `Remove conditional interrupt disabling requirements
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187668>`__

pw_doctor
---------
* `Update expected tools on POSIX
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188102>`__
  (issue `#315998985 <https://issues.pigweed.dev/issues/315998985>`__)

pw_env_setup
------------
* `Retrieve qemu on ARM Macs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187806>`__
  (issue `#315998985 <https://issues.pigweed.dev/issues/315998985>`__)

pw_hdlc
-------
* `Remove unused targets
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188226>`__
* `Remove unused rpc packet processor target
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188233>`__

pw_ide
------
* `Fix typo
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188423>`__
* `Launch activated terminals in VSC
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187195>`__
  (issue `#318583596 <https://issues.pigweed.dev/issues/318583596>`__)
* `VSC extension 0.1.3 release
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186726>`__

pw_kvs
------
The new ``pw::kvs::FlashMemory::EndOfWrittenData()`` method returns the first
byte of erased flash that has no more written bytes.

* `Add EndOfWrittenData()
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188872>`__

pw_persistent_ram
-----------------
* `Add more tests to PersistentBuffer
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188106>`__
  (issue `#320538351 <https://issues.pigweed.dev/issues/320538351>`__)

pw_polyfill
-----------
``pw_polyfill/static_assert.h`` now provides a C23-style ``static_assert()``.
See :ref:`module-pw_polyfill-static_assert`.

* `Remove _Static_assert workaround
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188277>`__
* `Provide static_assert polyfill for C
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188137>`__

pw_preprocessor
---------------
The new ``PW_ADD_OVERFLOW``, ``PW_SUB_OVERFLOW``, and ``PW_MUL_OVERFLOW``
macros can be used to :ref:`check for integer overflows
<module-pw_preprocessor-integer-overflow>`.

* `Add integer-overflow macros
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187653>`__

pw_presubmit
------------
* `Add more info to todo summary
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188750>`__
* `Trim paths in ninja summary
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188070>`__
* `No copyright for MODULE.bazel.lock
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188170>`__
* `Exclude docs on Mac ARM hosts
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187784>`__
  (issue `#315998985 <https://issues.pigweed.dev/issues/315998985>`__)

pw_protobuf
-----------
* `Fix another &*nullptr in test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188717>`__
* `Fix undefined pointer deref in fuzz test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188281>`__
* `Fix out-of-range read
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188095>`__
  (issue `#314803709 <https://issues.pigweed.dev/issues/314803709>`__)

pw_thread
---------
* `Add missing include
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/189212>`__

pw_tokenizer
------------
A longstanding GCC bug that caused tokenized logging within a function template
to not work has been fixed.

* `Compensate for GCC template bug
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188424>`__
  (issue `#321306079 <https://issues.pigweed.dev/issues/321306079>`__)
* `Allow use of static_assert in C99 test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188357>`__
* `Adjust rustdocs deps to only be in std environments
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188138>`__

pw_tool
-------
This incomplete module has been deleted.

* `Delete module
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188919>`__

pw_toolchain_bazel
------------------
The ``//pw_toolchain_bazel`` directory is now configured to be compiled as a
standalone Bazel module. Initial support for :py:class:`pw_cc_feature` has been
added, which completes the initial set of rules required for building
toolchains with :ref:`module-pw_toolchain_bazel`.

* `Remove deprecated action names
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188991>`__
  (issue `#320177248 <https://issues.pigweed.dev/issues/320177248>`__)
* `Migrate to type-safe action names
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187896>`__
  (issue `#320177248 <https://issues.pigweed.dev/issues/320177248>`__)
* `Require action labels in providers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188810>`__
  (issue `#320177248 <https://issues.pigweed.dev/issues/320177248>`__)
* `Define actions names as labels
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187895>`__
  (issue `#320177248 <https://issues.pigweed.dev/issues/320177248>`__)
* `Make the pw_toolchain repository into a bazel module
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187932>`__
  (issue `#320177248 <https://issues.pigweed.dev/issues/320177248>`__)
* `Add pw_cc_feature
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181755>`__
  (issue `#309533028 <https://issues.pigweed.dev/issues/309533028>`__)
* `Set exec_transition_for_inputs to False
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188361>`__
  (issue `#321268080 <https://issues.pigweed.dev/issues/321268080>`__)
* `Remove check_deps_provide
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187894>`__
  (issue `#320177248 <https://issues.pigweed.dev/issues/320177248>`__)

pw_transfer
-----------
The C++ client for :ref:`module-pw_transfer` now uses handles for
cancellation.

* `Remove duplicated Builder call
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188855>`__
* `Use handles for cancellation in C++ client
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/134290>`__
  (issue `#272840682 <https://issues.pigweed.dev/issues/272840682>`__)

pw_web
------
* `Init. improvements to resize performance
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188050>`__
  (issue `#320475138 <https://issues.pigweed.dev/issues/320475138>`__)

Build
=====
* `Use Python 3.11
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182191>`__
  (issue `#310293060 <https://issues.pigweed.dev/issues/310293060>`__)
* `Use pre-release of rules_python
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188852>`__
  (issue `#310293060 <https://issues.pigweed.dev/issues/310293060>`__)
* `Use rules_python in Bazel build files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188690>`__

Targets
=======

host_device_simulator
---------------------
* `Update docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187430>`__

Docs
====
* `How to use a single Pigweed module in Bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188922>`__
* `Add pre-reqs for non-Debian Linux distros
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188770>`__
  (issue `#320519800 <https://issues.pigweed.dev/issues/320519800>`__)
* `Auto-generate module source code and issues URLs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187312>`__
* `Minor updates to the FAQ
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188252>`__
* `Update changelog
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187614>`__

SEEDs
=====
* (SEED-0123) `Claim SEED number
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188140>`__
* (SEED-0124) `Claim SEED number
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188671>`__

Miscellaneous
=============
* `Remove module-level README.md files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188374>`__
* `Fix how we ignore bazel- directories
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/188940>`__

------------
Jan 12, 2024
------------
Highlights (Dec 29, 2023 to Jan 12, 2024):

* :ref:`docs-changelog-20240112-pw_allocator` added parameter to make it easier
  to work with allocation metadata in a block's header and made it easier to
  omit flag-related code for blocks.
* ``pw_cc_library`` has been replaced with the Bazel-native ``cc_library``.
* :ref:`docs-changelog-20240112-pw_thread_stl` disallowed
  ``std::thread::detach()`` on Windows because it's known to randomly hang
  indefinitely.

Active SEEDs
============
Help shape the future of Pigweed! Please visit :ref:`seed-0000`
and leave feedback on the RFCs (i.e. SEEDs) marked
``Open for Comments``.

Modules
=======

.. _docs-changelog-20240112-pw_allocator:

pw_allocator
------------
A new template parameter, ``kNumExtraBytes``, was added to ``Block``. This
parameter reserves space in the block's header for storing and fetching
allocation metadata.

The ``kMaxSize`` parameter was removed from ``Block``
and replaced by ``kNumFlags`` to make it easier to omit flag-related code when
not needed and define fewer block types rather than one for each allocation
pool size.

* `Fix source file list in soong rule
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187127>`__
* `Fix MulitplexAllocator's deps
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186873>`__
* `Add extra bytes to Block
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185954>`__
* `Add initializing constructors
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185953>`__
* `Add note to AllocatorMetricProxy tests
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185952>`__

pw_bluetooth_sapphire
---------------------
* `Advertise FCS Option
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186850>`__
  (issue `#318002648 <https://issues.pigweed.dev/issues/318002648>`__)
* `Refactor method and variable names
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182899>`__
  (issue `#312645622 <https://issues.pigweed.dev/issues/312645622>`__)
* `Revert commits to get to a known working state
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183087>`__

pw_containers
-------------
* `Remove DestructorHelper from vector
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185521>`__
  (issue `#301329862 <https://issues.pigweed.dev/issues/301329862>`__)

pw_doctor
---------
* `Fix overridden package check
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187110>`__

pw_emu
------
* `Remove psutil dependency
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186830>`__
  (issue `#316080297 <https://issues.pigweed.dev/issues/316080297>`__)

pw_env_setup
------------
* `Update ref to bazel version file
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187118>`__
* `Drop reference to rust version file
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187113>`__
* `Use correct arch for cipd in Bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184903>`__
  (issue `#234879770 <https://issues.pigweed.dev/issues/234879770>`__)
* `Switch to Fuchsia ninja CIPD package
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184078>`__
  (issue `#311711323 <https://issues.pigweed.dev/issues/311711323>`__)
* `Prevent NPM output from interrupting bootstrap
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186510>`__

pw_ide
------
The new ``pigweed.enforceExtensionRecommendations`` configuration option for
VS Code makes extension recommendation enforcement optional. When you set this
flag to ``true`` in your project's ``extension.json``, recommended extensions
will need to be installed and non-recommended extensions will need to be
disabled.

You can now submit Pigweed issues through VS Code. Open the Command Palette
and type ``Pigweed: File Bug``.

* `Add Jest testing to VSC extension
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187126>`__
* `Make VSC extension enforcement an option
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187170>`__
* `Support alternate \`clangd\` paths
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186730>`__
  (issue `#318413766 <https://issues.pigweed.dev/issues/318413766>`__)
* `VSC extension 0.1.2 release
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184907>`__
* `Fix CLI command docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184955>`__
* `Allow submitting bugs from VSC
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184908>`__
* `Document \`compdb_gen_cmd\`
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184899>`__
* `Improve VSC extension UX
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184990>`__

.. _docs-changelog-20240112-pw_log:

pw_log
------
The Python API of the log decoder now accepts a ``logging.Logger`` or
``logging.LoggerAdapter`` instance as the default device logger. Previously
it only accepted ``logging.Logger``.

* `Let Device accept logger adapter
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187119>`__

pw_log_zephyr
-------------
* `Add missing include for tokenized logging
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186810>`__

pw_multibuf
-----------
* `Add soong support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186815>`__

pw_protobuf
-----------
* `Fix bool overflow
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186801>`__
  (issue `#318732334 <https://issues.pigweed.dev/issues/318732334>`__)
* `Fix nullptr dereference in fuzz test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186800>`__
  (issue `#314829525 <https://issues.pigweed.dev/issues/314829525>`__)
* `Don't crash on invalid field number
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186724>`__
  (issue `#314803709 <https://issues.pigweed.dev/issues/314803709>`__)
* `Fix conflict in messages with fields named 'other'
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186951>`__
* `Properly fuzz nested encoders/decoders
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186723>`__
  (issue `#314803709 <https://issues.pigweed.dev/issues/314803709>`__)
* `Fully qualify message namespace
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186822>`__

pw_rpc
------
The new ``PW_RPC_METHOD_STORES_TYPE`` config option provides a way at runtime
to determine a method's type. This is useful when implementing a translation
layer from another RPC system. Most Pigweed projects won't need this feature so
it's disabled by default.

* `Have Method optionally store it's MethodType
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185773>`__
* `Run tests with completion callback config enabled
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187172>`__
* `Add some missing deps in Bazel targets
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186934>`__
  (issue `#318850523 <https://issues.pigweed.dev/issues/318850523>`__)

pw_stream
---------
* `Make stream adapters use reinterpret_cast
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186766>`__
  (issue `#314829006 <https://issues.pigweed.dev/issues/314829006>`__)

pw_system
---------
See :ref:`docs-changelog-20240112-pw_log`.

* `Let Device accept logger adapter
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187119>`__

.. _docs-changelog-20240112-pw_thread_stl:

pw_thread_stl
-------------
Using ``std::thread::detach()`` on Windows has been disallowed. When compiling
with MinGW on Windows, ``std::thread::detach()`` is known to randomly hang
indefinitely.

* `Don't allow std::thread::detach() on Windows
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186570>`__
  (issue `#317922402 <https://issues.pigweed.dev/issues/317922402>`__)

pw_tokenizer
------------
* `Fix undefined dereference in fuzz test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186821>`__
  (issue `#314829057 <https://issues.pigweed.dev/issues/314829057>`__)
* `Make Rust hashing function const
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186273>`__

pw_toolchain
------------
``pw_toolchain`` now adds ``-fno-ms-compatibility`` to ``cflags`` on Windows.
When building with ``clang`` on Windows targeting ``msvc``, ``clang``
previously enabled a ``ms-compatibility`` mode that broke Pigweed's macro
magic.

* `Remove duplicate config from Cortex-A32 toolchain
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187270>`__
* `Add -fno-ms-compatibility to cflags on Windows
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187173>`__
  (issue `#297542996 <https://issues.pigweed.dev/issues/297542996>`__)

pw_unit_test
------------
* `Fix sanitize(r) directive
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186650>`__

pw_web
------
* `Fix autoscroll when window is resized
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187193>`__
  (issue `#305022742 <https://issues.pigweed.dev/issues/305022742>`__)

Build
=====

Bazel
-----
All modules now support the injection of module configuration via
``label_flags``.

``pw_cc_library`` has been replaced with the Bazel-native ``cc_library``.

* `Update to Bazel 7.0.0 release version
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186935>`__
* `Add module configuration support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186725>`__
  (issue `#234872811 <https://issues.pigweed.dev/issues/234872811>`__)
* `Remove pw_cc_library
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186770>`__
  (issue `#267498492 <https://issues.pigweed.dev/issues/267498492>`__)
* `Remove references to pw_cc_library
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186765>`__
  (issue `#267498492 <https://issues.pigweed.dev/issues/267498492>`__)
* `Replace pw_cc_library with cc_library
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186763>`__
  (issue `#267498492 <https://issues.pigweed.dev/issues/267498492>`__)

OS support
==========

Zephyr
------
You can now enable the ``pw_thread/sleep.h`` library in your Zephyr project by
setting ``CONFIG_PIGWEED_THREAD_SLEEP=Y`` in your Kconfig.

* `Link pw_thread.sleep with appropriate kConfig
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186825>`__

Docs
====
* `Add example of conditionally enabling code based on module config
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187171>`__
* `Mention pw format tool
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186851>`__
* `Update changelog
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186470>`__
* `Rework first-time setup
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185730>`__
* `Add troubleshooting section to Bazel quickstart
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186237>`__

SEEDs
=====
* (SEED-0001) `Fix typo and formatting
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187273>`__
* (SEED-0114) `Fix Compiler Explorer link
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187330>`__
* (SEED-0122) `Claim SEED number
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/187120>`__

------------
Dec 29, 2023
------------
Highlights (Dec 15, 2023 to Dec 29, 2023):

* The new ``pw::allocator::MultiplexAllocator`` abstract class makes it easier
  to dispatch between allocators depending on an app-specific request type ID.
* ``pw_base64`` now has initial Rust support.
* ``pw_malloc_freertos``, a new FreeRTOS backend for :ref:`module-pw_malloc`,
  was added.
* The new ``pw::digital_io::Polarity`` helper enum makes it easier for backends
  to map logical levels to physical levels.
* The Rust macro ``tokenize_to_buffer!`` in the ``pw_tokenizer`` Rust crate now
  supports concatenation of format strings via the ``PW_FMT_CONCAT`` operator.
* The :ref:`module-pw_hdlc` and :ref:`module-pw_result` docs were updated to
  follow our latest :ref:`docs-contrib-docs-modules`.

Active SEEDs
============
Help shape the future of Pigweed! Please visit :ref:`seed-0000`
and leave feedback on the RFCs (i.e. SEEDs) marked
``Open for Comments``.

Modules
=======

pw_allocator
------------
The new ``pw::allocator::MultiplexAllocator`` abstract class makes it easier to
dispatch between allocators depending on an app-specific request type ID.

* `Add MultiplexAllocator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185027>`__
* `Add WithMetrics interface
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185296>`__
* `Split up test utilities
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185295>`__

pw_base64
---------
``pw_base64`` now has initial Rust support. See `Crate pw_base64
</rustdoc/pw_base64/>`_.

* `Add initial Rust support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185210>`__

pw_containers
-------------
* `Remove DestructorHelper from queues
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185030>`__
  (issue `#301329862 <https://issues.pigweed.dev/issues/301329862>`__)

pw_digital_io
-------------
`pw::digital_io::Polarity <https://cs.opensource.google/pigweed/pigweed/+/main:pw_digital_io/public/pw_digital_io/polarity.h>`_
was added to make it easier for backends to map logical levels to physical levels.

* `Add helper Polarity enum
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185435>`__

pw_emu
------
* `Use code-block instead of code
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186170>`__

pw_hdlc
-------
* `Follow new module docs guidelines
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184799>`__

pw_malloc
---------
``pw_malloc_freertos``, a FreeRTOS backend for ``pw_malloc`` was added.

* `Require specifying a backend in bazel build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185441>`__
* `Add freertos backend
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185452>`__

pw_presubmit
------------
* `Use local prettier + eslint versions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185144>`__

pw_result
---------
* `Rework docs to new standard
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185147>`__

pw_rpc
------
* `Adjust alarm_timer_test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186274>`__
  (issue `#317990451 <https://issues.pigweed.dev/issues/317990451>`__)

pw_snapshot
-----------
* `Clean up RISCV CpuArchitecture
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185690>`__
* `Add RISCV CpuArchitecture to metadata
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185630>`__

pw_thread_stl
-------------
* `Disable Bazel tests failing on Windows
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186242>`__
  (issue `#317922402 <https://issues.pigweed.dev/issues/317922402>`__)
* `Disable tests failing on Windows
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186253>`__
  (issue `#317922402 <https://issues.pigweed.dev/issues/317922402>`__)

pw_tokenizer
------------
The Rust macro ``tokenize_to_buffer!`` now supports concatenation of format
strings via the ``PW_FMT_CONCAT`` operator.

* `Support \`PW_FMT_CONCAT\` in \`tokenize_to_buffer!\`
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185211>`__

pw_toolchain
------------
The need to provide an ``unknown`` value for various ``pw_cc_toolchain`` fields
has been removed.

* `Remove "unknown" from various fields
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185170>`__
  (issue `#315206506 <https://issues.pigweed.dev/issues/315206506>`__)

pw_transfer
-----------
* `Improve Python stream reopening and closing
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184931>`__

pw_unit_test
------------
* `Silence ASAN error in TestInfo
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185654>`__

pw_watch
--------
* `Build directory exclude list handling
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185772>`__
  (issue `#317241320 <https://issues.pigweed.dev/issues/317241320>`__)

pw_web
------
* `Add state UI unit tests
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184909>`__
  (issue `#316218222 <https://issues.pigweed.dev/issues/316218222>`__)

SEEDs
=====
* `Add facilitators to metadata and generated table
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185932>`__
* (SEED-0105) `Use code-block instead of code
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/186171>`__

------------
Dec 15, 2023
------------
Highlights (Dec 1, 2023 to Dec 15, 2023):

* We started implementing our new async API, :ref:`module-pw_async2`.
* We deprecated the use of ``gtest/gtest.h`` header paths for tests intended
  to build against ``pw_unit_test``. See
  :ref:`docs-changelog-20231215-pw_unit_test` for details.

  .. note::

     All the commits titled ``Use unit test framework`` in the ``Dec 15, 2023``
     update are related to this change.

* The :ref:`module-pw_alignment` and :ref:`module-pw_emu` docs have been updated
  to follow our latest :ref:`docs-contrib-docs-modules`.
* ``pw_system`` now supports an :ref:`extra logging channel
  <module-pw_system-logchannel>`.
* ``pw_toolchain_bazel`` has a new :ref:`get started guide
  <module-pw_toolchain_bazel-get-started>`.

Active SEEDs
============
Help shape the future of Pigweed! Please visit :ref:`seed-0000`
and leave feedback on the RFCs (i.e. SEEDs) marked
``Open for Comments``.

Modules
=======

pw_alignment
------------
* `Follow the new docs guidelines
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181432>`__

pw_allocator
------------
* `Fix metric disabling
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185026>`__
* `Add support for fuzzing Allocator implementations
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178215>`__
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183270>`__

pw_analog
---------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183270>`__

pw_async2
---------
We started implementing our new async API, :ref:`module-pw_async2`.

* `Implement initial async API
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182727>`__

pw_base64
---------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183299>`__

pw_blob_store
-------------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183299>`__

pw_bluetooth
------------
* `Add LE Read Buffer Size [v2] command complete event
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185070>`__
  (issue `#311639040 <https://issues.pigweed.dev/issues/311639040>`__)
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184312>`__
* `Fix LEExtendedCreateConnectionCommandV1 data interleaving
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183950>`__
  (issue `#305976440 <https://issues.pigweed.dev/issues/305976440>`__)
* `Fix ExtendedCreateConnectionV1
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183930>`__
  (issue `#305976440 <https://issues.pigweed.dev/issues/305976440>`__)
* `Add new HCI definition
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183771>`__
  (issue `#311639690 <https://issues.pigweed.dev/issues/311639690>`__)
* `Re-format emboss files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183770>`__

pw_bluetooth_sapphire
---------------------
* `Read ISO data buffer info on startup
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184052>`__
  (issue `#311639040 <https://issues.pigweed.dev/issues/311639040>`__)
* `LE Read Buffer Size [v2]
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183772>`__
  (issue `#311639040 <https://issues.pigweed.dev/issues/311639040>`__)
* `Add LE Set Host Feature
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184050>`__
  (issue `#311639040 <https://issues.pigweed.dev/issues/311639040>`__)
* `Update name of ISO host support bit
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184051>`__
  (issue `#311639040 <https://issues.pigweed.dev/issues/311639040>`__)
* `Fix BrEdrConnectionManager.Inspect test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183304>`__
  (issue `#42086629 <https://issues.fuchsia.dev/issues/42086629>`__)

pw_build
--------
* `Fix use of TARGET_COMPATIBLE_WITH_HOST_SELECT in external repo
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184095>`__

pw_bytes
--------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183354>`__

pw_checksum
-----------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183300>`__

pw_chrono
---------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183300>`__

pw_cli
------
You can now disable the printing of the banner by setting ``PW_ENVSETUP_QUIET``
or ``PW_ENVSETUP_NO_BANNER``.

* `Allow banner to be suppressed
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184970>`__

pw_console
----------
* `Disable private attr auto-completion
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184644>`__

pw_containers
-------------
* `Fix missing include
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184961>`__
* `Fix IntrusiveList::Item move assignment test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182951>`__
  (issue `#303634979 <https://issues.pigweed.dev/issues/303634979>`__)
* `Add missing dep for IntrusiveList test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184245>`__

pw_digital_io
-------------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183301>`__

pw_docgen
---------
We updated the docs server to use native hot reloading.

* `Update Pigweed Live schedule
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184960>`__
* `Add new docs server
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181011>`__

pw_emu
------
We're handling QEMU startup errors more gracefully now.

* `Better handling for startup errors
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184859>`__
  (issue `#315868463 <https://issues.pigweed.dev/issues/315868463>`__)
* `qemu: Improve the QMP handshake handling
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184858>`__
  (issue `#315516286 <https://issues.pigweed.dev/issues/315516286>`__)
* `Link all the docs to each other
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184310>`__
* `Update docs to follow new guidelines
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183223>`__

pw_env_setup
------------
The upstream Pigweed bootstrap script now runs ``npm install``. We worked
on native macOS support that doesn't rely on Rosetta. Use the
``--disable-rosetta`` flag to try it out. Note that the work isn't complete
yet.

* `Run npm install on bootstrap
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184639>`__
* `Remove "untested" warning
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182813>`__
* `Use ARM protoc version on ARM Macs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184930>`__
  (issue `#315998985 <https://issues.pigweed.dev/issues/315998985>`__)
* `Fix typo in error message
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184910>`__
* `Add flag to disable Rosetta
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184804>`__
  (issue `#315998985 <https://issues.pigweed.dev/issues/315998985>`__)
* `Retrieve armgcc for ARM Macs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184785>`__
  (issue `#315998985 <https://issues.pigweed.dev/issues/315998985>`__)
* `Change case of armgcc version
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184798>`__
  (issue `#315998985 <https://issues.pigweed.dev/issues/315998985>`__)
* `Roll cipd client
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184277>`__
* `Bump executing Python package
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183838>`__

pw_file
-------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183301>`__

pw_format
---------
Format strings can now be built by macros at compile-time by specifying the
format string as a set of string literals separated by the custom
``PW_FMT_CONCAT`` keyword.

* `Allow format strings to be composed at compile time
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184200>`__

pw_fuzzer
---------
* `Fix ambiguous reference to ContainerOf
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184284>`__

pw_hdlc
-------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183302>`__

pw_hex_dump
-----------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183302>`__

pw_i2c
------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183302>`__

pw_ide
------
``pw_ide`` now handles additional clang toolchains if your project specifies one
that's different from the one provided by Pigweed. We also shipped several UX
improvements to the Visual Studio Code extension.

* `Add command to build VSC extension
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184992>`__
* `Remove VSIX installation stuff
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184991>`__
* `Don't warn on missing extensions.json
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184895>`__
* `Alpha-sort the list of targets in VSC
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184864>`__
* `Auto-run build system command
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184809>`__
* `Update VSC Python config
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184820>`__
* `Fix condition for Windows platform
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184730>`__
* `Fix for clang installed to project dir
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184010>`__
  (issue `#314693384 <https://issues.pigweed.dev/issues/314693384>`__)

pw_kvs
------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183307>`__

pw_libc
-------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183307>`__

pw_log
------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183307>`__

pw_malloc
---------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183308>`__

pw_metric
---------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183308>`__

pw_perf_test
------------
* `Refactor event handler types
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179611>`__
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183309>`__

pw_presubmit
------------
* `Add LUCI_CONTEXT to ctx
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184793>`__
* `Merge some of the "misc" checks
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184778>`__
* `Better support downstream GnGenNinja use
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183837>`__
  (issue `#314818274 <https://issues.pigweed.dev/issues/314818274>`__)
* `Automatically compute trim_prefix
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183100>`__
  (issue `#282164634 <https://issues.pigweed.dev/issues/282164634>`__)

pw_random
---------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183350>`__

pw_result
---------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183350>`__

pw_rust
-------
* `Remove excess newline in doc command line
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182451>`__

pw_snapshot
-----------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183351>`__

pw_status
---------
* `Docs tweak
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185146>`__

pw_string
---------
* `Fix string_test build error under new clang revision
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184219>`__
  (issue `#315190328 <https://issues.pigweed.dev/issues/315190328>`__)
* `Add missing include
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183233>`__
  (issue `#314130408 <https://issues.pigweed.dev/issues/314130408>`__)

pw_symbolizer
-------------
* `Fix symbolizer_test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184226>`__
  (issue `#315190328 <https://issues.pigweed.dev/issues/315190328>`__)

pw_system
---------
We added an :ref:`extra logging channel <module-pw_system-logchannel>`.

* `Support extra logging channel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184410>`__
  (issue `#315540660 <https://issues.pigweed.dev/issues/315540660>`__)

pw_thread
---------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183352>`__

pw_tls_client
-------------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183352>`__

pw_tokenizer
------------
* `Mark \`pw_tokenizer_core\` as \`no_std\`
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183197>`__

pw_toolchain
------------
The Arm GCC toolchain now uses :ref:`module-pw_toolchain_bazel`.

* `Move ARM toolchain to new API
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183160>`__
  (issue `#309533028 <https://issues.pigweed.dev/issues/309533028>`__)
* `Use action configs from LLVM tool repo
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183010>`__
  (issue `#311257445 <https://issues.pigweed.dev/issues/311257445>`__)

pw_toolchain_bazel
------------------
Support for :py:attr:`additional_files` was added to reduce ``*_files``
boilerplate. We added a new :ref:`get started guide
<module-pw_toolchain_bazel-get-started>`.

* `Fix naming in docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184753>`__
* `Add misc_files group
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184698>`__
  (issue `#305737273 <https://issues.pigweed.dev/issues/305737273>`__)
* `Add automagic toolchain file collection
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184299>`__
  (issue `#305737273 <https://issues.pigweed.dev/issues/305737273>`__)
* `Add getting started guide
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184281>`__
  (issue `#309533028 <https://issues.pigweed.dev/issues/309533028>`__)
* `Remove legacy tool API
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184280>`__
* `Remove deprecated API
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183235>`__
  (issue `#309533028 <https://issues.pigweed.dev/issues/309533028>`__)
* `Rename build file templates part 2/2
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183187>`__

pw_trace
--------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183355>`__

pw_transfer
-----------
We added support for setting different timeouts for clients and servers.
See ``PW_TRANSFER_DEFAULT_MAX_CLIENT_RETRIES`` and
``PW_TRANSFER_DEFAULT_MAX_SERVER_RETRIES`` in :ref:`module-pw_transfer-config`.

* `Remove small hardcoded timeout in proxy_test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184638>`__
  (issue `#315459788 <https://issues.pigweed.dev/issues/315459788>`__)
* `Allow setting different timeouts for client and server
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184210>`__
* `Update integration test documentation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183163>`__
  (issue `#250947749 <https://issues.pigweed.dev/issues/250947749>`__)

.. _docs-changelog-20231215-pw_unit_test:

pw_unit_test
------------
We deprecated use of ``gtest/gtest.h`` header paths for tests intended to build
against ``pw_unit_test``. Historically, we supported using the ``gtest/gtest.h``
include path to support compiling a test as either a ``pw_unit_test`` or with
the full GoogleTest on larger targets like desktop. However, this created a
problem since ``pw_unit_test`` only implements a subset of the GoogleTest API.
With the new approach, we require tests that are intended to compile on devices
to directly include ``pw_unit_test/framework.h``. In cases where GoogleTest is
available, that header will redirect to GoogleTest.

* `Add compatibility in bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184911>`__
  (issue `#309665550 <https://issues.pigweed.dev/issues/309665550>`__)
* `Use googletest backend as a dep
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184656>`__
  (issue `#315351886 <https://issues.pigweed.dev/issues/315351886>`__)
* `Fix building fuzztests with cmake
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184268>`__
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183353>`__
* `Create facade's header
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183194>`__
  (issue `#309665550 <https://issues.pigweed.dev/issues/309665550>`__)

pw_varint
---------
* `Use unit test framework
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183354>`__

pw_web
------
* `Handle unrequested responses after call_id changes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184320>`__
* `Add support for call_id in RPC calls
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160792>`__
* `Add Mocha and Jest global vars to ESLint
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183931>`__
* `Switch to pre-made subset of icon fonts
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156852>`__
  (issue `#287270736 <https://issues.pigweed.dev/issues/287270736>`__)

Build
=====
* `Set Zephyr toolchain to llvm in presubmit
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179170>`__

Targets
=======
* `Point deprecated flag to new backend
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184262>`__
* (stm32f429i_disc1) `Fix test runner
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184378>`__

Language support
================
* (Python) `Remove references to non-existing setup.py
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182770>`__

OS support
==========
* (Zephyr) `Remove unecessary toolchain downloads & filter invalid targets
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184072>`__

Docs
====
.. todo-check: disable

* `Remove inaccurate #include statements
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/185190>`__
  (issue `#295023422 <https://issues.pigweed.dev/issues/295023422>`__)
* `Add Bazel code coverage TODO
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182812>`__
  (issue `#304833225 <https://issues.pigweed.dev/issues/304833225>`__)
* `Add sort by alignment to size optimizations
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184150>`__
* `Fix build command typo
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184170>`__
* `Update changelog
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183310>`__

.. todo-check: disable

SEEDs
=====
* `Always use build metadata titles in index table
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183750>`__
* (SEED-0001) `Add section about SEEDs & code changes
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177084>`__
* (SEED-0001) `Update number selection guidance
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184223>`__
* SEED-0117) `Open for comments
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184795>`__
* (SEED-0121) `Claim SEED number
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184225>`__

Miscellaneous
=============
* `Fix formatting after clang roll
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/184752>`__

-----------
Dec 1, 2023
-----------
Highlights (Nov 17, 2023 to Dec 1, 2023):

* We now have an auto-generated :ref:`seed-0000` that shows you the current
  status of all SEEDs.
* We've started implementing a :ref:`Rust API for pw_log
  <docs-changelog-20231201-pw_log>`.
* The :ref:`module-pw_alignment`, :ref:`module-pw_perf_test`, and
  :ref:`module-pw_status` docs were refactored to follow our latest
  :ref:`docs-contrib-docs-modules`.

Active SEEDs
============
Help shape the future of Pigweed! Please visit :ref:`seed-0000`
and leave feedback on the RFCs (i.e. SEEDs) marked
``Open for Comments``.

Modules
=======

pw_alignment
------------
The :ref:`docs <module-pw_alignment>` were updated to follow our new
:ref:`docs-contrib-docs-modules`.

* `Follow the new docs guidelines
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181432>`__

pw_allocator
------------
Utilities were added that make it easier to write tests for custom allocator
implementations. The metric collection API was refactored. CMake support for
heap poisoning was added.

* `Fix use-after-free in ~AllocatorForTest
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182950>`__
* `Add AllocationTestHarness
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183032>`__
* `Add AllocationTestHarness
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180532>`__
* `Refactor metric collection
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180456>`__
* `Improve heap poisoning configuration
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180372>`__

pw_bluetooth
------------
* `Add LE Set Host Feature command
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181770>`__
  (issue `#311639040 <https://issues.pigweed.dev/issues/311639040>`__)
* `LE Request Peer SCA Complete event
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182430>`__
  (issue `#311639272 <https://issues.pigweed.dev/issues/311639272>`__)
* `Fix LEExtendedAdvertisingReportData tx_power to be an Int
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181756>`__

pw_bluetooth_sapphire
---------------------
Migration of :ref:`module-pw_bluetooth_sapphire` into Pigweed has begun.

* `Use pw_test_group for fuzzers target
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183195>`__
  (issue `#307951383 <https://issues.pigweed.dev/issues/307951383>`__)
* `Use pw_fuzzer_group
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183168>`__
  (issue `#307951383 <https://issues.pigweed.dev/issues/307951383>`__)
* `Add sales pitch & roadmap docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181834>`__
  (issue `#312287470 <https://issues.pigweed.dev/issues/312287470>`__)
* `Add testonly to testing targets
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182890>`__
  (issue `#136961 <https://issues.fuchsia.dev/issues/136961>`__)
* `Revert commits to get to a known working state
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183014>`__
* `Use Write instead of UncheckedWrite
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182734>`__
* `Remove now unnecessary use of std::optional
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182490>`__
  (issue `#311256496 <https://issues.pigweed.dev/issues/311256496>`__)
* `Move LegacyLowEnergyScanner impl to base class
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182017>`__
  (issue `#305975969 <https://issues.pigweed.dev/issues/305975969>`__)
* `Create new LowEnergyScanner polymorphic methods
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182016>`__
  (issue `#305975969 <https://issues.pigweed.dev/issues/305975969>`__)
* `Extended scanning support, Fake(Controller|Peer)
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182015>`__
  (issue `#305975969 <https://issues.pigweed.dev/issues/305975969>`__)
* `Remove fidl fuzzer from build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182714>`__
  (issue `#307951383 <https://issues.pigweed.dev/issues/307951383>`__)
* `Use explicit constructor for SmartTask
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182013>`__
* `Follow pigweed style for test files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182012>`__
* `Add OWNERS file
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181759>`__
* `Delete unused build file
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181830>`__
  (issue `#307951383 <https://issues.pigweed.dev/issues/307951383>`__)
* `Fix pragma_once lint
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181394>`__
  (issue `#307951383 <https://issues.pigweed.dev/issues/307951383>`__)
* `Fix linter errors
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181305>`__
  (issue `#307951383 <https://issues.pigweed.dev/issues/307951383>`__)
* `Fix bazel formatting
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181303>`__
  (issue `#307951383 <https://issues.pigweed.dev/issues/307951383>`__)
* `Remove ICU from bazel build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181391>`__
  (issue `#311449154 <https://issues.pigweed.dev/issues/311449154>`__)
* `Refactor pw_bluetooth_sapphire & fix errors
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173484>`__
  (issue `#42051324 <https://issues.fuchsia.dev/issues/42051324>`__)
* `Update copyright headers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177800>`__

pw_build_info
-------------
* `Fix relative paths in Python tests
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182681>`__

pw_containers
-------------
A warning was added about ``pw::Vector`` being unsafe with
non-trivially-destructible, self-referencing types. See
`b/313899658 <https://issues.pigweed.dev/issues/313899658>`_.

* `Warn about unsafe Vector usage
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182970>`__
  (issue `#313899658 <https://issues.pigweed.dev/issues/313899658>`__)

pw_format
---------
A ``core::fmt`` macro helper was added.

* `Add core::fmt macro helper
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178537>`__

.. _docs-changelog-20231201-pw_log:

pw_log
------
An initial Rust API has been added.

* `Add initial Rust API
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178538>`__

pw_multibuf
-----------
* `Remove unused GN dep
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183165>`__
* `Remove dep on external gtest
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183158>`__
* `Make HeaderChunkRegionTracket public
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183041>`__
* `Fix cmake build file
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182898>`__

pw_package
----------
* `Add ICU package
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181269>`__
  (issue `#311449154 <https://issues.pigweed.dev/issues/311449154>`__)

pw_perf_test
------------
The :ref:`docs <module-pw_perf_test>` have been refactored.

* `Rename logging event handler files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178915>`__
* `Rework docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179610>`__

pw_presubmit
------------
.. todo-check: disable

* `Automatically compute trim_prefix
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183100>`__
  (issue `#282164634 <https://issues.pigweed.dev/issues/282164634>`__)
* `Allow full issues.pigweed.dev urls in TODO links
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183095>`__
* `Fix TestTodoCheck.test_one_bug_legacy()
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183094>`__
* `No coverage upload for shadow builds
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183093>`__
  (issue `#282164634 <https://issues.pigweed.dev/issues/282164634>`__)
* `Remove some unused constants
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183092>`__
* `Add is_shadow/is_prod members to context
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183077>`__
* `Refactor the coverage options
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182873>`__
  (issue `#282164634 <https://issues.pigweed.dev/issues/282164634>`__)
* `Show diffs when parser tests fail
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182971>`__
* `Trim paths in Bazel summaries
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182952>`__
* `Correct the codesearch_host
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182850>`__
  (issue `#261779031 <https://issues.pigweed.dev/issues/261779031>`__)
* `Correct host in coverage presubmit
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182702>`__
  (issue `#261779031 <https://issues.pigweed.dev/issues/261779031>`__)
* `Fix coverage options
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182666>`__
  (issue `#261779031 <https://issues.pigweed.dev/issues/261779031>`__)
* `Add Fuchsia style to todo_check_with_exceptions
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181433>`__
  (issue `#307951383 <https://issues.pigweed.dev/issues/307951383>`__)
* `Create Sapphire presubmit step
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177453>`__
  (issue `#42051324 <https://issues.fuchsia.dev/issues/42051324>`__)

.. todo-check: enable

pw_protobuf
-----------
* `Add common_py_pb2 target
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182650>`__
  (issue `#309351244 <https://issues.pigweed.dev/issues/309351244>`__)

pw_rpc
------
* `Initialize serde_ members to nullptr
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182851>`__

pw_rpc_transport
----------------
* `Unblock sockets when stopping
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181308>`__
  (issue `#309680612 <https://issues.pigweed.dev/issues/309680612>`__)

pw_sensor
---------
* `Reserve SEED for configuring sensors
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182805>`__
* `Claim SEED number for high level view
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182673>`__

pw_status
---------
The :ref:`docs <module-pw_status>` have been refactored to follow
our latest :ref:`docs-contrib-docs-modules`.

* `Adopt latest docs standard
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181181>`__

pw_string
---------
* `Add missing include
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183233>`__

pw_system
---------
* `Load all domain tokens
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181231>`__
* `Style fixes to Python scripts
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182661>`__
* `Add missing dependency on pw_trace
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181350>`__

pw_tokenizer
------------
* `Move entry header to a separate struct
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183193>`__
* `Catch accidental use of test macro
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183192>`__
* `Fix NULL dereference in fuzz harness
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182710>`__
* `Move ReadUint32
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169714>`__

pw_toolchain
------------
The Arm toolchain has been updated to use the new toolchain specified
in :ref:`seed-0113`. A helper for registering C/C++ toolchains in Bazel
was added to enable upstream Pigweed to make changes without needing to
manually update downstream projects. See
:ref:`module-pw_toolchain-bazel-upstream-pigweed-toolchains`.

* `Move ARM toolchain to new API
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183160>`__
  (issue `#309533028 <https://issues.pigweed.dev/issues/309533028>`__)
* `Use action configs from LLVM tool repo
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183010>`__
  (issue `#311257445 <https://issues.pigweed.dev/issues/311257445>`__)
* `Add Bazel toolchain registration helper
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183157>`__
  (issue `#301336229 <https://issues.pigweed.dev/issues/301336229>`__)
* `Merge host toolchains
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181760>`__
* `Expose non-hermetic toolchain
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181330>`__
  (issue `#299151946 <https://issues.pigweed.dev/issues/299151946>`__)
* `Only fetch compatible Rust toolchains
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181306>`__

pw_toolchain/arm_clang
----------------------
* `Reduce binary size
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169576>`__
  (issue `#254541584 <https://issues.pigweed.dev/issues/254541584>`__)

pw_toolchain_bazel
------------------
Support for binding tools to toolchains was added. See :py:class:`pw_cc_tool`
and :py:class:`pw_cc_action_config`. Support for featureless sysroots was
added. See :py:attr:`pw_cc_toolchain.builtin_sysroot` and
:py:attr:`pw_cc_toolchain.cxx_builtin_include_directories`.

* `Remove deprecated API
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183235>`__
  (issue `#309533028 <https://issues.pigweed.dev/issues/309533028>`__)
* `Rename build file templates part 2/2
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183187>`__
* `Rename build file templates part 1/2
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183186>`__
* `Add LLVM clang tool template
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182969>`__
  (issue `#311257445 <https://issues.pigweed.dev/issues/311257445>`__)
* `Add ARM GCC toolchain template
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182968>`__
  (issue `#309533028 <https://issues.pigweed.dev/issues/309533028>`__)
* `Support featureless sysroots
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181833>`__
  (issue `#309533028 <https://issues.pigweed.dev/issues/309533028>`__)
* `Mirror features to be flag sets
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181832>`__
  (issue `#309533028 <https://issues.pigweed.dev/issues/309533028>`__)
* `Add initial pw_cc_action_config support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180842>`__
  (issue `#309533028 <https://issues.pigweed.dev/issues/309533028>`__)

pw_transfer
-----------
Commands in the :ref:`integration test docs
<module-pw_transfer-integration-tests>` were updated and docs were
added that explain how to :ref:`run more than one instance of tests
in parallel <module-pw_transfer-parallel-tests>`.

* `Update integration test documentation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183163>`__
* `Set clients to transfer_v2
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183050>`__
  (issue `#309686987 <https://issues.pigweed.dev/issues/309686987>`__)
* `Limit to sending a single chunk in tests
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182665>`__
  (issue `#295037376 <https://issues.pigweed.dev/issues/295037376>`__)
* `Don't "block-network" by default
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182010>`__
  (issue `#311297881 <https://issues.pigweed.dev/issues/311297881>`__)
* `Use StatusCode in integration tests
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180828>`__
* `Make integration_test_server testonly, fix fx roller
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182019>`__
  (issue `#312493408 <https://issues.pigweed.dev/issues/312493408>`__)
* `Tag integration tests block-network
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181297>`__
  (issue `#311297881 <https://issues.pigweed.dev/issues/311297881>`__)

pw_unit_test
------------
* `Skip googletest tests if not set
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/183089>`__

pw_web
------
* `Install Web Test Runner and dependencies
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181870>`__

Build
=====

Bazel
-----
More Bazel information has been added to :ref:`docs-module-structure`.

* `Add simple module configuration mechanism
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181032>`__
  (issue `#234872811 <https://issues.pigweed.dev/issues/234872811>`__)
* `Tidy up WORKSPACE
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181292>`__
* `Rename Python toolchains
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181762>`__
  (issue `#310293060 <https://issues.pigweed.dev/issues/310293060>`__)
* `Remove py_proto_library wrapper
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180825>`__
  (issue `#266950138 <https://issues.pigweed.dev/issues/266950138>`__)
* `Use py_proto_library from rules_python
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180537>`__
* `Partial pw_system_console fix
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181304>`__
  (issue `#310307709 <https://issues.pigweed.dev/issues/310307709>`__)

Docs
====
The tool that we use to semi-automate these changelog updates has been
added to the main Pigweed repository. Try out the tool on
:ref:`docs-contrib-docs-changelog` and see
``//docs/sphinx/_static/js/changelog.js`` to view its implementation.

* `Gerrit code coverage documentation
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182799>`__
  (issue `#282164634 <https://issues.pigweed.dev/issues/282164634>`__)
* `Move copyright header info to style guide
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182795>`__
* `Document the Test footer
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181752>`__
* `Add changelog update instructions and tool
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181765>`__
* `Update changelog
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181393>`__

SEEDs
=====
We now have an auto-generated :ref:`seed-0000` that shows you the current
status of all SEEDs.

* `Generate the SEED index table
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181267>`__
* (SEED-0114) `Update status; format header in table
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182872>`__
* (SEED-0114) `Channels
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175471>`__
* (SEED-0118) `Claim SEED number
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/182654>`__
* (SEED-0118) `Claim SEED number
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181837>`__

Third party
===========
* `Add GN rules for ICU
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181311>`__
  (issue `#311449154 <https://issues.pigweed.dev/issues/311449154>`__)

------------
Nov 15, 2023
------------
Highlights (Nov 02, 2023 to Nov 15, 2023):

* The API for writing proc macros with pw_format was simplified.
* ``pw_emu`` added a command for resuming the execution of paused emulators
  and now has limited support for inserting environment variables into
  configuration entries.
* ``pw_ide`` can now output logs to files.
* ``pw_unit_test`` added support for GoogleTest's
  ``ASSERT_OK_AND_ASSIGN``, ``StatusIs``, and ``IsOkAndHolds``.
* Pigweed's :ref:`docs-mission` are now documented.

Active SEEDs
============
Help shape the future of Pigweed! Please leave feedback on the following active RFCs (SEEDs):

* `SEED-0114: Channels
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175471>`__
* `SEED-0115: Sensors
  <http://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176760>`__
* `SEED-0116: Sockets
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177696>`__

Modules
=======

pw_allocator
------------
The ``...Unchecked`` methods have been removed from the
``pw::allocator::Allocator`` interface and the NVI-style ``Do...`` methods
have been modified to take ``Layout`` parameters.

* `Update interface based on final SEED-0110 design
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176754>`__
* `Refactor test support and example allocator
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177653>`__
  (issue `#306686936 <https://issues.pigweed.dev/issues/306686936>`__)

pw_analog
---------
* `Mark libs as test only in bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179612>`__
  (issue `#309665550 <https://issues.pigweed.dev/issues/309665550>`__)

pw_console
----------
``SocketClient`` has been updated to support both IPv4 and IPv6 addresses
in addition to Unix sockets.

* `Add docs banner
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180824>`__
* `Improve SocketClient addressing
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178921>`__

pw_emu
------
There is now limited supported for inserting environment variable values
into configuration entries. A command for resuming the execution
of a paused emulator was added.

* `Add support for substitutions in config entries
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179150>`__
  (issue `#308793747 <https://issues.pigweed.dev/issues/308793747>`__)
* `Add resume command to CLI
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179030>`__
  (issue `#308793747 <https://issues.pigweed.dev/issues/308793747>`__)
* `Fix CLI gdb and load commands
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178922>`__
  (issue `#308793747 <https://issues.pigweed.dev/issues/308793747>`__)

pw_env_setup
------------
* `Make pigweed_environment.gni content gni-relative
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180991>`__
* `Update Bazel to 7.0.0 pre-release
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178950>`__

pw_format
---------
The API for writing `proc macros </rustdoc/pw_format/#proc-macros>`__ that take
format strings and arguments was simplified.

* `Add tests for macro helpers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181030>`__
* `Generalize format macro handling
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178132>`__

pw_fuzzer
---------
* `Move \`Domain\` from fuzztest::internal to fuzztest
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178213>`__
* `Switch oss-fuzz build to Bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175618>`__

pw_i2c
------
* `Mark libs as test only in bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179836>`__
  (issue `#309665550 <https://issues.pigweed.dev/issues/309665550>`__)

pw_ide
------
Logs can now be output to files.

* `Set 3-space tabs in VS Code
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179671>`__
* `Support output to logs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163573>`__
* `Remove redundant licence
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179613>`__
* `Remove clangd auto-restart
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171691>`__
* `Make Sphinx extensions upstream-only
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171690>`__
* `VSC extension 0.1.1 release
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171070>`__

pw_perf_test
------------
* `Reogranize source files
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178914>`__

pw_presubmit
------------
* `Create new fuzz program
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181134>`__
  (issue `#311215681 <https://issues.pigweed.dev/issues/311215681>`__)
* `Add examples showing how to create formatters
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180310>`__
* `Correct coverage ref
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179452>`__
  (issue `#279161371 <https://issues.pigweed.dev/issues/279161371>`__)

pw_stream
---------
* `Fix use of shutdown on Windows
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180401>`__
  (issue `#309680612 <https://issues.pigweed.dev/issues/309680612>`__)

pw_system
---------
* `Add tracing to the demo system
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168834>`__

pw_system_demo
--------------
* `Add clang to default stm32f4 build
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178930>`__
  (issue `#301079199 <https://issues.pigweed.dev/issues/301079199>`__)

pw_tokenizer
------------
* `Add Java to supported languages list
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179251>`__

pw_toolchain
------------
* `Set alwayslink = 1 when using --wrap
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180930>`__
* `Add objdump
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175810>`__

pw_toolchain_bazel
------------------
Core building blocks from the :ref:`seed-0113` plan were implemented:
:py:class:`pw_cc_flag_set` and :py:class:`pw_cc_flag_group`.

* `Introduce pw_cc_flag_set and pw_cc_flag_group
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179932>`__
  (issue `#309533028 <https://issues.pigweed.dev/issues/309533028>`__)

pw_trace_tokenized
------------------
* `Add a transfer based trace service
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168833>`__

pw_transfer
-----------
There's been a concerted effort to reduce ``pw_transfer`` test flakiness.

* `Limit data sent in handler clear test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180951>`__
  (issue `#297355578 <https://issues.pigweed.dev/issues/297355578>`__)
* `Limit data sent in manual cancel test
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180826>`__
* `Use project-absolute imports for test fixture
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180390>`__
  (issue `#310038737 <https://issues.pigweed.dev/issues/310038737>`__)
* `Prevent accidental timeouts in unit tests
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180274>`__

pw_unit_test
------------
The :ref:`module-pw_unit_test-api-expect` and
:ref:`module-pw_unit_test-api-assert` APIs were documented. Support for
GoogleTest's ``ASSERT_OK_AND_ASSIGN``, ``StatusIs``, and ``IsOkAndHolds`` was
added.

* `Document ASSERT_ and EXPECT_ macros
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179873>`__
* `Include the right gmock header
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180030>`__
  (issue `#309665550 <https://issues.pigweed.dev/issues/309665550>`__)
* `Mark libs as test only in bazel
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179874>`__
  (issue `#309665550 <https://issues.pigweed.dev/issues/309665550>`__)
* `Support *_NEAR, *_FLOAT_EQ, *_DOUBLE_EQ
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179770>`__
* `Allow googletest_test_matchers_test to run
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179450>`__
* `Add more googletest test matchers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179151>`__
* `Add googletest test matchers
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177878>`__

pw_watch
--------
Support for ``httpwatcher`` was removed because it's not supported on modern
versions of Python.

* `Remove httpwatcher support
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179250>`__
  (issue `#304603192 <https://issues.pigweed.dev/issues/304603192>`__)

pw_web
------
The log viewer has been polished and testing has been enhanced.

* `Fix LogViewControls responsive behavior
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179470>`__
  (issue `#308993282 <https://issues.pigweed.dev/issues/308993282>`__)
* `Resume autoscroll with clear logs event
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179252>`__
* `Fix clear logs due to error thrown handling input text
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176867>`__
* `Add manual testing page in docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178923>`__
  (issue `#288587657 <https://issues.pigweed.dev/issues/288587657>`__)

Build
=====
* `Update the default C++ standard
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178913>`__

Bazel
-----
* `Upgrade nanopb version
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180871>`__
* `Update comment
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180815>`__
* `Set --incompatible_default_to_explicit_init_py
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/180454>`__
  (issue `#266950138 <https://issues.pigweed.dev/issues/266950138>`__)
* `Make pw_cc_library an alias for cc_library
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178924>`__
  (issue `#267498492 <https://issues.pigweed.dev/issues/267498492>`__)
* `Don't disable use_header_modules
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178565>`__
  (issue `#267498492 <https://issues.pigweed.dev/issues/267498492>`__)

Targets
=======
.. todo-check: disable

* (``stm32f429i_disc1_stm32cube``)
  `Update TODO
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179172>`__

.. todo-check: enable

Language support
================
* (Python) `Update constraint.list
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179614>`__
* (Python) `Upgrade parameterized package
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179451>`__

Docs
====
A document about Pigweed's :ref:`docs-mission` was added. The
:ref:`style guide <docs-pw-style>` was split into multiple pages.

* `Update Pigweed Live dates
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/181031>`__
* `Add mission & philosophies
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178910>`__
* `Add Contribution Standards section
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179171>`__
* `Add details to codependent docs
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179879>`__
* `Update changelog
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178911>`__
  (issue `#292247409 <https://issues.pigweed.dev/issues/292247409>`__)
* `Split the style guide: Doxygen & Sphinx
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178912>`__
* `Split the style guide: C++
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178952>`__
* `Split the style guide: commit style
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178490>`__

SEEDs
=====
* (SEED-0110) `Correct status
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/179436>`__
* (SEED-0110) `Memory Allocation Interfaces
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168772>`__
* (SEED-0113) `Add modular Bazel C/C++ toolchain API
  <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173453>`__

-----------
Nov 3, 2023
-----------
Highlights (Oct 19, 2023 to Nov 3, 2023):

* A lot more of the :cpp:class:`pw::multibuf::Chunk` API was implemented.
* :ref:`module-pw_format` is a new module dedicated to Rust format string parsing.
* The tokenizer prefix is now configurable via
  ``PW_TOKENIZER_NESTED_PREFIX_STR``.
* References to C++14 have been removed throughout the codebase. Pigweed no
  longer supports C++14; C++17 or newer is required.
* The upstream Pigweed GN build is now
  :ref:`more isolated <docs-changelog-20231103-pw_build>` so that downstream
  projects have less conflicts when importing Pigweed into their existing GN
  build.
* Build configuration is moving away from Bazel macros like ``pw_cc_library``
  and towards the toolchain configuration so that downstream projects can have
  :ref:`full control <docs-changelog-20231103-bazel>` over how Pigweed libraries
  are built.
* New guidelines for authoring module docs have been published at
  :ref:`docs-contrib-docs-modules`. :ref:`module-pw_string` is now an example
  of a "golden" module docs set that follows the new guidelines. Please leave
  feedback on the new guidelines (and module docs updated to follow the
  guidelines) in `issue #309123039 <https://issues.pigweed.dev/issues/309123039>`__.


Active SEEDs
============
Help shape the future of Pigweed! Please leave feedback on the following active RFCs (SEEDs):

* `SEED-0103: pw_protobuf Object Model <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/133971>`__
* `SEED-0106: Project Template <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155430>`__
* `SEED-0110: Memory Allocation Interfaces <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168772>`__
* `SEED-0113: Modular Bazel C/C++ Toolchain API <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173453>`__
* `SEED-0114: Channels <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175471>`__
* `SEED-0115: Sensors <http://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176760>`__
* `SEED-0116: Sockets <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177696>`__

Modules
=======

pw_allocator
------------
The docs now have an auto-generated size report.
``pw::allocator::SplitFreeListAllocator`` has a new ``blocks()`` method for getting the
range of blocks being tracked. The class was also refactored to
use the existing ``Block`` API. The ``Block`` API itself was refactored to
encode offsets and flags into fields.

* `Add size reporting <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178370>`__
* `Return Range from SplitFreeListAllocator <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177807>`__
* `Refactor SplitFreeListAllocator to use Block <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176579>`__
* `Refactor Block to use encoded offsets <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176578>`__

pw_arduino_build
----------------
* `STM32 Core fixes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177750>`__

pw_assert
---------
* `Update print_and_abort backend formatting <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177845>`__

pw_bluetooth
------------
More :ref:`Emboss <module-pw_third_party_emboss>` definitions were added.

.. todo-check: disable

* `Add TODO for issue 308794058 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/151070>`__
  (issue `#308794058 <https://issues.pigweed.dev/issues/308794058>`__)
* `Remove anonymous entry in LEPeerAddressTypeNoAnon <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177881>`__
* `Separate LEAddressType and LEExtendedAddressType <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178010>`__
* `Define LEExtendedCreateConnectionV1 Emboss structure <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176574>`__
  (issue `#305976440 <https://issues.pigweed.dev/issues/305976440>`__)
* `Define LEEnhancedConnectionCompleteSubeventV1 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176576>`__
  (issue `#305976440 <https://issues.pigweed.dev/issues/305976440>`__)
* `Remove padding from Emboss command definitions <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176772>`__

.. todo-check: enable

.. _docs-changelog-20231103-pw_build:

pw_build
--------
Pigweed used to inject a selection of recommended configs into every ``pw_*``
C/C++ target in the GN build. These were previously only possible to remove
with the ``remove_configs`` argument. These configs are now bundled with
toolchains instead, and if you don't use a Pigweed-style toolchain you'll
no longer need to find ways to strip the default configs from Pigweed build rules.
More importantly, this changes makes Pigweed's recommended configs behave
identically to other toolchain configs, and they're now more clearly part of
GN toolchain definitions. This change is transparent to most projects, but some
Pigweed customers have been asking for this for a while.

The :ref:`module-pw_build-bazel-empty_cc_library` Bazel utility was added.

* `Add empty_cc_library <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178555>`__
* `Remove pw_build_default_configs_in_toolchain <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177894>`__
* `Apply pigweed_default_configs in toolchain <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/120610>`__
  (issue `#260111641 <https://issues.pigweed.dev/issues/260111641>`__)
* `Fix blob attribute ordering <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177458>`__
* `Only use -Wextra-semi on C++ files with GCC <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177171>`__
  (issues `#301262374 <https://issues.pigweed.dev/issues/306734552>`__,
  `#301262374 <https://issues.pigweed.dev/issues/301262374>`__)
* `Silence Windows-specific warnings <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177172>`__

pw_bytes
--------
A new ``_b`` literal was added to make it easier to create bytes for tests
and constants.

* `Add _b suffix for byte literals <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178134>`__

pw_containers
-------------
The reference docs for the variable length entry queue API in C and Python
were updated.

* `Update VariableLengthEntryQueue size functions; cleanup <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173454>`__

pw_digital_io_mcuxpresso
------------------------
* `Remove RT595 size def <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178353>`__

pw_doctor
---------
* `Trivial linter fixes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176939>`__

pw_emu
------
* `renode: Show more details when failing to connect <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178563>`__
  (issue `#307736513 <https://issues.pigweed.dev/issues/307736513>`__)

pw_env_setup
------------
``pip`` has been pinned to ``23.2.1`` and ``pip-tools`` to ``7.3.0`` to
prevent dependency resolution problems.

* `Pin pip and pip-tools <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177834>`__
* `Update protoc to 2@24.4 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177050>`__
  (issue `#306461552 <https://issues.pigweed.dev/issues/306461552>`__)

pw_format
---------
:ref:`module-pw_format` is a new module dedicated to Rust format string parsing.

* `Correct crate name in docs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178078>`__
* `Move Rust format string parsing into its own module <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168362>`__

pw_fuzzer
---------
* `Inline NonOkStatus() <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178212>`__
* `Fix instrumentation config <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178214>`__

.. _docs-changelog-20231103-pw_hdlc:

pw_hdlc
-------
Using read callbacks in ``RpcClient`` is no longer accepted and the use of
``CancellableReader`` is now enforced because it provides a safe and clean
shutdown process.

* `Enforce use of CancellableReader <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173618>`__
  (issue `#301496598 <https://issues.pigweed.dev/issues/301496598>`__)

pw_libcxx
---------
:ref:`module-pw_libcxx` is a new module that provides ``libcxx`` symbols and
will eventually facilitate pulling in headers as well.

* `Add pw_libcxx library <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/144970>`__

pw_log
------
A :ref:`module-pw_log-bazel-backend_impl` label flag was added to Bazel to
avoid circular dependencies.

* `Enable sandboxing for pigweed genrules <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178550>`__
  (issue `#307824623 <https://issues.pigweed.dev/issues/307824623>`__)
* `Introduce backend_impl label flag <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177842>`__
  (issue `#234877642 <https://issues.pigweed.dev/issues/234877642>`__)

pw_multibuf
-----------
A lot more of the :cpp:class:`pw::multibuf::Chunk` API was implemented.

* `Add basic MultiBuf operations <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178036>`__
* `Add Chunk::Merge <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177636>`__
* `Fix TrackingAllocatorWithMemory UAF <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177694>`__
* `Add module and Chunk implementation <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173951>`__

pw_package
----------
* `Use mirror for stm32cube <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/142510>`__
  (issue `#278914999 <https://issues.pigweed.dev/issues/278914999>`__)
* `Fix Zephyr URL <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177456>`__

pw_presubmit
------------
A CSS formatter was added.

* `Add basic CSS formatter <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178810>`__
  (issue `#308948504 <https://issues.pigweed.dev/issues/308948504>`__)
* `Kalypsi-based coverage upload <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175070>`__
  (issue `#279161371 <https://issues.pigweed.dev/issues/279161371>`__)
* `Handle missing upstream better <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177038>`__
  (issue `#282808936 <https://issues.pigweed.dev/issues/282808936>`__)
* `Trivial linter fixes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176939>`__

pw_protobuf
-----------
* `Enable sandboxing for pigweed genrules <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178550>`__
  (issue `#307824623 <https://issues.pigweed.dev/issues/307824623>`__)

pw_rpc
------
:ref:`pw::rpc::SynchronousCallFor() <module-pw_rpc-client-sync-call-wrappers>`
now supports :ref:`DynamicClient <module-pw_rpc_pw_protobuf-client>`.

* `Update Java service error with tip <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178812>`__
  (issue `#293361955 <https://issues.pigweed.dev/issues/293361955>`__)
* `Support DynamicClient with SynchronousCallFor API <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177637>`__

pw_string
---------
The docs were updated to match the new :ref:`docs-contrib-docs-modules`.

* `Docs tweaks <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177883>`__

pw_sys_io
---------
Backends that depend on ``default_putget_bytes`` were updated to express the
dependency.

* `Fix Bazel backends <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177656>`__

pw_system
---------
See :ref:`docs-changelog-20231103-pw_hdlc` for an explanation of the
``CancellableReader`` change.

* `Enforce use of CancellableReader <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173618>`__
  (issue `#301496598 <https://issues.pigweed.dev/issues/301496598>`__)

pw_tls_client
-------------
* `Update to new boringssl API <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178150>`__

pw_tokenizer
------------
The tokenizer prefix is now configurable via ``PW_TOKENIZER_NESTED_PREFIX_STR``.

* `Enable sandboxing for pigweed genrules <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178550>`__
  (issue `#307824623 <https://issues.pigweed.dev/issues/307824623>`__)
* `Let tokenizer prefix be configurable <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177575>`__

pw_toolchain
------------
You can now set the ``dir_pw_third_party_builtins`` GN var to your
``compiler-rt/builtins`` checkout to enable buildings LLVM ``builtins`` from
source instead of relying on a shipped ``libgcc``.

* `Apply pigweed_default_configs in toolchain <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/120610>`__
  (issue `#260111641 <https://issues.pigweed.dev/issues/260111641>`__)
* `Build compiler-rt builtins to replace libgcc <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/144050>`__

pw_unit_test
------------
* `Pass verbose flag to TestRunner <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177470>`__

pw_web
------
* `Limit component rerendering <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177810>`__
  (issue `#307559191 <https://issues.pigweed.dev/issues/307559191>`__)

Build
=====
References to C++14 have been removed throughout the codebase. Pigweed no
longer supports C++14; C++17 or newer is required.

* `Drop C++14 compatibility from the build and docs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177610>`__

.. _docs-changelog-20231103-bazel:

Bazel
-----
Build configuration is moving away from Bazel macros like ``pw_cc_library``
and towards the toolchain configuration so that downstream projects can have
full control over how Pigweed libraries are built.

* `Move Kythe copts to toolchain configuration <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178592>`__
  (issue `#267498492 <https://issues.pigweed.dev/issues/267498492>`__)
* `Move warnings to toolchain configuration <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178557>`__
  (issue `#240466562 <https://issues.pigweed.dev/issues/240466562>`__)
* `Silence warnings from external code <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178436>`__
  (issue `#300330623 <https://issues.pigweed.dev/issues/300330623>`__)
* `stm32cube support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177134>`__
* `Remove most copts from pw_cc_library macro <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170824>`__
  (issue `#267498492 <https://issues.pigweed.dev/issues/267498492>`__)

Targets
=======
``pw_assert_BACKEND`` for :ref:`target-host` was set to
``print_and_abort_check_backend`` to enable compatibility with GoogleTest death
tests.

* (``host``) `Change pw_assert_BACKEND <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177835>`__

OS support
==========
* (``zephyr``) `Update checkout to v3.5 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177669>`__

Docs
====
New guidelines for authoring module docs have been published at
:ref:`docs-contrib-docs-modules`. :ref:`module-pw_string` is now an example
of a "golden" module docs set that follows the new guidelines. Please leave
feedback on the new guidelines (and module docs updated to follow the
guidelines) in `issue #309123039 <https://issues.pigweed.dev/issues/309123039>`__.

There's now a definition for :ref:`docs-glossary-facade` in the glossary.

* `Update module docs authoring guidelines <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177465>`__
* `Fix nav and main content scrolling <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178591>`__
  (issue `#303261476 <https://issues.pigweed.dev/issues/303261476>`__)
* `Add udev instructions to Bazel Get Started <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178435>`__
* `Add information on the experimental repo to contributing.rst <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178272>`__
* `Mention command for updating Py dep hashes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177799>`__
* `Define facade in glossary <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177632>`__
* `Remove symlinks to files that were removed <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177530>`__
* `Mention upstream development guide in contributor guidelines <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177459>`__
* `Move all images out of the repo <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176751>`__
* `Update changelog <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177085>`__
  (issue `#292247409 <https://issues.pigweed.dev/issues/292247409>`__)
* `Move CoC to Contributors section of sitenav <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177071>`__

SEEDs
=====
* (SEED-0107) `Update SEED references; fix typo <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177698>`__
* (SEED-0112) `Async Poll Model <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168337>`__
* (SEED-0115) `Fix link <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177093>`__
* (SEED-0116) `Claim SEED number <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177697>`__

Third party
===========
* (nanopb) `Detect protoc updates <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177650>`__

------------
Oct 20, 2023
------------
Highlights (Oct 5, 2023 to Oct 20, 2023):

* ``pw_emu`` has launched! Check out :ref:`module-pw_emu` to get started.
  See :ref:`seed-0108` for background.
* :ref:`module-pw_log-tokenized-args` are now supported. See :ref:`seed-0105`
  for background.
* The new :cpp:class:`pw::allocator::UniquePtr` class offers a safer, simpler
  RAII API for allocating individual values within an allocator.
* A few SEEDs were accepted: :ref:`seed-0105`, :ref:`seed-0109`, and
  :ref:`seed-0111`.
* Lots of new docs, including a guide for
  :ref:`getting started with Bazel <docs-get-started-bazel>`, a
  conceptual explanation of :ref:`facades and backends <docs-facades>`,
  and an eng blog post detailing :ref:`Kudzu <docs-blog-01-kudzu>`, an
  electronic badge that the Pigweed team made for Maker Faire 2023.

Active SEEDs
============
Help shape the future of Pigweed! Please leave feedback on the following active RFCs (SEEDs):

* `SEED-0103: pw_protobuf Object Model <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/133971>`__
* `SEED-0106: Project Template <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155430>`__
* `SEED-0110: Memory Allocation Interfaces <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168772>`__
* `SEED-0113: Modular Bazel C/C++ Toolchain API <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173453>`__
* `SEED-0114: Channels <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175471>`__
* `SEED-0115: Sensors <http://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176760>`__

Modules
=======

pw_allocator
------------
The new :cpp:class:`pw::allocator::UniquePtr` class offers a safer, simpler
RAII API for allocating individual values within an allocator.

* `Fix SplitFreeListAllocator region alignment <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175232>`__
* `Add UniquePtr\<T\> <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176781>`__

pw_async
--------
* `Add CMake support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175475>`__

pw_async_basic
--------------
* `Add missing include <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175476>`__
* `Fix build error when using pw_async:heap_dispatcher <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173480>`__

pw_bluetooth
------------
* `Define LEChannelSelectionAlgorithmSubevent <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176577>`__
* `Define LEScanTimeoutSubevent <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176575>`__
  (issue `#265052417 <https://issues.pigweed.dev/issues/265052417>`__)
* `Use $size_in_bits instead of hardcoding size <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176573>`__
* `Switch from parameterized value to determining at run time <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176572>`__
  (issue `#305975969 <https://issues.pigweed.dev/issues/305975969>`__)
* `Fix size reports <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173620>`__

pw_build
--------
:ref:`module-pw_build-bazel-pw_linker_script` now describes how to work
with linker scripts.

* `Update pw_linker_script docs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174848>`__
* `Move pw_linker_script rule definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174872>`__

pw_chre
-------
* `Remove TODOs for CHRE MacOS support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175490>`__

pw_cli
------
* `Honor NO_COLOR and CLICOLOR_FORCE <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176860>`__
* `Use typing.Literal <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176778>`__

pw_digital_io
-------------
* `Add Android.bp for proto/rpc <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176270>`__

pw_emu
------
The module has launched! Check out :ref:`module-pw_emu` to get started.

* `renode: Increase start timeout to 120s <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176865>`__
* `Fix pid file race condition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176782>`__
* `mock_emu: start listening before making the port available <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176856>`__
  (issue `#306155313 <https://issues.pigweed.dev/issues/306155313>`__)
* `qemu: Force using IPv4 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176430>`__
  (issue `#305810466 <https://issues.pigweed.dev/issues/305810466>`__)
* `Add renode support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173613>`__
* `Add QEMU support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173612>`__
* `core: Let the OS terminate foreground emulator processes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175638>`__
* `Add user APIs and the command line interface <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173611>`__
* `Add core components <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173610>`__
* `Add Emulators Frontend module boilerplate <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162096>`__

pw_env_setup
------------
* `Allow disabling CIPD cache <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176650>`__
* `Add prpc <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175236>`__

pw_function
-----------
* `Move pw_function_CONFIG to .gni <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173652>`__

pw_hdlc
-------
:ref:`module-pw_hdlc-api-rpc` now has much more information on how to use
``pw_hdlc`` for RPC in Python.

* `Update Python RPC documents <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174825>`__

pw_i2c
------
* `Fix accidental c++2a <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176511>`__
* `Add Android.bp for i2c proto/rpc <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176070>`__

pw_kvs
------
The new ``FlashPartitionWithLogicalSectors`` variant of ``FlashPartition``
supports combining multiple physical ``FlashMemory`` sectors into a single
logical ``FlashPartition`` sector.

* `Add FlashPartitionWithLogicalSectors <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/106917>`__

pw_log_tokenized
----------------
:ref:`module-pw_log-tokenized-args` are now supported. See :ref:`seed-0105` for background.

* `Add tokenized string args support to log backend <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/164514>`__

pw_log_zephyr
-------------
* `Clean-up unused dependencies from TOKENIZED_LIB <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174813>`__

pw_minimal_cpp_stdlib
---------------------
* `Support additional libraries <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173814>`__
* `Add Zephyr Kconfig to enable include path <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173653>`__

pw_package
----------
* `Update boringssl commit & skip clang-tidy <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175016>`__
* `Update Emboss commit <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173619>`__

pw_presubmit
------------
:ref:`module-pw_presubmit-presubmit-checks` has more guidance on when to use
``--base`` and ``--full``.

* `Add note about --full and --base <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175633>`__

pw_snapshot
-----------
* `More detokenization tests <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176759>`__

pw_spi
------
* `Fix cmake integration <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175190>`__

pw_sync_zephyr
--------------
* `Add TimedThreadNotification::try_acquire_until <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175478>`__

pw_system
---------
The ``Device`` class's constructor now accepts a ``logger`` argument
that enables you to specify which logger should be used.

* `Add option to pass logger to Device <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175075>`__

pw_third_party_freertos
-----------------------
* `Add arm_cm7_not_r0p1 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172382>`__

pw_thread
---------
* `More detokenization tests <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176759>`__

pw_thread_freertos
------------------
* `Fix extra wakeups when detaching threads <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175310>`__
  (issue `#303885539 <https://issues.pigweed.dev/issues/303885539>`__)

pw_tokenizer
------------
:ref:`module-pw_tokenizer-get-started-integration` has new guidance around
configuring linker scripts in Bazel.

* `Expose linker_script in BUILD.bazel <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175590>`__

pw_toolchain
------------
* `Exclude googletest from static analysis <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173482>`__

pw_transfer
-----------
* `Start the API reference <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170011>`__
  (issue `#299147635 <https://issues.pigweed.dev/issues/299147635>`__)

pw_web
------
* `Reduce table cell padding <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176690>`__
  (issue `#305022558 <https://issues.pigweed.dev/issues/305022558>`__)
* `Fix invisible jump button <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175330>`__
* `Enable manual color scheme setting <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173630>`__
  (issue `#301498553 <https://issues.pigweed.dev/issues/301498553>`__)

Build
=====
* `Fix pw_BUILD_BROKEN_GROUPS <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176114>`__
* `Update Android.bp <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175631>`__
  (issue `#277108894 <https://issues.pigweed.dev/issues/277108894>`__)

Bazel
-----
* `Don't autodetect C++ toolchain <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175613>`__
  (issue `#304880653 <https://issues.pigweed.dev/issues/304880653>`__)
* `Add O2 to arm_gcc toolchain <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175536>`__
  (issue `#299994234 <https://issues.pigweed.dev/issues/299994234>`__)

Targets
=======
* (rp2040_pw_system) `Enable time slicing <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175074>`__

OS support
==========
* (zephyr) `Allow direct CMake inclusions <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175477>`__

Docs
====
* `Move CoC to Contributors section of sitenav <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177071>`__
* `Create concepts section in sitenav <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/177037>`__
* `Add facades and backends page <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170602>`__
* `Add Bazel getting started tutorial <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176319>`__
* `Remove css class on Kudzu image captions <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176770>`__
* `Kudzu photos <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176710>`__
* `Refactor the getting started section <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176331>`__
* `Add sitemap <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176492>`__
* `Add hat tip for pixel doubling technique <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175639>`__
* `Start eng blog and add Kudzu page <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175619>`__
* `Add Pigweed Live directive <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174892>`__
* `Add builder viz to CI/CQ intro <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175414>`__
  (issue `#302680656 <https://issues.pigweed.dev/issues/302680656>`__)
* `Fix link <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175415>`__
  (issue `#302680656 <https://issues.pigweed.dev/issues/302680656>`__)
* `Add changelog highlight <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175231>`__
* `Update changelog <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174818>`__

SEEDs
=====
A few SEEDs were accepted and a few more started.

* (SEED-0105) `Add nested tokens to pw_tokenizer and pw_log <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154190>`__
* (SEED-0109) `Communication Buffers <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168357>`__
* (SEED-0111) `Update status, add link to SEED-0113 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176254>`__
* (SEED-0111) `Make Bazel Pigweed's Primary Build System <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171695>`__
* (SEED-0113) `Claim SEED number (Modular Bazel C/C++ Toolchain API) <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175510>`__
* (SEED-0114) `Claim SEED number (Channels) <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175412>`__
* (SEED-0115) `Clain SEED number (Sensors) <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176763>`__

Third party
===========
* (boringssl) `Remove crypto_sysrand.cc <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175017>`__
* (fuchsia) `Copybara import <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173651>`__
* (fuchsia) `Update copybara with fit/defer.h <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173481>`__

Miscellaneous
=============
* `Update formatting for new clang version <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/175311>`__
* `Use C++20 everywhere <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174630>`__
  (issue `#303371098 <https://issues.pigweed.dev/issues/303371098>`__)
* (revert) `Use .test convention" <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171793>`__
* `Add generated Emboss code <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/176571>`__

-----------
Oct 6, 2023
-----------
Highlights (Sep 21, 2023 to Oct 6, 2023):

* We expanded our RP2040 support. See the new :ref:`module-pw_chrono_rp2040`
  and :ref:`module-pw_digital_io_rp2040` modules.
* The :ref:`new CancellableReader class in pw_hdlc <docs-changelog-20231009-pw_hdlc>`
  is an interface for receiving RPC packets that guarantees its read process can be
  stopped.
* ``pw_rpc`` now :ref:`automatically generates a new DynamicClient interface
  <docs-changelog-20231009-pw_rpc>` when dynamic allocation is enabled.
* The Python backend for ``pw_tokenizer`` now supports :ref:`tokenizing strings as
  arguments <docs-changelog-20231009-pw_tokenizer>`.
* The ``pigweed_config`` mechanism in Bazel is now officially retired.

Active SEEDs
============
Help shape the future of Pigweed! Please leave feedback on the following active RFCs (SEEDs):

* `SEED-0103: pw_protobuf Object Model <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/133971>`__
* `SEED-0105: Add nested tokens and tokenized args to pw_tokenizer and pw_log <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154190>`__
* `SEED-0106: Project Template <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155430>`__
* `SEED-0109: Communication Buffers <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168357>`__
* `SEED-0110: Memory Allocation Interfaces <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168772>`__
* `SEED-0111: Make Bazel Pigweed's Primary Build System <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171695>`__
* `SEED-0112: Async Poll Model <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168337>`__

Modules
=======

.. _docs-changelog-20231009-pw_allocator:

pw_allocator
------------
We added a bunch of new allocator APIs! ``AllocatorMetricProxy`` is a wrapper for
``Allocator`` that tracks the number and total of current memory allocations as well
as peak memory usage. ``LibCAllocator`` is an allocator that uses ``malloc()`` and
``free()``. ``NullAllocator`` is an allocator that always fails which is useful for
disallowing memory allocations under certain circumstances. ``SplitFreeListAllocator``
uses a free list to reduce fragmentation. ``FallbackAllocator`` enables you to
specify primary and secondary allocators.

* `Add Android.bp <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173851>`__
* `Add pool accessors <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173615>`__
* `Move Resize assertion <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173614>`__
* `Add AllocatorMetricProxy <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172380>`__
* `Add LibCAllocator <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172232>`__
* `Add NullAllocator <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172233>`__
* `Add SplitFreeListAllocator <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172231>`__
* `Add FallbackAllocator <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171837>`__
* `Generic interface for allocators <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171709>`__

pw_analog
---------
* `Migrate MicrovoltInput to Doxygen <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170593>`__
  (issue `#299147635 <https://issues.pigweed.dev/issues/299147635>`__)

pw_async
--------
* `Add OWNERS file <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173690>`__

pw_bloat
--------
``pw_size_report()`` has a new ``json_key_prefix`` argument which is an
optional prefix for key names in JSON size reports and a new
``full_json_summary`` argument which provides more control over how
much detail is provided in a JSON size report.

* `Update API to allow verbose json content <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168718>`__
  (issue `#282057969 <https://issues.pigweed.dev/issues/282057969>`__)

pw_bluetooth
------------
* `Format Emboss files <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174832>`__
* `Update comments in HCI event defs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174070>`__
  (issue `#265052417 <https://issues.pigweed.dev/issues/265052417>`__)

pw_build
--------


* `Fix path in Bazel pw_linker_script <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174591>`__
* `Expose pw_linker_script in Bazel <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174590>`__
  (issue `#303482154 <https://issues.pigweed.dev/issues/303482154>`__)
* `Define empty configs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174490>`__
* `Add bazel implementation of pw_cc_blob_library <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173452>`__
  (issue `#238339027 <https://issues.pigweed.dev/issues/238339027>`__)
* `Clean up build_target.gni <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/120215>`__
  (issue `#260111641 <https://issues.pigweed.dev/issues/260111641>`__)
* `Allow add_global_link_deps to be overridden <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/150050>`__
* `Expose pigweed_default_configs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173372>`__
  (issue `#260111641 <https://issues.pigweed.dev/issues/260111641>`__)
* `Apply -Wextra-semi to C code as well as C++ <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172372>`__

pw_chre
-------
* `Update bug numbers <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172330>`__

pw_chrono
---------
* `Add clarification to is_nmi_safe <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174573>`__

pw_chrono_rp2040
----------------
This module is a new ``pw::chrono::SystemClock`` backend for RP2040.

* `System clock backend <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174651>`__

pw_cli
------
* `Update requires script <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/126101>`__
* `Narrow logic around colors <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173232>`__

pw_containers
-------------
There's a new C implementation for ``VariableLengthEntryDeque`` which is a
double-ended queue buffer that stores variable-length entries inline in a
circular (ring) buffer. The old ``VariableLengthEntryDeque`` was renamed
to ``VariableLengthEntryQueue``.

* `Add missing <utility> include for std::move <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173879>`__
* `Rename to VariableLengthEntryQueue <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173451>`__
* `Rename files to variable_length_entry_queue <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173450>`__
* `VariableLengthEntryDeque Entry struct <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173130>`__
* `VariableLengthEntryDeque C implementation <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169731>`__

pw_digital_io_rp2040
--------------------
This module is a new RP2040 backend for ``pw_digital_io``.

* `Implementation <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173550>`__
  (issue `#303255049 <https://issues.pigweed.dev/issues/303255049>`__)

pw_env_setup
------------
We made the Pigweed bootstrap process on Windows more robust.

* `Fix double bootstrap.bat failures on Windows <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172416>`__
  (issue `#300992566 <https://issues.pigweed.dev/issues/300992566>`__)
* `Better highlight bootstrap failure <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172415>`__
* `Fix double bootstrap.bat failures on Windows <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172410>`__
  (issue `#300992566 <https://issues.pigweed.dev/issues/300992566>`__)

.. _docs-changelog-20231009-pw_hdlc:

pw_hdlc
-------
The new ``CancellableReader`` class is a new interface for receiving RPC
packets that guarantees its read process can be stopped.

* `Add CancellableReader <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172051>`__
  (issue `#294858483 <https://issues.pigweed.dev/issues/294858483>`__)

pw_i2c
------
* `Fix docs to use MakeExpectedTransactionArray <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173570>`__
* `Add cmake integration <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172210>`__

pw_kvs
------
The new ``FlashPartitionWithLogicalSectors`` C++ class supports combining
multiple physical ``FlashMemory`` sectors into a single logical
``FlashPartition`` sector.

* `Add FlashPartitionWithLogicalSectors <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/106917>`__

pw_libc
-------
* `Don't implicitly link against global link_deps <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/150051>`__

pw_metric
---------
* `Make constructors constexpr <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172379>`__

pw_minimal_cpp_stdlib
---------------------
* `Update to compile with stdcompat <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173350>`__
* `Namespace public/internal to module <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173692>`__

pw_perf_test
------------
* `Gate on pw_chrono_SYSTEM_TIMER_BACKEND <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174650>`__

pw_presubmit
------------
* `Allow dots in module part of commit message <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174232>`__
* `Use autodoc for context classes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169119>`__
* `Allow passing kwargs to build.bazel <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173853>`__
  (issue `#302045722 <https://issues.pigweed.dev/issues/302045722>`__)
* `No env_with_clang_vars with bazel <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173656>`__

pw_ring_buffer
--------------
* `Minor build and docs updates <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173030>`__

.. _docs-changelog-20231009-pw_rpc:

pw_rpc
------
If dynamic allocation is enabled via ``PW_RPC_DYNAMIC_ALLOCATION`` a new
``DynamicClient`` is now generated which dynamically allocates the call
object with ``PW_RPC_MAKE_UNIQUE_PTR``.

* `Generate DynamicClient that dynamically allocates call objects <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168534>`__
* `Add CancellableReader <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172051>`__
  (issue `#294858483 <https://issues.pigweed.dev/issues/294858483>`__)

pw_rpc_transport
----------------
* `Add a test loopback service registry <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171114>`__
  (issue `#300663813 <https://issues.pigweed.dev/issues/300663813>`__)

pw_stream
---------
``pw_stream`` now has initial support for ``winsock2``.

* `Add Windows socket support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172413>`__

pw_sys_io_rp2040
----------------
* `Renamed from pw_sys_io_pico <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174910>`__

.. _docs-changelog-20231009-pw_tokenizer:

pw_tokenizer
------------
The Python backend now supports nested hashing tokenization. See
:ref:`module-pw_tokenizer-nested-arguments`.

* `Support nested hashing tokenization (python backend) <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/145339>`__
  (issue `#278890205 <https://issues.pigweed.dev/issues/278890205>`__)
* `Test for C99 support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170150>`__

pw_toolchain
------------
* `Add libc stub for gettimeofday, update visibility rules <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173850>`__
* `Link against pw_libc for host clang toolchains <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/151439>`__

pw_transfer
-----------
* `Start the API reference <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170011>`__
  (issue `#299147635 <https://issues.pigweed.dev/issues/299147635>`__)
* `Remove old test server <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172951>`__
  (issue `#234875234 <https://issues.pigweed.dev/issues/234875234>`__)

pw_unit_test
------------
* `Do not print contents of unknown objects <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174911>`__
* `Add more pw_unit_test_TESTONLY args <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173670>`__
  (issue `#234873207 <https://issues.pigweed.dev/issues/234873207>`__)
* `Add pw_unit_test_TESTONLY build arg <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171970>`__
  (issue `#234873207 <https://issues.pigweed.dev/issues/234873207>`__)

pw_watch
--------
* `Add link to served docs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173456>`__

pw_web
------
* `Make ongoing transfers accessible downstream <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174231>`__
* `TypeScript workarounds for disambiguation errors <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173590>`__
* `Throw error as an Error type <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173051>`__
* `Remove need for Buffer package in pw_hdlc <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172377>`__
* `Remove date-fns <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172371>`__

Build
=====
* `Fix extended default group <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174574>`__
  (issue `#279161371 <https://issues.pigweed.dev/issues/279161371>`__)
* `Fix \`all\` target in GN build <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173050>`__
* `Add an extended default group <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/110391>`__

Bazel
-----
* `Retire pigweed_config (part 3) <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172411>`__
* `Retire pigweed_config (part 2) <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170058>`__
  (issue `#291106264 <https://issues.pigweed.dev/issues/291106264>`__)

Docs
====
We started a :ref:`glossary <docs-glossary>` and added new docs about
:ref:`rollers <docs-rollers>` and :ref:`CI/CQ <docs-ci-cq-intro>`.

* `Add docs on rollers <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174770>`__
  (issue `#302680656 <https://issues.pigweed.dev/issues/302680656>`__)
* `Remove redundant auto-submit section <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174890>`__
  (issue `#302680656 <https://issues.pigweed.dev/issues/302680656>`__)
* `Reformat CI/CQ Intro <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174870>`__
  (issue `#302680656 <https://issues.pigweed.dev/issues/302680656>`__)
* `Move CI/CQ Intro to infra/ <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174776>`__
  (issue `#302680656 <https://issues.pigweed.dev/issues/302680656>`__)
* `Address comments on CI/CQ intro <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173932>`__
  (issue `#302680656 <https://issues.pigweed.dev/issues/302680656>`__)
* `Tidy up build system docs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173658>`__
* `Fix typo <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173872>`__
* `Add CI/CQ Intro <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173455>`__
  (issue `#302680656 <https://issues.pigweed.dev/issues/302680656>`__)
* `Add policy on incomplete docs changes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/173617>`__
* `Start the glossary <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172952>`__
* `Update changelog <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172810>`__
  (issue `#292247409 <https://issues.pigweed.dev/issues/292247409>`__)
* `Add Doxygen @endcode guidance <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172470>`__

SEEDs
=====
* (SEED-0112) `Fix link <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174771>`__

Miscellaneous
=============

pigweed.json
------------
* `Exclude patches.json from formatting <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/174230>`__
  (issue `#232234662 <https://issues.pigweed.dev/issues/232234662>`__)

------------
Sep 22, 2023
------------
Highlights (Sep 07, 2023 to Sep 22, 2023):

* ``pw_tokenizer`` has :ref:`new C++ methods for detokenizing
  Base64-encoded strings and new C functions for manually encoding tokenized
  messages that contain integers <docs-changelog-pw_tokenizer-20230922>`.
* ``pw::rpc::SynchronousCall`` now supports the use of :ref:`custom response message
  classes <docs-changelog-pw_rpc-20230922>`.
* The C API for ``pw_varint`` got :ref:`lots of ergonomic improvements
  <docs-changelog-pw_varint-20230922>`.
* The new :ref:`docs-code_reviews` document outlines the upstream Pigweed code
  review process.

Active SEEDs
============
Help shape the future of Pigweed! Please leave feedback on the following active RFCs (SEEDs):

* `SEED-0103: pw_protobuf Object Model <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/133971>`__
* `SEED-0105: Add nested tokens and tokenized args to pw_tokenizer and pw_log <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154190>`__
* `SEED-0106: Project Template <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155430>`__
* `SEED-0109: Communication Buffers <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168357>`__
* `SEED-0110: Memory Allocation Interfaces <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168772>`__
* `SEED-0111: Future of Pigweed build systems <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171695>`__
* `SEED-0112: Async Poll Model <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168337>`__

Modules
=======

pw function
-----------
* `Sign conversion fixes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171832>`__
  (issue `#301079199 <https://issues.pigweed.dev/issues/301079199>`__)

pw perf_test
------------
* `Sign conversion fixes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171832>`__
  (issue `#301079199 <https://issues.pigweed.dev/issues/301079199>`__)

pw_analog
---------
* `Migrate AnalogInput to Doxygen <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170511>`__
  (issue `#299147635 <https://issues.pigweed.dev/issues/299147635>`__)

pw_async
--------
The ``Run*()`` methods of ``FakeDispatcher`` now return a boolean that indicates
whether any tasks were invoked.

* `Return bool from FakeDispatcher Run*() methods <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170599>`__

pw_async_basic
--------------
``release()`` is now only called outside of locked contexts to prevent an
issue where the thread wakes up and then immediately goes back to sleep.
An unnecessary 5-second wakeup has been removed from ``BasicDispatcher``.

* `release outside of lock context <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171103>`__
* `Remove unnecessary 5-second wakeup <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171102>`__

pw_base64
---------
The new ``pw::base64::IsValidChar()`` method can help you determine if a
character is valid Base64.

* `Add base64 detokenizer handler <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/165010>`__

pw_bluetooth
------------
More :ref:`Emboss <module-pw_third_party_emboss>` definitions were added.

* `Add ReadLocalSupportedCommandsCommandCompleteEvent Emboss <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169877>`__
* `Add LEReadLocalSupportedFeaturesCommandCompleteEvent <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169931>`__
* `Add ReadBufferSizeCommandComplete Emboss definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169869>`__
* `Add ReadBdAddrCommandCompleteEvent Emboss definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170052>`__
* `Add ReadLocalVersionInfoCommandCompleteEvent Emboss def <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169951>`__
* `Add LELongTermKeyRequestSubevent Emboss definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169950>`__
* `Add UserPasskeyNotificationEvent Emboss definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169917>`__

pw_build
--------
* `Apply -Wextra-semi to C code as well as C++ <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172372>`__

pw_bytes
--------
The ``AlignDown()``, ``AlignUp()``, and ``Padding()`` methods of ``pw_kvs``
have moved to ``pw_bytes`` to enable ``pw_allocator`` to use them without
taking a dependency on ``pw_kvs``.

* `Move Align functions from pw_kvs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171831>`__

pw_checksum
-----------
* `Sign conversion fixes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171832>`__
  (issue `#301079199 <https://issues.pigweed.dev/issues/301079199>`__)

pw_chre
-------
The implementation of a module that will enable to work more seamlessly with
Android's `Context Hub Runtime Environment <https://source.android.com/docs/core/interaction/contexthub>`__
has begun.

* `Update bug numbers <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172330>`__
* `Minor fixes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171851>`__
  (issue `#301079509 <https://issues.pigweed.dev/issues/301079509>`__)
* `Fix build rules to use paramertized paths <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171850>`__
  (issue `#298474212 <https://issues.pigweed.dev/issues/298474212>`__)
* `Split out shared_platform <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170791>`__
* `Write our own version.cc <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170790>`__
  (issue `#300633363 <https://issues.pigweed.dev/issues/300633363>`__)
* `Add barebones CHRE <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162510>`__
  (issue `#294106526 <https://issues.pigweed.dev/issues/294106526>`__)

pw_console
----------
When invoking ``pw_console`` directly from Python, you can now provide arguments
through an ``argparse.Namespace`` instead of messing with ``sys.argv`` or forking
another process.

* `Allow injecting args via Python call <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172414>`__

pw_containers
-------------
`MemorySanitizer <https://github.com/google/sanitizers/wiki/MemorySanitizer>`__ has
been disabled in some of the ``InlineDeque`` implementation to prevent some false
positive detections of uninitialized memory reads.

* `Silence MSAN false positives <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171990>`__

pw_env_setup
------------
Work continues on making the Windows bootstrap process more robust.

* `Better highlight bootstrap failure <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172415>`__
* `Fix double bootstrap.bat failures on Windows <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172410>`__
  (issue `#300992566 <https://issues.pigweed.dev/issues/300992566>`__)
* `Enable overriding Clang CIPD version <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171838>`__
* `PyPI version bump to 0.0.15 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171836>`__
* `Add relative_pigweed_root to pigweed.json <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171218>`__
  (issue `#300632028 <https://issues.pigweed.dev/issues/300632028>`__)
* `Roll cipd to 0f08b927516 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170452>`__

pw_function
-----------
The documentation has been updated for accuracy.

* `Update config.h comments <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171250>`__
* `Add configurable Allocator default <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171130>`__
* `Update example to match guidelines for parameters <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170651>`__
* `Add Allocator injection <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170190>`__

pw_fuzzer
---------
Conditional logic around fuzzing support has been refactored to allow for
dedicated targets based on specific conditions and to make it clearer
exactly what configurations and dependencies are being used.

* `Refactor conditional GN targets <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169712>`__

pw_ide
------
* `Reformat json files <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172310>`__
* `Fix clangd path on Windows <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171099>`__
* `Move VSC extension into npm package dir <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170995>`__

pw_libc
-------
The initial implementation work continues.

* `Pull in 'abort' <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/138518>`__
* `Use .test convention <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171793>`__
* `Use underscore prefixed variables <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171792>`__
* `Add documentation for pw_libc_source_set <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171693>`__
* `Pull in 'gmtime' <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/137699>`__
* `Fix printf for newer llvm-libc commits <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170831>`__
* `Fix llvm-libc after internal assert changes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168830>`__

pw_log
------
The implementation work continues to enable an Android component to read logs
from a component running the ``pw_log_rpc`` service.

* `Update Android.bp to generate RPC header files <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169530>`__
  (issue `#298693458 <https://issues.pigweed.dev/issues/298693458>`__)

pw_log_string
-------------
* `Fix the default impl to handle zero length va args <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169975>`__

pw_package
----------
Mirrors are now being used for various third-party dependencies.

* `Use mirror for zephyrproject-rtos/zephyr <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170355>`__
  (issue `#278914999 <https://issues.pigweed.dev/issues/278914999>`__)
* `Use Pigweed mirror for google/emboss <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170356>`__
  (issue `#278914999 <https://issues.pigweed.dev/issues/278914999>`__)
* `Use mirror for raspberrypi/picotool <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170040>`__
  (issue `#278914999 <https://issues.pigweed.dev/issues/278914999>`__)

pw_polyfill
-----------
* `Increase __GNUC__ for __constinit <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171150>`__
  (issue `#300478321 <https://issues.pigweed.dev/issues/300478321>`__)

pw_presubmit
------------
A new JSON formatting check has been added. The missing newline check has been
made more robust.

* `Add JSON formatter <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171991>`__
* `Better handling of missing newlines <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172230>`__
  (issue `#301315329 <https://issues.pigweed.dev/issues/301315329>`__)
* `Expand Bazel parser to tests <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171890>`__
* `Remove now-unnecessary flag <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171670>`__
  (issue `#271299438 <https://issues.pigweed.dev/issues/271299438>`__)
* `Additional functions for handling gn args <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170594>`__
* `Include bazel_build in full program <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170056>`__

pw_protobuf
-----------
* `Fix "Casting..." heading level <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171030>`__

.. _docs-changelog-pw_rpc-20230922:

pw_rpc
------
``pw::rpc::SynchronousCall`` now supports the use of custom response message
classes that set field callbacks in their constructor. See
:ref:`module-pw_rpc-client-sync-call-wrappers`.

.. todo-check: disable

* `Refer to bug in TODO and fix format <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172453>`__
* `Support custom response messages in SynchronousCall <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170041>`__
  (issue `#299920227 <https://issues.pigweed.dev/issues/299920227>`__)
* `Add fuzz tests <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/143474>`__

.. todo-check: enable

pw_stream
---------
* `Add Windows socket support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172413>`__

pw_string
---------
* `Fix signed integer overflow <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171839>`__

pw_system
---------
* `Add arm_none_eabi_gcc_support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/158730>`__

pw_thread
---------
* `Fix small typo in docs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171750>`__

.. _docs-changelog-pw_tokenizer-20230922:

pw_tokenizer
------------
``pw::tokenizer::Detokenizer`` has new ``DetokenizeBase64Message()`` and
``DetokenizeBase64()`` methods for detokenizing Base64-encoded strings.
The new ``pw_tokenizer_EncodeInt()`` and ``pw_tokenizer_EncodeInt64()``
functions in the C API make it easier to manually encode tokenized messages
with integers from C.

* `C++ Base64 detokenization improvements <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171675>`__
* `Add base64 detokenizer handler <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/165010>`__
* `C functions for encoding arguments <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169976>`__

pw_toolchain
------------
``arm_gcc`` now supports Cortex-M33.

* `Add missing objcopy tool to bazel toolchains <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171697>`__
  (issue `#301004620 <https://issues.pigweed.dev/issues/301004620>`__)
* `Add cpu flags to asmopts as well <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171671>`__
* `Add cortex-m33 support to arm_gcc <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171237>`__

pw_toolchain_bazel
------------------
* `Support ar opts in pw_toolchain_features <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171673>`__
* `Add cortex-m7 constraint_value <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171071>`__
  (issue `#300467616 <https://issues.pigweed.dev/issues/300467616>`__)

.. _docs-changelog-pw_varint-20230922:

pw_varint
---------
The C encoding functions now have an output size argument, making them much
easier to use. There's a new macro for calculating the encoded size of an
integer in a C constant expression. Incremental versions of the encode and
decode functions have been exposed to support in-place encoding and decoding
with non-contiguous buffers.

* `C API updates <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170050>`__

pw_web
------
The ``ProgressStats`` and ``ProgressCallback`` types are now exported.
Styling and scrolling behavior in the log viewer has been improved.

* `Remove need for Buffer package in pw_hdlc <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172377>`__
* `Remove date-fns <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172371>`__
* `Export ProgressStats, ProgressCallback types <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171707>`__
* `Add back 'buffer' dependency <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171891>`__
* `NPM version bump to 0.0.13 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171110>`__
* `Improve scrolling behavior <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171290>`__
  (issue `#298097109 <https://issues.pigweed.dev/issues/298097109>`__)
* `Fix leading white spaces, scrollbar size, and filters in quotes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170811>`__
* `NPM version bump to 0.0.12 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170597>`__
* `Fix column sizing & toggling, update UI <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169591>`__
* `Replace Map() with object in proto collection <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170493>`__

pw_work_queue
-------------
* `Don't lock around work_notification_ <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170450>`__
* `Migrate API reference to Doxygen <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169830>`__
  (issue `#299147635 <https://issues.pigweed.dev/issues/299147635>`__)

Build
=====
* `Update Android.bp <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171510>`__

Bazel
-----
* `Add platform-printing aspect <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/122974>`__
* `Retire pigweed_config (part 2) <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170058>`__
  (issue `#291106264 <https://issues.pigweed.dev/issues/291106264>`__)
* `Retire pigweed_config (part 1) <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168721>`__
  (issue `#291106264 <https://issues.pigweed.dev/issues/291106264>`__)
* `Remove -Wno-private-header from copts <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170951>`__
  (issue `#240466562 <https://issues.pigweed.dev/issues/240466562>`__)
* `Remove bazelembedded dependency <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170650>`__
  (issue `#297239780 <https://issues.pigweed.dev/issues/297239780>`__)
* `Move cxxopts out of bazelrc <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170639>`__
  (issue `#269195628 <https://issues.pigweed.dev/issues/269195628>`__)
* `Use the same clang version as in GN <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170638>`__
* `Arm gcc configuration <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168192>`__
  (issue `#297239780 <https://issues.pigweed.dev/issues/297239780>`__)

Targets
=======
* `Fix pico_sdk elf2uf2 on Windows <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170770>`__
* `Add pw_strict_host_clang_debug_dynamic_allocation tc <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171674>`__

Docs
====
The new :ref:`docs-code_reviews` document outlines the upstream Pigweed code
review process.

* `Add Doxygen @endcode guidance <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172470>`__
* `Clean up remaining instances of code:: <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172050>`__
  (issue `#300317685 <https://issues.pigweed.dev/issues/300317685>`__)
* `Document code review process <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171774>`__
* `Add link to in-progress hardware targets <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171239>`__
* `Fix link title for pw_log <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170670>`__
* `Update changelog <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170055>`__
  (issue `#292247409 <https://issues.pigweed.dev/issues/292247409>`__)

SEEDs
=====
* `Update process document <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170390>`__
* (SEED-0104) `Display Support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/150793>`__
* (SEED-0109) `Make link externally accessible <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170043>`__
* (SEED-0110) `Claim SEED number <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170038>`__
* (SEED-0111) `Claim SEED number <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171672>`__
* (SEED-0112) `Claim SEED number <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168359>`__

Third party
===========
* `Add public configs for FuzzTest deps <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169711>`__

third_party/fuchsia
-------------------
* `Copybara import <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171010>`__
* `Update patch script and patch <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170890>`__
* `Update patch <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170794>`__
* `Support specifying the Fuchsia repo to use <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170170>`__

third_party/pico_sdk
--------------------
* `Selectively disable elf2uf2 warnings <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/171072>`__
  (issue `#300474559 <https://issues.pigweed.dev/issues/300474559>`__)
* `Fix multicore source filename <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170330>`__

Miscellaneous
=============
.. todo-check: disable

* `Use new TODO style <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/170730>`__
* `Add toolchain team members <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172170>`__
* `Fix double bootstrap.bat failures on Windows" <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/172410>`__
  (issue `#300992566 <https://issues.pigweed.dev/issues/300992566>`__)

.. todo-check: enable

-----------
Sep 8, 2023
-----------
Highlights (Aug 25, 2023 to Sep 8, 2023):

* SEED :ref:`seed-0107` has been approved! Pigweed will adopt a new sockets API as
  its primary networking abstraction. The sockets API will be backed by a new,
  lightweight embedded-focused network protocol stack inspired by TCP/IP.
* SEED :ref:`seed-0108` has also been approved! Coming soon, the new ``pw_emu``
  module will make it easier to work with emulators.

Active SEEDs
============
Help shape the future of Pigweed! Please leave feedback on the following active RFCs (SEEDs):

* `SEED-0103: pw_protobuf Object Model <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/133971>`__
* `SEED-0104: display support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/150793>`__
* `SEED-0105: Add nested tokens and tokenized args to pw_tokenizer and pw_log <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154190>`__
* `SEED-0106: Project Template <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155430>`__
* `SEED-0109: Communication Buffers <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168357>`__

Modules
=======

pw_assert
---------
We fixed circular dependencies in Bazel.

* `Remove placeholder target <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168844>`__
* `Fix Bazel circular deps <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160794>`__
  (issue `#234877642 <https://issues.pigweed.dev/issues/234877642>`__)
* `Introduce pw_assert_backend_impl <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168774>`__
  (issue `#234877642 <https://issues.pigweed.dev/issues/234877642>`__)

pw_bluetooth
------------
We added :ref:`Emboss <module-pw_third_party_emboss>` definitions.

* `Add SimplePairingCompleteEvent Emboss definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169916>`__
* `Add UserPasskeyRequestEvent Emboss definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169912>`__
* `Add UserConfirmationRequestEvent Emboss definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169871>`__
* `Use hci.LinkKey in LinkKeyNotificationEvent.link_key <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168858>`__
* `Add IoCapabilityResponseEvent Emboss definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168354>`__
* `Add IoCapabilityRequestEvent Emboss definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168353>`__
* `Add EncryptionKeyRefreshCompleteEvent Emboss definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168331>`__
* `Add ExtendedInquiryResultEvent Emboss definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168330>`__

pw_build
--------
* `Force watch and default recipe names <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169911>`__

pw_build_mcuxpresso
-------------------
* `Output formatted bazel target <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169740>`__

pw_cpu_exception
----------------
We added Bazel support.

* `bazel build support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169733>`__
  (issue `#242183021 <https://issues.pigweed.dev/issues/242183021>`__)

pw_crypto
---------
The complete ``pw_crypto`` API reference is now documented on :ref:`module-pw_crypto`.

* `Add API reference <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169572>`__
  (issue `#299147635 <https://issues.pigweed.dev/issues/299147635>`__)

pw_env_setup
------------
Banners should not print correctly on Windows.

* `Add i2c protos to python deps <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169231>`__
* `Fix banner printing on Windows <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169172>`__
  (issue `#289008307 <https://issues.pigweed.dev/issues/289008307>`__)

pw_file
-------
* `Add pw_file python package <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168831>`__

pw_function
-----------
The :cpp:func:`pw::bind_member()` template is now exposed in the public API.
``bind_member()`` is useful for binding the ``this`` argument of a callable.
We added a section to the docs explaining :ref:`why pw::Function is not a
literal <module-pw_function-non-literal>`.

* `Explain non-literal design rationale <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168777>`__
* `Expose \`bind_member\` <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169123>`__

pw_fuzzer
---------
We refactored ``pw_fuzzer`` logic to be more robust and expanded the
:ref:`module-pw_fuzzer-guides-reproducing_oss_fuzz_bugs` doc.

* `Refactor OSS-Fuzz support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167348>`__
  (issue `#56955 <https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=56955>`__)

pw_i2c
------
* `Use new k{FieldName}MaxSize constants to get buffer size <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168913>`__

pw_kvs
------
We are discouraging the use of the shorter macros because they collide with
Abseil's logging API.

* `Remove usage of pw_log/shorter.h API <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169920>`__
  (issue `#299520256 <https://issues.pigweed.dev/issues/299520256>`__)

pw_libc
-------
``snprintf()`` support was added.

* `Import LLVM libc's snprintf <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/137735>`__

pw_log_string
-------------
We added more detail to :ref:`module-pw_log_string`.

* `Fix the default impl to handle zero length va args <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169975>`__
* `Provide more detail in the getting started docs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168934>`__
  (issue `#298124226 <https://issues.pigweed.dev/issues/298124226>`__)

pw_log_zephyr
-------------
It's now possible to define ``pw_log_tokenized_HandleLog()`` outside of Pigweed
so that Zephyr projects have more flexibility around how they capture tokenized
logs.

* `Split tokenize handler into its own config <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168612>`__

pw_package
----------
* `Handle failed cipd acl checks <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168530>`__

pw_persistent_ram
-----------------
* `Add persistent_buffer flat_file_system_entry <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168832>`__

pw_presubmit
------------
We added a reStructuredText formatter.

* `Make builds_from_previous_iteration ints <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169721>`__
  (issue `#299336222 <https://issues.pigweed.dev/issues/299336222>`__)
* `Move colorize_diff to tools <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168839>`__
* `RST formatting <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168541>`__

pw_protobuf
-----------
``max_size`` and ``max_count`` are now exposed in generated headers.
The new ``proto_message_field_props()`` helper function makes it easier to
iterate through a messages fields and properties.

* `Expose max_size, max_count in generated header file <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168973>`__
  (issue `#297364973 <https://issues.pigweed.dev/issues/297364973>`__)
* `Introduce proto_message_field_props() <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168972>`__
* `Change PROTO_FIELD_PROPERTIES to a dict of classes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168971>`__
* `Rename 'node' to 'message' in forward_declare() <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168970>`__
* `Simplify unnecessary Tuple return type <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168910>`__

pw_random
---------
We're now auto-generating the ``XorShiftStarRng64`` API reference via Doxygen.

* `Doxygenify xor_shift.h <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/164510>`__

pw_rpc
------
The new ``request_completion()`` method in Python enables you to send a
completion packet for server streaming calls.

* `Add request_completion to ServerStreamingCall python API <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168439>`__

pw_spi
------
* `Fix Responder.SetCompletionHandler() signature <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169130>`__

pw_symbolizer
-------------
The ``LlvmSymbolizer`` Python class has a new ``close()`` method to
deterministically close the background process.

* `LlvmSymbolizer tool improvement <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168863>`__

pw_sync
-------
We added :ref:`module-pw_sync-genericbasiclockable`.

* `Add GenericBasicLockable <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/165930>`__

pw_system
---------
``pw_system`` now supports different channels for primary and logging RPC.

* `Multi-channel configuration <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167158>`__
  (issue `#297076185 <https://issues.pigweed.dev/issues/297076185>`__)

pw_thread_freertos
------------------
* `Add missing dep to library <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169239>`__

pw_tokenizer
------------
We added :c:macro:`PW_TOKENIZE_FORMAT_STRING_ANY_ARG_COUNT` and
:c:macro:`PW_TOKENIZER_REPLACE_FORMAT_STRING`. We refactored the docs
so that you don't have to jump around the docs as much when learning about
key topics like tokenization and token databases. Database loads now happen
in a separate thread to avoid blocking the main thread. Change detection for
directory databases now works more as expected. The config API is now exposed
in the API reference.

* `Remove some unused deps <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169573>`__
* `Simplify implementing a custom tokenization macro <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169121>`__
* `Refactor the docs to be task-focused <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169124>`__
* `Reload database in dedicated thread <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168866>`__
* `Combine duplicated docs sections <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168865>`__
* `Support change detection for directory dbs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168630>`__
* `Move config value check to .cc file <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168615>`__

pw_unit_test
------------
We added ``testing::Test::HasFailure()``, ``FRIEND_TEST``, and ``<<`` messages
to improve gTest compatibility.

* `Add testing::Test::HasFailure() <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168810>`__
* `Add FRIEND_TEST <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169270>`__
* `Allow <<-style messages in test expectations <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168860>`__

pw_varint
---------
``pw_varint`` now has a C-only API.

* `Add C-only implementation; cleanup <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169122>`__

pw_web
------
Logs can now be downloaded as plaintext.

* `Fix TypeScript errors in Device files <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169930>`__
* `Json Log Source example <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169176>`__
* `Enable downloading logs as plain text <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168130>`__
* `Fix UI/state bugs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167911>`__
* `NPM version bump to 0.0.11 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168591>`__
* `Add basic bundling tests for log viewer bundle <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168539>`__

Build
=====

Bazel
-----
* `Fix alwayslink support in MacOS host_clang <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168614>`__
  (issue `#297413805 <https://issues.pigweed.dev/issues/297413805>`__)
* `Fix lint issues after roll <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169611>`__

Docs
====
* `Fix broken links <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169579>`__
  (issue `#299181944 <https://issues.pigweed.dev/issues/299181944>`__)
* `Recommend enabling long file paths on Windows <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169578>`__
* `Update Windows command for git hook <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168592>`__
* `Fix main content scrolling <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168555>`__
  (issue `#297384789 <https://issues.pigweed.dev/issues/297384789>`__)
* `Update changelog <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168540>`__
  (issue `#292247409 <https://issues.pigweed.dev/issues/292247409>`__)
* `Use code-block:: instead of code:: everywhere <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168617>`__
* `Add function signature line breaks <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168554>`__
* `Cleanup indentation <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168537>`__

SEEDs
=====
* `SEED-0108: Emulators Frontend <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/158190>`__

Third party
===========
* `Add public configs for FuzzTest deps <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169711>`__
* `Reconfigure deps & add cflags to config <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/152691>`__

Miscellaneous
=============
* `Fix formatting with new clang version <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/169078>`__

mimxrt595_evk_freertos
----------------------
* `Use config_assert helper <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160378>`__

------------
Aug 25, 2023
------------
Highlights (Aug 11, 2023 to Aug 25, 2023):

* ``pw_tokenizer`` now has Rust support.
* The ``pw_web`` log viewer now has advanced filtering and a jump-to-bottom
  button.
* The ``run_tests()`` method of ``pw_unit_test`` now returns a new
  ``TestRecord`` dataclass which provides more detailed information
  about the test run.
* A new Ambiq Apollo4 target that uses the Ambiq Suite SDK and FreeRTOS
  has been added.

Active SEEDs
============
Help shape the future of Pigweed! Please leave feedback on the following active RFCs (SEEDs):

* `SEED-0103: pw_protobuf Object Model <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/133971>`__
* `SEED-0104: display support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/150793>`__
* `SEED-0105: Add nested tokens and tokenized args to pw_tokenizer and pw_log <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154190>`__
* `SEED-0106: Project Template <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155430>`__
* `SEED-0108: Emulators Frontend <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/158190>`__

Modules
=======

pw_bloat
--------
* `Fix typo in method name <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/166832>`__

pw_bluetooth
------------
The :ref:`module-pw_third_party_emboss` files were refactored.

* `Add SynchronousConnectionCompleteEvent Emboss definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167862>`__
* `Add all Emboss headers/deps to emboss_test & fix errors <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168355>`__
* `Add InquiryResultWithRssiEvent Emboss definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167859>`__
* `Add DataBufferOverflowEvent Emboss definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167858>`__
* `Add LinkKeyNotificationEvent Emboss definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167855>`__
* `Add LinkKeyRequestEvent emboss definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167349>`__
* `Remove unused hci emboss files <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167090>`__
* `Add RoleChangeEvent emboss definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167230>`__
* `Add missing test dependency <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167130>`__
* `Add new hci subset files <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/166730>`__

pw_build
--------
The ``pw_build`` docs were split up so that each build system has its own page
now. The new ``output_logs`` flag enables you to not output logs for ``pw_python_venv``.

* `Handle read-only files when deleting venvs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167863>`__
* `Split build system docs into separate pages <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/165071>`__
* `Use pw_toolchain_clang_tools <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167671>`__
* `Add missing pw_linker_script flag <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167632>`__
  (issue `#296928739 <https://issues.pigweed.dev/issues/296928739>`__)
* `Fix output_logs_ unused warning <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/166991>`__
  (issue `#295524695 <https://issues.pigweed.dev/issues/295524695>`__)
* `Don't include compile cmds when preprocessing ldscripts <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/166490>`__
* `Add pw_python_venv.output_logs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/165330>`__
  (issue `#295524695 <https://issues.pigweed.dev/issues/295524695>`__)
* `Increase size of test linker script memory region <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/164823>`__
* `Add integration test metadata <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154553>`__

pw_cli
------
* `Default change pw_protobuf default <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/126806>`__
  (issue `#266298474 <https://issues.pigweed.dev/issues/266298474>`__)

pw_console
----------
* `Update web viewer to use pigweedjs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162995>`__

pw_containers
-------------
* `Silence MSAN false positive in pw::Vector <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167111>`__

pw_docgen
---------
Docs builds should be faster now because Sphinx has been configured to use
all available cores.

* `Remove top nav bar <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168446>`__
* `Parallelize Sphinx <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/164738>`__

pw_env_setup
------------
Sphinx was updated from v5.3.0 to v7.1.2. We switched back to the upstream Furo
theme and updated to v2023.8.19. The content of ``pigweed_environment.gni`` now
gets logged. There was an update to ensure that ``arm-none-eabi-gdb`` errors
propagate correctly. There is now a way to override Bazel build files for CIPD
repos.

* `Upgrade sphinx and dependencies for docs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168431>`__
* `Upgrade sphinx-design <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168339>`__
* `Copy pigweed_environment.gni to logs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167850>`__
* `arm-gdb: propagate errors <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/165411>`__
* `arm-gdb: exclude %VIRTUAL_ENV%\Scripts from search paths <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/164370>`__
* `Add ability to override bazel BUILD file for CIPD repos <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/165530>`__

pw_function
-----------
* `Rename template parameter <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168334>`__

pw_fuzzer
---------
* `Add test metadata <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154555>`__

pw_hdlc
-------
A new ``close()`` method was added to ``HdlcRpcClient`` to signal to the thread
to stop.

* `Use explicit logger name <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/166591>`__
* `Mitigate errors on Python background thread <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162712>`__
  (issue `#293595266 <https://issues.pigweed.dev/issues/293595266>`__)

pw_ide
------
A new ``--install-editable`` flag was added to install Pigweed Python modules
in editable mode so that code changes are instantly realized.

* `Add cmd to install Py packages as editable <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163572>`__
* `Make VSC extension run on older versions <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167054>`__

pw_perf_test
------------
* `Add test metadata <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154554>`__

pw_presubmit
------------
``pw_presubmit`` now has an ESLint check for linting and a Prettier check for
formatting JavaScript and TypeScript files.

* `Add msan to OTHER_CHECKS <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168325>`__
  (issue `#234876100 <https://issues.pigweed.dev/issues/234876100>`__)
* `Upstream constraint file output fix <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/166270>`__
* `JavaScript and TypeScript lint check <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/165410>`__
* `Apply TypeScript formatting <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/164825>`__
* `Use prettier for JS and TS files <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/165390>`__

pw_rpc
------
A ``request_completion()`` method was added to the ``ServerStreamingCall``
Python API. A bug was fixed related to encoding failures when dynamic buffers
are enabled.

* `Add request_completion to ServerStreamingCall python API <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168439>`__
* `Various small enhancements <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167162>`__
* `Remove deprecated method from Service <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/165510>`__
* `Prevent encoding failure when dynamic buffer enabled <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/166833>`__
  (issue `#269633514 <https://issues.pigweed.dev/issues/269633514>`__)

pw_rpc_transport
----------------
* `Add simple_framing Soong rule <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/165350>`__

pw_rust
-------
* `Update rules_rust to 0.26.0 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/166831>`__

pw_stm32cube_build
------------------
* `Windows path fixes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167865>`__

pw_stream
---------
Error codes were updated to be more accurate and descriptive.

* `Use more appropriate error codes for Cursor <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/164592>`__

pw_stream_uart_linux
--------------------
Common baud rates such as ``9600``, ``19200``, and so on are now supported.

* `Add support for baud rates other than 115200 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/165070>`__

pw_sync
-------
Tests were added to make sure that ``pw::sync::Borrowable`` works with lock
annotations.

* `Test Borrowable with Mutex, TimedMutex, and InterruptSpinLock <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/153575>`__
  (issue `#261078330 <https://issues.pigweed.dev/issues/261078330>`__)

pw_system
---------
The ``pw_system.device.Device`` Python class can now be used as a
`context manager <https://realpython.com/python-with-statement/>`_.

* `Make pw_system.device.Device a context manager <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163410>`__

pw_tokenizer
------------
``pw_tokenizer`` now has Rust support. The ``pw_tokenizer`` C++ config API
is now documented at :ref:`module-pw_tokenizer-api-configuration` and
the C++ token database API is now documented at
:ref:`module-pw_tokenizer-api-token-databases`. When creating a token
database, parent directories are now automatically created if they don't
already exist. ``PrefixedMessageDecoder`` has been renamed to
``NestedMessageDecoder``.

* `Move config value check to .cc file <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168615>`__
* `Create parent directory as needed <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168510>`__
* `Rework pw_tokenizer.detokenize.PrefixedMessageDecoder <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167150>`__
* `Minor binary database improvements <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167053>`__
* `Update binary DB docs and convert to Doxygen <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163570>`__
* `Deprecate tokenizer buffer size config <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163257>`__
* `Fix instance of -Wconstant-logical-operand <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/166731>`__
* `Add Rust support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/145389>`__

pw_toolchain
------------
A new Linux host toolchain built using ``pw_toolchain_bazel`` has been
started. CIPD-provided Rust toolchains are now being used.

* `Link against system libraries using libs not ldflags <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/151050>`__
* `Use %package% for cxx_builtin_include_directories <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168340>`__
* `Extend documentation for tool prefixes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167633>`__
* `Add Linux host toolchain <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/164824>`__
  (issue `#269204725 <https://issues.pigweed.dev/issues/269204725>`__)
* `Use CIPD provided Rust toolchains <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/166852>`__
* `Switch macOS to use builtin_sysroot <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/165414>`__
* `Add cmake helpers for getting clang compile+link flags <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163811>`__

pw_unit_test
------------
``run_tests()`` now returns the new ``TestRecord`` dataclass which provides
more detailed information about the test run. ``SetUpTestSuit()`` and
``TearDownTestSuite()`` were added to improve GoogleTest compatibility.

* `Add TestRecord of Test Results <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/166273>`__
* `Reset static value before running tests <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/166590>`__
  (issue `#296157327 <https://issues.pigweed.dev/issues/296157327>`__)
* `Add per-fixture setup/teardown <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/165210>`__

pw_web
------
Log viewers are now drawn every 100 milliseconds at most to prevent crashes
when many logs arrive simultaneously. The log viewer now has a jump-to-bottom
button. Advanced filtering has been added.

* `NPM version bump to 0.0.11 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168591>`__
* `Add basic bundling tests for log viewer bundle <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168539>`__
* `Limit LogViewer redraws to 100ms <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167852>`__
* `Add jump to bottom button, fix UI bugs and fix state bugs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/164272>`__
* `Implement advanced filtering <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162070>`__
* `Remove object-path dependency from Device API <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/165013>`__
* `Log viewer toolbar button toggle style <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/165412>`__
* `Log-viewer line wrap toggle <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/164010>`__

Targets
=======

targets
-------
A new Ambiq Apollo4 target that uses the Ambiq Suite SDK and FreeRTOS
has been added.

* `Ambiq Apollo4 support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/129490>`__

Language support
================

Python
------
* `Upgrade mypy to 1.5.0 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/166272>`__
* `Upgrade pylint to 2.17.5 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/166271>`__

Docs
====
Doxygen-generated function signatures now present each argument on a separate
line. Tabbed content looks visually different than before.

* `Use code-block:: instead of code:: everywhere <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168617>`__
* `Add function signature line breaks <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168554>`__
* `Cleanup indentation <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168537>`__
* `Remove unused myst-parser <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168392>`__
* `Use sphinx-design for tabbed content <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168341>`__
* `Update changelog <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/164810>`__

SEEDs
=====
:ref:`SEED-0107 (Pigweed Communications) <seed-0107>` was accepted and
SEED-0109 (Communication Buffers) was started.

* `Update protobuf SEED title in index <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/166470>`__
* `Update status to Accepted <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167770>`__
* `Pigweed communications <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157090>`__
* `Claim SEED number <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168358>`__

Miscellaneous
=============

Build
-----
* `Make it possible to run MSAN in GN <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/167112>`__

soong
-----
* `Remove host/vendor properties from defaults <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/165270>`__

------------
Aug 11, 2023
------------
Highlights (Jul 27, 2023 to Aug 11, 2023):

* We're prototyping a Pigweed extension for VS Code. Learn more at
  :ref:`module-pw_ide-guide-vscode`.
* We added ``pw_toolchain_bazel``, a new LLVM toolchain for building with
  Bazel on macOS.
* We are working on many docs improvements in parallel: auto-generating ``rustdocs``
  for modules that support Rust
  (`example <https://pigweed.dev/rustdoc/pw_varint/>`_), refactoring the
  :ref:`module-pw_tokenizer` docs, migrating API references to Doxygen,
  fixing `longstanding docs site UI issues <https://issues.pigweed.dev/issues/292273650>`_,
  and more.

Active SEEDs
============
Help shape the future of Pigweed! Please leave feedback on the following active RFCs (SEEDs):

* `SEED-0103: pw_protobuf Object Model <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/133971>`__
* `SEED-0104: display support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/150793>`__
* `SEED-0105: Add nested tokens and tokenized args to pw_tokenizer and pw_log <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154190>`__
* `SEED-0106: Project Template <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155430>`__
* `SEED-0107: Pigweed communications <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157090>`__
* `SEED-0108: Emulators Frontend <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/158190>`__

Modules
=======

pw_alignment
------------
* `Fix typos <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163250>`__

pw_analog
---------
Long-term, all of our API references will be generated from header comments via
Doxygen. Short-term, we are starting to show header files directly within the
docs as a stopgap solution for helping Pigweed users get a sense of each
module's API. See :ref:`module-pw_analog` for an example.

* `Include header files as stopgap API reference <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/161491>`__
  (issue `#293895312 <https://issues.pigweed.dev/issues/293895312>`__)

pw_base64
---------
We finished migrating the ``pw_random`` API reference to Doxygen.

* `Finish Doxygenifying the API reference <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162911>`__
* `Doxygenify the Encode() functions <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156532>`__

pw_boot_cortex_m
----------------
* `Allow explict target name <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159790>`__

pw_build
--------
We added a ``log_build_steps`` option to ``ProjectBuilder`` that enables you
to log all build step lines to your screen and logfiles.

* `Handle ProcessLookupError exceptions <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163710>`__
* `ProjectBuilder log build steps option <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162931>`__
* `Fix progress bar clear <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160791>`__

pw_cli
------
We polished tab completion support.

* `Zsh shell completion autoload <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160796>`__
* `Make pw_cli tab completion reusable <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160379>`__

pw_console
----------
We made copy-to-clipboard functionality more robust when running ``pw_console``
over SSH.

* `Set clipboard fallback methods <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/150238>`__

pw_containers
-------------
We updated :cpp:class:`filteredview` constructors and migrated the
``FilteredView`` API reference to Doxygen.

* `Doxygenify pw::containers::FilteredView <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160373>`__
* `Support copying the FilteredView predicate <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160372>`__

pw_docgen
---------
At the top of pages like :ref:`module-pw_tokenizer` there is a UI widget that
provides information about the module. Previously, this UI widget had links
to all the module's docs. This is no longer needed now that the site nav
automatically scrolls to the page you're on, which allows you to see the
module's other docs.

* `Remove the navbar from the module docs header widget <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162991>`__
  (issue `#292273650 <https://issues.pigweed.dev/issues/292273650>`__)

pw_env_setup
------------
We made Python setup more flexible.

* `Add clang_next.json <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163810>`__
  (issue `#295020927 <https://issues.pigweed.dev/issues/295020927>`__)
* `Pip installs from CIPD <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162093>`__
* `Include Python packages from CIPD <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162073>`__
* `Remove unused pep517 package <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162072>`__
* `Use more available Python 3.9 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/161492>`__
  (issue `#292278707 <https://issues.pigweed.dev/issues/292278707>`__)
* `Update Bazel to 2@6.3.0.6 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/161010>`__

pw_ide
------
We are prototyping a ``pw_ide`` extension for VS Code.

* `Restore stable clangd settings link <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/164011>`__
* `Add command to install prototype extension <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162412>`__
* `Prototype VS Code extension <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/151653>`__

pw_interrupt
------------
We added a backend for Xtensa processors.

* `Add backend for xtensa processors <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160031>`__
* `Tidy up target compatibility <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160650>`__
  (issue `#272090220 <https://issues.pigweed.dev/issues/272090220>`__)

pw_log_zephyr
-------------
We encoded tokenized messages to ``pw::InlineString`` so that the output is
always null-terminated.

* `Fix null termination of Base64 messages <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163650>`__

pw_presubmit
------------
We increased
`LUCI <https://chromium.googlesource.com/infra/infra/+/main/doc/users/services/about_luci.md>`_
support and updated the ``#pragma once`` check to look for matching ``#ifndef``
and ``#define`` lines.

* `Fix overeager format_code matches <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162611>`__
* `Exclude vsix files from copyright <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163011>`__
* `Clarify unicode errors <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162993>`__
* `Upload coverage json to zoss <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162090>`__
  (issue `#279161371 <https://issues.pigweed.dev/issues/279161371>`__)
* `Add to context tests <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162311>`__
* `Add patchset to LuciTrigger <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162310>`__
* `Add helpers to LuciContext <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162091>`__
* `Update Python vendor wheel dir <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/161514>`__
* `Add summaries to guard checks <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/161391>`__
  (issue `#287529705 <https://issues.pigweed.dev/issues/287529705>`__)
* `Copy Python packages <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/161490>`__
* `Add ifndef/define check <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/152173>`__
  (issue `#287529705 <https://issues.pigweed.dev/issues/287529705>`__)

pw_protobuf_compiler
--------------------
We continued work to ensure that the Python environment in Bazel is hermetic.

* `Use hermetic protoc <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162913>`__
  (issue `#294284927 <https://issues.pigweed.dev/issues/294284927>`__)
* `Move reference to python interpreter <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162932>`__
  (issue `#294414535 <https://issues.pigweed.dev/issues/294414535>`__)
* `Make nanopb hermetic <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162313>`__
  (issue `#293792686 <https://issues.pigweed.dev/issues/293792686>`__)

pw_python
---------
We fixed bugs related to ``requirements.txt`` files not getting found.

* `setup.sh requirements arg fix path <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/164430>`__
* `setup.sh arg spaces bug <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163510>`__

pw_random
---------
We continued migrating the ``pw_random`` API reference to Doxygen.

* `Doxygenify random.h <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163730>`__

pw_rpc
------
We made the Java client more robust.

* `Java client backwards compatibility <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/164515>`__
* `Avoid reflection in Java client <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162930>`__
  (issue `#293361955 <https://issues.pigweed.dev/issues/293361955>`__)
* `Use hermetic protoc <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162913>`__
  (issue `#294284927 <https://issues.pigweed.dev/issues/294284927>`__)
* `Improve Java client error message for missing parser() method <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159471>`__

pw_spi
------
We continued work on implementing a SPI responder interface.

* `Responder interface definition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159230>`__

pw_status
---------
We fixed the nesting on a documentation section.

* `Promote Zephyr heading to h2 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160730>`__

pw_stream
---------
We added ``remaining()``, ``len()``, and ``position()`` methods to the
``Cursor`` wrapping in Rust.

* `Add infalible methods to Rust Cursor <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/164271>`__

pw_stream_shmem_mcuxpresso
--------------------------
We added the :ref:`module-pw_stream_shmem_mcuxpresso` backend for ``pw_stream``.

* `Add shared memory stream for NXP MCU cores <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160831>`__
  (issue `#294406620 <https://issues.pigweed.dev/issues/294406620>`__)

pw_sync_freertos
----------------
* `Fix ODR violation in tests <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160795>`__

pw_thread
---------
* `Fix test_thread_context typo and presubmit <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162770>`__

pw_tokenizer
------------
We added support for unaligned token databases and continued the
:ref:`seed-0102` update of the ``pw_tokenizer`` docs.

* `Separate API reference and how-to guide content <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163256>`__
* `Polish the sales pitch <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163571>`__
* `Support unaligned databases <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163333>`__
* `Move the basic overview into getting started <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163253>`__
* `Move the case study to guides.rst <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163255>`__
* `Restore info that get lost during the SEED-0102 migration <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163330>`__
* `Use the same tagline on every doc <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163332>`__
* `Replace savings table with flowchart <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/158893>`__
* `Ignore string nonliteral warnings <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162092>`__

pw_toolchain
------------
We fixed a regression that made it harder to use Pigweed in an environment where
``pw_env_setup`` has not been run and fixed a bug related to incorrect Clang linking.

* `Optionally depend on pw_env_setup_CIPD_PIGWEED <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163790>`__
  (issue `#294886611 <https://issues.pigweed.dev/issues/294886611>`__)
* `Prefer start-group over whole-archive <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/150610>`__
  (issue `#285357895 <https://issues.pigweed.dev/issues/285357895>`__)

pw_toolchain_bazel
------------------
We added a an LLVM toolchain for building with Bazel on macOS.

* `LLVM toolchain for macOS Bazel build <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157634>`__
  (issue `#291795899 <https://issues.pigweed.dev/issues/291795899>`__)

pw_trace_tokenized
------------------
We made tracing more robust.

* `Replace trace callback singletons with dep injection <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156912>`__

pw_transfer
-----------
We made integration tests more robust.

* `Fix use-after-destroy in integration test client <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163252>`__
  (issue `#294101325 <https://issues.pigweed.dev/issues/294101325>`__)
* `Fix legacy binary path <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162914>`__
  (issue `#294284927 <https://issues.pigweed.dev/issues/294284927>`__)
* `Mark linux-only Bazel tests <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162094>`__
  (issue `#294101325 <https://issues.pigweed.dev/issues/294101325>`__)

pw_web
------
We added support for storing user preferences in ``localStorage``.

* `Fix TypeScript warnings in web_serial_transport.ts <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/164591>`__
* `Add state for view number, search string, and columns visible <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/161390>`__
* `Fix TypeScript warnings in transfer.ts <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162411>`__
* `Fix TypeScript warnings <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162095>`__
* `Remove dependency on 'crc' and 'buffer' NPM packages <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160830>`__

Build
=====
We made the Bazel system more hermetic and fixed an error related to not
finding the Java runtime.

* `Do not allow PATH leakage into Bazel build <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162610>`__
  (issue `#294284927 <https://issues.pigweed.dev/issues/294284927>`__)
* `Use remote Java runtime for Bazel build <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160793>`__
  (issue `#291791485 <https://issues.pigweed.dev/issues/291791485>`__)

Docs
====
We created a new doc that explains how to improve Pigweed
support in various IDEs. We standardized how we present call-to-action buttons
on module homepages. See :ref:`module-pw_tokenizer` for an example. We fixed a
longstanding UI issue around the site nav not scrolling to the page that you're
currently on.

* `Add call-to-action buttons <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163331>`__
* `Update module items in site nav <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163251>`__
* `Add editor support doc <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/110261>`__
* `Delay nav scrolling to fix main content scrolling <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162990>`__
  (issue `#292273650 <https://issues.pigweed.dev/issues/292273650>`__)
* `Suggest editor configuration <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162710>`__
* `Scroll to the current page in the site nav <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162410>`__
  (issue `#292273650 <https://issues.pigweed.dev/issues/292273650>`__)
* `Add changelog <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160170>`__
  (issue `#292247409 <https://issues.pigweed.dev/issues/292247409>`__)

SEEDs
=====
We created a UI widget to standardize how we present SEED status information.
See the start of :ref:`seed-0102` for an example.

* `Create Sphinx directive for metadata <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/161517>`__

Third party
===========

third_party/mbedtls
-------------------
* `3.3.0 compatibility <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160790>`__
  (issue `#293612945 <https://issues.pigweed.dev/issues/293612945>`__)

Miscellaneous
=============

OWNERS
------
* `Add kayce@ <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/163254>`__

------------
Jul 28, 2023
------------
Highlights (Jul 13, 2023 to Jul 28, 2023):

* `SEED-0107: Pigweed Communications <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157090>`__,
  a proposal for an embedded-focused network protocol stack, is under
  discussion. Please review and provide your input!
* ``pw_cli`` now supports tab completion!
* A new UART Linux backend for ``pw_stream`` was added (``pw_stream_uart_linux``).

Active SEEDs
============
* `SEED-0103: pw_protobuf Object Model <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/133971>`__
* `SEED-0104: display support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/150793>`__
* `SEED-0105: Add nested tokens and tokenized args to pw_tokenizer and pw_log <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154190>`__
* `SEED-0106: Project Template <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155430>`__
* `SEED-0107: Pigweed communications <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157090>`__
* `SEED-0108: Emulators Frontend <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/158190>`__

Modules
=======

pw_allocator
------------
We started migrating the ``pw_allocator`` API reference to Doxygen.

* `Doxygenify the freelist chunk methods <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155395>`__

pw_async
--------
We increased Bazel support.

* `Fill in bazel build rules <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156911>`__

pw_async_basic
--------------
We reduced logging noisiness.

* `Remove debug logging <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/158193>`__

pw_base64
---------
We continued migrating the ``pw_base64`` API reference to Doxygen.

* `Doxygenify MaxDecodedSize() <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157091>`__

pw_bloat
--------
We improved the performance of label creation. One benchmark moved from 120
seconds to 0.02 seconds!

* `Cache and optimize label production <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159474>`__

pw_bluetooth
------------
Support for 3 events was added.

* `Add 3 Event packets & format hci.emb <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157663>`__

pw_build
--------
* `Fix progress bar clear <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160791>`__
* `Upstream build script fixes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159473>`__
* `Add pw_test_info <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154551>`__
* `Upstream build script & presubmit runner <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/137130>`__
* `pw_watch: Redraw interval and bazel steps <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159490>`__
* `Avoid extra newlines for docs in generate_3p_gn <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/150233>`__
* `pip install GN args <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155270>`__
  (issue `#240701682 <https://issues.pigweed.dev/issues/240701682>`__)
* `pw_python_venv generate_hashes option <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157630>`__
  (issue `#292098416 <https://issues.pigweed.dev/issues/292098416>`__)

pw_build_mcuxpresso
-------------------
Some H3 elements in the ``pw_build_mcuxpresso`` docs were being incorrectly
rendered as H2.

* `Fix doc headings <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155570>`__

pw_chrono_freertos
------------------
We investigated what appeared to be a race condition but turned out to be an
unreliable FreeRTOS variable.

* `Update SystemTimer comments <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159231>`__
  (issue `#291346908 <https://issues.pigweed.dev/issues/291346908>`__)
* `Remove false callback precondition <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156091>`__
  (issue `#291346908 <https://issues.pigweed.dev/issues/291346908>`__)

pw_cli
------
``pw_cli`` now supports tab completion!

* `Zsh shell completion autoload <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160796>`__
* `Make pw_cli tab completion reusable <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160379>`__
* `Print tab completions for pw commands <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160032>`__
* `Fix logging msec timestamp format <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159930>`__

pw_console
----------
Communication errors are now handled more gracefully.

* `Detect comms errors in Python <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155814>`__

pw_containers
-------------
The flat map implementation and docs have been improved.

* `Doxygenify pw::containers::FilteredView <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160373>`__
* `Support copying the FilteredView predicate <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160372>`__
* `Improve FlatMap algorithm and filtered_view support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156652>`__
* `Improve FlatMap doc example <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156651>`__
* `Update FlatMap doc example so it compiles <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156650>`__

pw_digital_io
-------------
We continued migrating the API reference to Doxygen. An RPC service was added.

* `Doxygenify the interrupt handler methods <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154193>`__
* `Doxygenify Enable() and Disable() <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155817>`__
* `Add digital_io rpc service <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154974>`__

pw_digital_io_mcuxpresso
------------------------
We continued migrating the API reference to Doxygen. Support for a new RPC
service was added.

* `Remove unneeded constraints <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155394>`__

pw_docgen
---------
Support for auto-linking to Rust docs (when available) was added. We also
explained how to debug Pigweed's Sphinx extensions.

* `Add rustdoc linking support to pigweed-module tag <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159292>`__
* `Add extension debugging instructions <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156090>`__

pw_env_setup
------------
There were lots of updates around how the Pigweed environment uses Python.

* `pw_build: Disable pip version check <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160551>`__
* `Add docstrings to visitors <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159131>`__
* `Sort pigweed_environment.gni lines <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/158892>`__
* `Mac and Windows Python requirements <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/158912>`__
  (issue `#292098416 <https://issues.pigweed.dev/issues/292098416>`__)
* `Add more Python versions <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/158891>`__
  (issue `#292278707 <https://issues.pigweed.dev/issues/292278707>`__)
* `Remove python.json from Bazel CIPD <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/158911>`__
  (issue `#292585791 <https://issues.pigweed.dev/issues/292585791>`__)
* `Redirect variables from empty dirs <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/158890>`__
  (issue `#292278707 <https://issues.pigweed.dev/issues/292278707>`__)
* `Split Python constraints per OS <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157657>`__
  (issue `#292278707 <https://issues.pigweed.dev/issues/292278707>`__)
* `Add --additional-cipd-file argument <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/158170>`__
  (issue `#292280529 <https://issues.pigweed.dev/issues/292280529>`__)
* `Upgrade Python cryptography to 41.0.2 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157654>`__
* `Upgrade ipython to 8.12.2 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157653>`__
* `Upgrade PyYAML to 6.0.1 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157652>`__
* `Add Python constraints with hashes <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/153470>`__
  (issue `#287302102 <https://issues.pigweed.dev/issues/287302102>`__)
* `Bump pip and pip-tools <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156470>`__
* `Add coverage utilities <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155810>`__
  (issue `#279161371 <https://issues.pigweed.dev/issues/279161371>`__)

pw_fuzzer
---------
A fuzzer example was updated to more closely follow Pigweed code conventions.

* `Update fuzzers to use Pigweed domains <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/148337>`__

pw_hdlc
-------
Communication errors are now handled more gracefully.

* `Detect comms errors in Python <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155814>`__
* `Add target to Bazel build <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157651>`__

pw_i2c
------
An RPC service was added. Docs and code comments were updated to use ``responder``
and ``initiator`` terminology consistently.

* `Standardize naming on initiator/responder <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159132>`__
* `Add i2c rpc service <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155250>`__

pw_i2c_mcuxpresso
-----------------
* `Allow for static initialization of initiator <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155790>`__
* `Add test to ensure compilation of module <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155390>`__

pw_ide
------
* `Support multiple comp DB search paths <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/144210>`__
  (issue `#280363633 <https://issues.pigweed.dev/issues/280363633>`__)

pw_interrupt
------------
Work continued on how facade backend selection works in Bazel.

* `Add backend for xtensa processors <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160031>`__
* `Tidy up target compatibility <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160650>`__
  (issue `#272090220 <https://issues.pigweed.dev/issues/272090220>`__)
* `Remove cpu-based backend selection <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160380>`__
  (issue `#272090220 <https://issues.pigweed.dev/issues/272090220>`__)
* `Add backend constraint setting <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160371>`__
  (issue `#272090220 <https://issues.pigweed.dev/issues/272090220>`__)
* `Remove redundant Bazel targets <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154500>`__
  (issue `#290359233 <https://issues.pigweed.dev/issues/290359233>`__)

pw_log_rpc
----------
A docs section was added that explains how ``pw_log``, ``pw_log_tokenized``,
and ``pw_log_rpc`` interact with each other.

* `Explain relation to pw_log and pw_log_tokenized <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157231>`__

pw_package
----------
Raspberry Pi Pico and Zephyr support improved.

* `Add picotool package installer <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155791>`__
* `Handle windows Zephyr SDK setup <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157030>`__
* `Run Zephyr SDK setup.sh after syncing from CIPD <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156530>`__

pw_perf_test
------------
* `Remove redundant Bazel targets <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154498>`__
  (issue `#290359233 <https://issues.pigweed.dev/issues/290359233>`__)

pw_presubmit
------------
* `Add ifndef/define check <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/152173>`__
  (issue `#287529705 <https://issues.pigweed.dev/issues/287529705>`__)
* `Remove deprecated gn_docs_build step <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159291>`__
* `Fix issues with running docs_build twice <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159290>`__
* `Add Rust docs to docs site <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157656>`__

pw_protobuf_compiler
--------------------
* `Disable legacy namespace <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157232>`__
* `Transition to our own proto compiler rules <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157033>`__
  (issue `#234874064 <https://issues.pigweed.dev/issues/234874064>`__)
* `Allow external usage of macros <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155432>`__

pw_ring_buffer
--------------
``pw_ring_buffer`` now builds with ``-Wconversion`` enabled.

* `Conversion warning cleanups <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157430>`__
  (issue `#259746255 <https://issues.pigweed.dev/issues/259746255>`__)

pw_rpc
------
* `Create client call hook in Python client <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157870>`__
* `Provide way to populate response callbacks during tests <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156670>`__
* `Add Soong rule for pwpb echo service <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156270>`__

pw_rpc_transport
----------------
* `Add more Soong rules <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155035>`__

pw_rust
-------
We are preparing pigweed.dev to automatically link to auto-generated
Rust module documentation when available.

* `Add combined Rust doc support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157632>`__
* `Update @rust_crates sha <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155051>`__

pw_spi
------
We updated docs and code comments to use ``initiator`` and ``responder``
terminology consistently.

* `Standardize naming on initiator/responder <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159132>`__

pw_status
---------
* `Add Clone and Copy to Rust Error enum <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157093>`__

pw_stream
---------
We continued work on Rust support.

* `Fix Doxygen typo <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154732>`__
* `Add read_exact() an write_all() to Rust Read and Write traits <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157094>`__
* `Clean up rustdoc warnings <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157092>`__
* `Add Rust varint reading and writing support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156451>`__
* `Refactor Rust cursor to reduce monomorphization <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155391>`__
* `Add Rust integer reading support <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155053>`__
* `Move Rust Cursor to it's own sub-module <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155052>`__

pw_stream_uart_linux
--------------------
A new UART Linux backend for ``pw_stream`` was added.

* `Add stream for UART on Linux <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156910>`__

pw_sync
-------
C++ lock traits were added and used.

* `Improve Borrowable lock traits and annotations <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/153573>`__
  (issue `#261078330 <https://issues.pigweed.dev/issues/261078330>`__)
* `Add lock traits <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/153572>`__

pw_sync_freertos
----------------
* `Fix ODR violation in tests <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160795>`__

pw_sys_io
---------
* `Add android to alias as host system <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157871>`__
  (issue `#272090220 <https://issues.pigweed.dev/issues/272090220>`__)
* `Add chromiumos to alias as host system <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155811>`__
  (issue `#272090220 <https://issues.pigweed.dev/issues/272090220>`__)

pw_system
---------
* `Update IPython init API <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157872>`__
* `Remove redundant Bazel targets <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154497>`__
  (issue `#290359233 <https://issues.pigweed.dev/issues/290359233>`__)

pw_tokenizer
------------
We refactored the ``pw_tokenizer`` docs to adhere to :ref:`seed-0102`.

* `Update tagline, restore missing info, move sections <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/158192>`__
* `Migrate the proto docs (SEED-0102) <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157655>`__
* `Remove stub sections and add guides link (SEED-0102) <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157631>`__
* `Migrate the custom macro example (SEED-0102) <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157032>`__
* `Migrate the Base64 docs (SEED-0102) <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156531>`__
* `Migrate token collision docs (SEED-0102) <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155818>`__
* `Migrate detokenization docs (SEED-0102) <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155815>`__
* `Migrate masking docs (SEED-0102) <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155812>`__

pw_toolchain
------------
The build system was modified to use relative paths to avoid unintentionally
relying on the path environment variable. Map file generation is now optional
to avoid generating potentially large map files when they're not needed.

* `Test trivially destructible class <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159232>`__
* `Make tools use relative paths <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159130>`__
  (issue `#290848929 <https://issues.pigweed.dev/issues/290848929>`__)
* `Support conditionally creating mapfiles <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157431>`__

pw_trace_tokenized
------------------
* `Replace singletons with dependency injection <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155813>`__
* `Remove redundant Bazel targets <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154499>`__
  (issue `#290359233 <https://issues.pigweed.dev/issues/290359233>`__)

pw_unit_test
------------
* `Update metadata test type for unit tests <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/154550>`__

pw_varint
---------
* `Update Rust API to return number of bytes written <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156450>`__

pw_watch
--------
We fixed an issue where builds were getting triggered when files were opened
or closed without modication.

* `Trigger build only on file modifications <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157633>`__

pw_web
------
* `Remove dependency on 'crc' and 'buffer' NPM packages <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160830>`__
* `Update theme token values and usage <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155970>`__
* `Add disconnect() method to WebSerialTransport <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156471>`__
* `Add docs section for log viewer component <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155050>`__

Build
=====

bazel
-----
* `Add host_backend_alias macro <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160550>`__
  (issue `#272090220 <https://issues.pigweed.dev/issues/272090220>`__)
* `Fix missing deps in some modules <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160376>`__
* `Support user bazelrc files <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160030>`__
* `Update rules_python to 0.24.0 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/158913>`__
  (issue `#266950138 <https://issues.pigweed.dev/issues/266950138>`__)

build
-----
* `Use remote Java runtime for Bazel build <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160793>`__
  (issue `#291791485 <https://issues.pigweed.dev/issues/291791485>`__)
* `Add Rust toolchain to Bazel macOS build <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159491>`__
  (issue `#291749888 <https://issues.pigweed.dev/issues/291749888>`__)
* `Mark linux-only Bazel build targets <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/158191>`__

Targets
=======

targets/rp2040_pw_system
------------------------
Some of the Pico docs incorrectly referred to another hardware platform.

* `Fix references to STM32 <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157233>`__
  (issue `#286652309 <https://issues.pigweed.dev/issues/286652309>`__)

Language support
================

python
------
* `Remove setup.py files <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159472>`__

rust
----
* `Add rustdoc links for existing crates <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/159470>`__

OS support
==========

zephyr
------
* `Add project name to unit test root <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156850>`__
* `Add pigweed root as module <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156596>`__
* `Fix setup.sh call <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156591>`__

Docs
====
We added a feature grid to the homepage and fixed outdated info in various
docs.

* `pigweed.dev feature grid <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157658>`__
* `Mention SEED-0102 in module_structure.rst <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157234>`__
  (issue `#286477675 <https://issues.pigweed.dev/issues/286477675>`__)
* `Remove outdated Homebrew info in getting_started.rst <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157291>`__
  (issue `#287528787 <https://issues.pigweed.dev/issues/287528787>`__)
* `Fix "gn args" examples which reference pw_env_setup_PACKAGE_ROOT <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/156452>`__
* `Consolidate contributing docs in site nav <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/155816>`__

SEEDs
=====

SEED-0107
---------
* `Claim SEED number <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157031>`__

SEED-0108
---------
* `Claim SEED number <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/158171>`__

Third party
===========

third_party
-----------
* `Remove now unused rules_proto_grpc <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/157290>`__

third_party/mbedtls
-------------------
* `3.3.0 compatibility <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/160790>`__
  (issue `#293612945 <https://issues.pigweed.dev/issues/293612945>`__)
