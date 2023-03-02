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
definitions. So far, packets from the following protocols are defined:

- HCI

Usage
=====
1. Set the `dir_pw_third_party_emboss` GN variable to the path of your Emboss
checkout.

2. Add `$dir_pw_bluetooth/emboss_hci` (for HCI packets) or
`$dir_pw_bluetooth/emboss_vendor` (for vendor packets) to your dependency list.

3. Include the generated header files.

.. code-block:: cpp

   #include "pw_bluetooth/hci.emb.h"
   #include "pw_bluetooth/vendor.emb.h"

4. Construct an Emboss view over a buffer.

.. code-block:: cpp

   std::array<uint8_t, 4> buffer = {0x00, 0x01, 0x02, 0x03};
   auto view = pw::bluetooth::emboss::MakeTestCommandPacketView(&buffer);
   EXPECT_EQ(view.payload().Read(), 0x03);

.. note::

   clangd will complain that the generated header file does not exist.
   You need to build your project to resolve this. Similarly, you need to build
   in order for .emb file updates to be reflected in the generated headers.

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
- Create a backend for the Bluetooth API using Fuchsia's Bluetooth stack
  (sapphire).
- Stabilize the Bluetooth API.
- Add BR/EDR APIs.
- Bazel support
- CMake support
