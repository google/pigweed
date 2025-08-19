.. _module-pw_bluetooth_sapphire:

=====================
pw_bluetooth_sapphire
=====================
.. pigweed-module::
   :name: pw_bluetooth_sapphire

.. attention::
  This module is still under construction. There is no public API yet. Please
  contact the Pigweed team if you are interested in using this module.

The ``pw_bluetooth_sapphire`` module provides a dual-mode Bluetooth Host stack
that will implement the :ref:`module-pw_bluetooth` APIs.  Sapphire originated as
the Fuchsia operating system's Bluetooth stack and is used in production. The
Sapphire Host stack has been migrated into the Pigweed project for use in
embedded projects. However, as it was not written in an embedded context, there
is a lot of work to be done to optimize memory usage.

Why use Sapphire?

* **Open source**, unlike vendor Bluetooth stacks. This makes debugging and
  fixing issues much easier!
* **Integrated with Pigweed**. Logs, asserts, randomness, time, async, strings,
  and more are all using Pigweed modules.
* **Excellent test coverage**, unlike most Bluetooth stacks. This means fewer
  bugs and greater maintainability.
* **Certified**. Sapphire was certified by the Bluetooth SIG after passing
  all conformance tests.
* **A great API**. Coming 2025. See :ref:`module-pw_bluetooth`.

------------
Architecture
------------

Sapphire is composed of multiple libraries that implement different protocols
in the Core specification. These libraries include:

HCI: Host-Controller Interface
  Processes data (ACL), command, and event packets exchanged between the host
  and the controller. Note that HCI is currently split into ``hci``,
  ``transport``, and ``hci-spec`` libraries.

L2CAP: Logical Link Control and Adaptation Layer Protocol
  Multiplexes a single ACL connection into multiple channels and manages
  channel configuration and flow control.

SDP: Service Discovery Protocol
  Serves the BR/EDR service database and searches peers for BR/EDR services.

SM: Security Manager
  Implements LE security, including pairing.

ATT: Attribute Protocol
  Implements the ATT protocol PDUs, flow control, and database.

GATT: Generic Attribute Profile
  Implements the GATT server and client.

GAP: Generic Access Profile
  Responsible for device discovery and connection functionality.

SCO: Synchronous Connection Oriented link
  Used for audio data by the Hands-Free Profile.

ISO: Isochronous Channels
  Used for audio data in LE Audio.

These libraries build on top of each other as shown in the following diagram.
HCI is the foundation, L2CAP builds on top of HCI, ATT builds on top of L2CAP,
etc.

.. mermaid::
   :alt: Sapphire architecture

   flowchart TB
     subgraph HCI["HCI"]
            TRA["Transport"]
            HCI2["HCI"]
            SPEC["HCI-Spec"]
            CONT["Controllers"]
     end
     subgraph Host["Host"]
            COM["Common"]
            GATT["GATT"]
            ISO["ISO"]
            SCO["SCO"]
            SM["SM"]
            L2CAP["L2CAP"]
            SDP["SDP"]
            GAP["GAP"]
            ATT["ATT"]
            HCI
      end
      GAP --> GATT & ISO & SCO & SM & L2CAP & SDP & HCI2
      GATT --> ATT
      ATT --> L2CAP
      L2CAP --> TRA
      SCO --> HCI2 & TRA
      SM --> L2CAP
      SDP --> L2CAP
      ISO --> TRA
      HCI2 --> TRA
      TRA --> CONT
      CONT -- UART/USB --> Controller["Controller"]


-------------
Certification
-------------
The Sapphire Host stack was certified as implementing the Bluetooth Core
Specification 5.0 by the Bluetooth SIG in 2020 as "Google Sapphire 1.0
Bluetooth Core Host Solution". You can find its public listing and add it to
your product certification at the `Qualification Workspace
<https://qualification.bluetooth.com/ListingDetails/112941>`_.

-------------------------
Products running Sapphire
-------------------------
Sapphire is running on millions of the following devices:

* Google Nest Hub (1st Gen)
* Google Nest Hub (2nd Gen)
* Google Nest Hub Max

------------------
Supported features
------------------

GAP
===
.. include:: host/gap/docs.rst
   :start-after: .. supported-features-start
   :end-before: .. supported-features-end

L2CAP
=====
.. include:: host/l2cap/docs.rst
   :start-after: .. supported-features-start
   :end-before: .. supported-features-end

ATT
===
.. include:: host/att/docs.rst
   :start-after: .. supported-features-start
   :end-before: .. supported-features-end

------------
Contributing
------------

.. _module-pw_bluetooth_sapphire-building:

Building
========
The following commands will build test binaries for the host platform. The
tests use GoogleTest so they are not supported by the default build
configuration.

.. tab-set::

   .. tab-item:: Bazel

      .. code-block:: console

         bazelisk build --config googletest //pw_bluetooth_sapphire/host/...

   .. tab-item:: GN

      First, install the boringssl, emboss, and googletest packages with ``pw package``.

      .. code-block:: console

         pw package install boringssl
         pw package install emboss
         pw package install googletest

      Next, configure the GN args for all of the packages and backends that
      Sapphire uses. For example:

      .. code-block:: console

         gn args out

      .. code-block:: console

         dir_pw_third_party_boringssl = "/path/to/pigweed/.environment/packages/boringssl"
         dir_pw_third_party_emboss = "/path/to/pigweed/.environment/packages/emboss"
         dir_pw_third_party_googletest = "/path/to/pigweed/.environment/packages/googletest"
         pw_bluetooth_sapphire_ENABLED = true
         pw_unit_test_MAIN = "//third_party/googletest:gmock_main"
         pw_unit_test_BACKEND = "//pw_unit_test:googletest"
         pw_function_CONFIG = "//pw_function:enable_dynamic_allocation"
         pw_chrono_SYSTEM_CLOCK_BACKEND = "//pw_chrono_stl:system_clock"
         pw_chrono_SYSTEM_TIMER_BACKEND = "//pw_chrono_stl:system_timer"
         pw_async_TASK_BACKEND = "//pw_async_basic:task"
         pw_async_FAKE_DISPATCHER_BACKEND = "//pw_async_basic:fake_dispatcher"

      Finally, build with ``pw watch``.

Running tests
=============
.. tab-set::

   .. tab-item:: Bazel

      Run all tests:

      .. code-block:: console

         bazelisk test --config googletest //pw_bluetooth_sapphire/host/...

      Run l2cap tests with a test filter, logs, and log level filter:

      .. code-block:: console

         bazelisk test --config googletest //pw_bluetooth_sapphire/host/l2cap:l2cap_test \
           --test_arg=--gtest_filter="*InboundChannelFailure" \
           --test_output=all \
           --copt=-DPW_LOG_LEVEL_DEFAULT=PW_LOG_LEVEL_ERROR

   .. tab-item:: GN

      The easiest way to run GN tests is with ``pw presubmit``, which will install
      dependencies and set GN args correctly.

      .. code-block:: console

         $ pw presubmit --step gn_chre_googletest_nanopb_sapphire_build

      You can also use ``pw watch`` if you install required packages and set
      your GN args as described in the :ref:`Building
      <module-pw_bluetooth_sapphire-building>` section.

Clangd support
==============
In order to use :ref:`module-pw_ide` to generate a compilation database for
Clangd, you must first configure your GN args as described in the
:ref:`Building <module-pw_bluetooth_sapphire-building>` GN tab.

-------
Roadmap
-------
* Support CMake
* Implement :ref:`module-pw_bluetooth` APIs
* Optimize memory footprint
* Add metrics
* Add configuration options (LE only, Classic only, etc.)
* Add CLI for controlling stack over RPC

.. toctree::
   :maxdepth: 1

   fuchsia
   size_report

.. _fuchsia/prebuilt/bt-host: https://chrome-infra-packages.appspot.com/p/fuchsia/prebuilt/bt-host
.. _fuchsia/prebuilt/bt-hci-virtual: https://chrome-infra-packages.appspot.com/p/fuchsia/prebuilt/bt-hci-virtual
.. _pigweed-linux-bazel-bthost: https://ci.chromium.org/ui/p/pigweed/builders/pigweed.ci/pigweed-linux-bazel-bthost
.. _GN presubmit step: https://cs.opensource.google/pigweed/pigweed/+/main:pw_presubmit/py/pw_presubmit/pigweed_presubmit.py?q=gn_chre_googletest_nanopb_sapphire_build
