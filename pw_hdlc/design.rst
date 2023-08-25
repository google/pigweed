.. _module-pw_hdlc-design:

===============
pw_hdlc: Design
===============
.. pigweed-module-subpage::
   :name: pw_hdlc
   :tagline: Lightweight, simple, and easy serial communication

``pw_hdlc`` implements a subset of the
`HDLC <https://en.wikipedia.org/wiki/High-Level_Data_Link_Control>`_
protocol.

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
This module first writes an initial frame delimiter byte (0x7E) to indicate the
beginning of the frame. Before sending any of the payload data through serial,
the special bytes are escaped:

+-------------------------+-----------------------+
| Unescaped Special Bytes | Escaped Special Bytes |
+=========================+=======================+
|           7E            |        7D 5E          |
+-------------------------+-----------------------+
|           7D            |        7D 5D          |
+-------------------------+-----------------------+

The bytes of the payload are escaped and written in a single pass. The
frame check sequence is calculated, escaped, and written after. After this, a
final frame delimiter byte (0x7E) is written to mark the end of the frame.

Decoding received bytes
=======================
Frames may be received in multiple parts, so we need to store the received data
in a buffer until the ending frame delimiter (0x7E) is read. When the
``pw_hdlc`` decoder receives data, it unescapes it and adds it to a buffer.
When the frame is complete, it calculates and verifies the frame check sequence
and does the following:

* If correctly verified, the decoder returns the decoded frame.
* If the checksum verification fails, the frame is discarded and an error is
  reported.
