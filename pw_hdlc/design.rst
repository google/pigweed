.. _module-pw_hdlc-design:

================
Design & roadmap
================
.. pigweed-module-subpage::
   :name: pw_hdlc
   :tagline: pw_hdlc: Simple, robust, and efficient serial communication

.. pw_hdlc-overview-start

``pw_hdlc`` implements a subset of the `High-Level Data Link Control
<https://en.wikipedia.org/wiki/High-Level_Data_Link_Control>`_ (HDLC) protocol.
HDLC is a data link layer protocol intended for serial communication between
devices and is standardized as `ISO/IEC 13239:2002
<https://www.iso.org/standard/37010.html>`_.

.. pw_hdlc-overview-end

--------
Overview
--------
The ``pw_hdlc`` module provides a simple, robust frame-oriented transport that
uses a subset of the HDLC protocol. ``pw_hdlc`` supports sending between
embedded devices or the host. It can be used with :ref:`module-pw_rpc` to enable
remote procedure calls (RPCs) on embedded devices.

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

--------------------
Protocol Description
--------------------

Frames
======
The HDLC implementation in ``pw_hdlc`` supports only HDLC unnumbered
information frames. These frames are encoded as follows:

.. code-block:: text

   _________________________________________
   | | | |                          |    | |...
   | | | |                          |    | |... [More frames]
   |_|_|_|__________________________|____|_|...
    F A C       Payload              FCS  F

    F = flag byte (0x7e, the ~ character)
    A = address field
    C = control field
    FCS = frame check sequence (CRC-32)


Encoding and sending data
=========================
This module first writes an initial frame delimiter byte (``0x7E``) to indicate
the beginning of the frame. Before sending any of the payload data through
serial, the special bytes are escaped:

+-------------------------+-----------------------+
| Unescaped Special Bytes | Escaped Special Bytes |
+=========================+=======================+
|         ``7E``          |      ``7D 5E``        |
+-------------------------+-----------------------+
|         ``7D``          |      ``7D 5D``        |
+-------------------------+-----------------------+

The bytes of the payload are escaped and written in a single pass. The
frame check sequence is calculated, escaped, and written after. After this, a
final frame delimiter byte (``0x7E``) is written to mark the end of the frame.

Decoding received bytes
=======================
Frames may be received in multiple parts, so ``pw_hdlc`` needs to store the
received data in a buffer until the ending frame delimiter (``0x7E``) is read.
When the ``pw_hdlc`` decoder receives data, it unescapes it and adds it to a
buffer. When the frame is complete, it calculates and verifies the frame check
sequence and does the following:

* If correctly verified, the decoder returns the decoded frame.
* If the checksum verification fails, the frame is discarded and an error is
  reported.

-------
Roadmap
-------
- **Expanded protocol support** - ``pw_hdlc`` currently only supports
  unnumbered information frames. Support for different frame types and
  extended control fields may be added in the future.

-----------------
More pw_hdlc docs
-----------------
.. include:: docs.rst
   :start-after: .. pw_hdlc-nav-start
   :end-before: .. pw_hdlc-nav-end
