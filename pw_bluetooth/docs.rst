.. _module-pw_bluetooth:

================
pw_bluetooth
================
The ``pw_bluetooth`` module contains APIs and utilities for the host layer of
Bluetooth Low Energy.

--------
Host API
--------
.. attention::

   This module is still under construction, the API is not yet stable.

The headers in `public/pw_bluetooth` constitute a Bluetooth Host API. `host.h`
is the entry point from which all other APIs are exposed. Currently, only Low
Energy APIs exist.

Host
====
.. doxygenclass:: pw::bluetooth::Host
   :members:

low_energy::Central
===================
.. doxygenclass:: pw::bluetooth::low_energy::Central
   :members:

low_energy::Peripheral
======================
.. doxygenclass:: pw::bluetooth::low_energy::Peripheral
   :members:

low_energy::AdvertisedPeripheral
================================
.. doxygenclass:: pw::bluetooth::low_energy::AdvertisedPeripheral
   :members:

low_energy::Connection
======================
.. doxygenclass:: pw::bluetooth::low_energy::Connection
   :members:

low_energy::ConnectionOptions
=============================
.. doxygenstruct:: pw::bluetooth::low_energy::ConnectionOptions
   :members:

low_energy::RequestedConnectionParameters
=========================================
.. doxygenstruct:: pw::bluetooth::low_energy::RequestedConnectionParameters
   :members:

low_energy::ConnectionParameters
================================
.. doxygenstruct:: pw::bluetooth::low_energy::ConnectionParameters
   :members:

gatt::Server
============
.. doxygenclass:: pw::bluetooth::gatt::Server
   :members:

gatt::LocalServiceInfo
======================
.. doxygenstruct:: pw::bluetooth::gatt::LocalServiceInfo
   :members:

gatt::LocalService
==================
.. doxygenclass:: pw::bluetooth::gatt::LocalService
   :members:

gatt::LocalServiceDelegate
==========================
.. doxygenclass:: pw::bluetooth::gatt::LocalServiceDelegate
   :members:

gatt::Client
============
.. doxygenclass:: pw::bluetooth::gatt::Client
   :members:

gatt::RemoteService
===================
.. doxygenclass:: pw::bluetooth::gatt::RemoteService
   :members:

Callbacks
=========
This module contains callback-heavy APIs. Callbacks must not call back into the
``pw_bluetooth`` APIs unless otherwise noted. This includes calls made by
destroying objects returned by the API. Additionally, callbacks must not block.

-------------------------
Emboss Packet Definitions
-------------------------
``pw_bluetooth`` contains `Emboss <https://github.com/google/emboss>`_ packet
definitions. So far, some packets and enums from the following protocols are
defined:

- HCI
- L2CAP
- H4

Usage
=====
1. Add Emboss to your project as described in
   :ref:`module-pw_third_party_emboss`.

.. tab-set::

   .. tab-item:: Bazel
      :sync: bazel

      2. Add the Emboss target you require to your build target's dependency
         list - for example, ``@pigweed//pw_bluetooth:emboss_hci_group`` for HCI
         packets.

   .. tab-item:: GN
      :sync: gn

      2. Add the Emboss target you require to your build target's dependency
         list - for example, ``$dir_pw_bluetooth/emboss_hci_group`` for HCI
         packets.

   .. tab-item:: CMake
      :sync: cmake

      2. Add the Emboss target you require to your build target's dependency
         list - for example, ``pw_bluetooth.emboss_hci_group`` for HCI packets.


3. Include the generated header files you require in your source files.

.. code-block:: cpp

   #include "pw_bluetooth/hci_commands.emb.h"
   #include "pw_bluetooth/hci_vendor.emb.h"

.. inclusive-language: disable

4. Construct an Emboss view over a buffer as described in the `Emboss User Guide
   <https://github.com/google/emboss/blob/master/doc/guide.md>`_.

.. inclusive-language: enable

.. literalinclude:: emboss_test.cc
   :language: cpp
   :start-after: [pw_bluetooth-examples-make_view]
   :end-before: [pw_bluetooth-examples-make_view]

.. note::

   When you first add generated headers, ``clangd`` may complain that the
   generated header files do not exist. You need to build your project to
   resolve this. Similarly, you need to rebuild in order for .emb file updates
   to be reflected in the generated headers.


Contributing
============
Emboss ``.emb`` files can be edited to add additional packets and enums.

Emboss files should be formatted by running the following from the Pigweed root.

.. code-block:: bash

   (export EMBOSS_PATH="/ssd2/dev2/pigweed/environment/packages/emboss" &&
       export PYTHONPATH+=":${EMBOSS_PATH}" &&
       python3 "${EMBOSS_PATH}/compiler/front_end/format.py" pw_bluetooth/public/pw_bluetooth/*.emb)

If adding files, be sure to update the GN, Bazel, and CMake build rules.
Presubmit runs the ``emboss_test.cc`` test on all three.


Size Report
===========
Delta of +972 when constructing the first packet view and reading/writing a
field. This includes the runtime library and the 4-byte buffer.

.. include:: emboss_size_report

Delta of +96 when adding a second packet view and reading/writing a field.

.. include:: emboss_size_report_diff

-------
Roadmap
-------
- Bluetooth Proxy (WIP in in :ref:`module-pw_bluetooth_proxy`)
- Add automated Emboss file formatting to `pw format` (`b/331195584 <https://pwbug.dev/331195584>`_)
- Create a backend for the Bluetooth API using Fuchsia's Bluetooth stack
  (Sapphire).
- Stabilize the Bluetooth API.
- Add BR/EDR APIs.
