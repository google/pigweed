.. _module-pw_chre:

=======
pw_chre
=======

.. warning::

  This module is extremely experimental. Parts of this module might be broken,
  and the module does not provide a stable API.

The `Context Hub Runtime Environment <https://source.android.com/docs/core/interaction/contexthub>`_
(CHRE) is Android's platform for developing always-on applications called
nanoapps. These nanoapps run on a vendor-specific processor which is more power
efficient. Nanoapps use the CHRE API, which is standardized across platforms,
allowing them to be code-compatible across devices.

This module implements a Pigweed backend to CHRE. In order to use this module,
``dir_pw_third_party_chre`` must point to the directory to the CHRE library.

-----------
Get started
-----------

To integrate ``pw_chre`` with your project:

- Call the initialization functions and start the event loop.
- Handle messages from the application processor and connect them through to
  the CHRE runtime.
- Implement the functions in ``pw_chre/host_link``. This is how CHRE sends
  messages to the application processor.


``$pw_chre:chre_example`` runs the CHRE environment using ``pw_system``.
This also loads several static example nanoapps from the CHRE codebase by
compiling them into the executable. This can be a helpful reference.

CHRE is implemented using the following Pigweed modules for functionality:

- ``pw_chrono:system_timer``: implements getting monotonic time
- ``pw_log``: implements logging to the application processor
- ``pw_assert``: implements crash handling
- ``pw_sync``:  implements mutual exclusion primitives
- ``malloc/free``: implements virtual memory allocation
  (This may be eventually replaced with a pigweed module)

----------------------
Current implementation
----------------------

As mentioned at the top of this document, ``pw_chre`` is extremely experimental.
Only a few parts of CHRE have been tested. There are likely to be bugs and
unimplemented behavior. The lists below track the current state and will
be updated as things change.

Supported and tested behavior:

- Loading static nanoapps.
- The following sample nanoapps have been run:
  - hello_world
  - debug_dump_world
  - timer_world
  - unload_tester
  - message_world
  - spammer
- Logging from a nanoapp.
- Allocating memory (although it uses malloc/free).
- Sending messages to/from the AP.

Features not implemented, but likely to be implemented in the future:

- Context Hub Qualification Test Suite (CHQTS).
- Some simulated PALS for testing (based off of CHRE's linux simulated PALs).
- Power Management APIs, e.g: waking the host AP and flushing messages.
- Instructions around implementing a PAL.
- Instructions around building and testing a nanoapp.
- Dynamically loading nanoapps.

Features that would be nice to have:

- A plug-and-play implementation of AP <-> MCU flatbuffer message communication.
- Pigweed defined facades for each PAL.
- PAL implementations using Pigweed functionality (i.e: implementing bluetooth
  via ``pw_bluetooth``).
- Pigweed defined facades for core CHRE functionality, such as clock selection,
  memory management, cache management.
- A protobuf implementation of CHRE's flatbuffer API.
- Cmake and Bazel build system integration.

-------------
API reference
-------------
.. doxygennamespace:: pw::chre
   :members:

-------------
Porting Guide
-------------
The ``pw_chre`` module has completed the steps outlined for `creating a new CHRE platform`_ .

.. _Creating a new CHRE platform: https://android.googlesource.com/platform/system/chre/+/refs/heads/main/doc/porting_guide.md#recommended-steps-for-porting-chre

The ``pw_chre`` module still needs to be configured correctly on a new platform.
You ``pw_chre`` user is responsible for:

- Starting a thread for CHRE's event loop and calling the correct APIs.
- Forwarding messages to/from the Application Processor (AP).

-----------------------------
Adding Optional Feature Areas
-----------------------------
However, ``pw_chre`` users will likely want to implement their own
Platform Abstraction Layers (PALs). For more information see this
`implementation guide <https://android.googlesource.com/platform/system/chre/+/refs/heads/main/doc/porting_guide.md#implementing-optional-feature-areas-e_g_pals>`_.

.. list-table:: List of PALs
   :widths: 1 1
   :header-rows: 1

   * - PAL Name
     - Pigweed implementation available
   * - Audio
     - ❌
   * - Bluetooth
     - ❌
   * - GNSS
     - ❌
   * - Sensor
     - ❌
   * - Wifi
     - ❌
   * - WWAN
     - ❌


For more information on a specific PAL see
`the PAL headers <https://cs.android.com/android/platform/superproject/+/main:system/chre/pal/include/chre/pal/>`_
or the `Linux reference PAL implementations <https://cs.android.com/android/platform/superproject/+/main:system/chre/platform/linux/>`_.
