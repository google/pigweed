:tocdepth: 2

.. _docs-changelog:

=====================
What's New In Pigweed
=====================

--------------------------------
Talk to the team at Pigweed Live
--------------------------------
.. pigweed-live::

.. _docs-changelog-latest:

-----------
Mar 7, 2024
-----------

.. changelog_highlights_start

Highlights (Feb 22, 2024 to Mar 7, 2024):

* The new :ref:`module-pw_digital_io_linux` module is a
  :ref:`module-pw_digital_io` backend for Linux userspace.
* :cpp:class:`pw::multibuf::MultiBufAllocator` class is a new interface
  for allocating ``pw::multibuf::MultiBuf`` objects.
* The ``pw_web`` log viewer now captures browser console logs. It also
  now supports creating log stores and downloading logs from stores.

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
* `Fix formatting of typescript code
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
  follow our latest :ref:`docs-contrib-moduledocs`.

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
  to follow our latest :ref:`docs-contrib-moduledocs`.
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
  :ref:`docs-contrib-moduledocs`.

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
:ref:`docs-contrib-moduledocs`.

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
our latest :ref:`docs-contrib-moduledocs`.

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
:ref:`docs-contrib-changelog` and see ``//docs/_static/js/changelog.js``
to view its implementation.

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
  :ref:`docs-contrib-moduledocs`. :ref:`module-pw_string` is now an example
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
The docs were updated to match the new :ref:`docs-contrib-moduledocs`.

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
:ref:`docs-contrib-moduledocs`. :ref:`module-pw_string` is now an example
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
* `Allow add_global_link_deps to be overriden <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/150050>`__
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
``pw_varint`` now has a :ref:`C-only API <module-pw_varint-api-c>`.

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
  :ref:`docs-editors`.
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
We created a new doc (:ref:`docs-editors`) that explains how to improve Pigweed
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
