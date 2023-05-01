.. _module-pw_hdlc:

.. rst-class:: with-subtitle

=======
pw_hdlc
=======

.. pigweed-module::
   :name: pw_hdlc
   :tagline: Lightweight, simple, and easy serial communication
   :status: stable
   :languages: C++17
   :code-size-impact: 1400 to 2600 bytes
   :get-started: module-pw_hdlc-get-started
   :design: module-pw_hdlc-design
   :guides: module-pw_hdlc-guide
   :api: module-pw_hdlc-api

   - Transmit RPCs and other data between devices over serial
   - Detect corruption and data loss
   - Stream to transport without buffering

----------
Background
----------

`High-Level Data Link Control (HDLC)
<https://en.wikipedia.org/wiki/High-Level_Data_Link_Control>`_ is a data link
layer protocol intended for serial communication between devices. HDLC is
standardized as `ISO/IEC 13239:2002 <https://www.iso.org/standard/37010.html>`_.

------------
Our solution
------------

The ``pw_hdlc`` module provides a simple, robust frame-oriented transport that
uses a subset of the HDLC protocol. ``pw_hdlc`` supports sending between
embedded devices or the host. It can be used with :ref:`module-pw_rpc` to enable
remote procedure calls (RPCs) on embedded on devices.

``pw_hdlc`` has two primary functions:

* **Encoding** data by constructing a frame with the escaped payload bytes and
  frame check sequence.
* **Decoding** data by unescaping the received bytes, verifying the frame
  check sequence, and returning successfully decoded frames.

Design considerations
=====================

* ``pw_hdlc`` only supports unnumbered information frames.
* It uses special escaped bytes to mark the beginning and end of a frame.
* Frame data is stored in a buffer until the end-of-frame delimiter byte is read.

See :ref:`module-pw_hdlc-design` for more information.

Size report
===========
``pw_hdlc`` currently optimizes for robustness and flexibility instead of
binary size or performance.

There are two size reports: the first shows the cost of everything needed to
use HDLC, including the dependencies on common modules like CRC32 from
:ref:`module-pw_checksum` and variable-length integer handling from
:ref:`module-pw_varint`. The other is the cost if your application is already
linking those functions. ``pw_varint`` is commonly used since it's necessary
for protocol buffer handling, so is often already present.

.. include:: size_report

Roadmap
=======
- **Expanded protocol support** - ``pw_hdlc`` currently only supports
  unnumbered information frames. Support for different frame types and
  extended control fields may be added in the future.

.. _module-pw_hdlc-get-started:

---------------
Getting started
---------------

For an example of how to use HDLC with :ref:`module-pw_rpc`, see the
:ref:`module-pw_hdlc-rpc-example`.

Example pw::rpc::system_server backend
======================================
This module includes an example implementation of ``pw_rpc``'s ``system_server``
facade. This implementation sends HDLC encoded RPC packets via ``pw_sys_io``,
and has blocking sends/reads, so it is hardly performance-oriented and
unsuitable for performance-sensitive applications. This mostly servers as a
simplistic example for quickly bringing up RPC over HDLC on bare-metal targets.

Zephyr
======
To enable ``pw_hdlc.pw_rpc`` for Zephyr add ``CONFIG_PIGWEED_HDLC_RPC=y`` to
the project's configuration.

.. toctree::
  :maxdepth: 1
  :hidden:

  design
  api
  rpc_example/docs
  guide
