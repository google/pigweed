.. _module-pw_hdlc_lite:

------------
pw_hdlc_lite
------------
`High-Level Data Link Control (HDLC)
<https://en.wikipedia.org/wiki/High-Level_Data_Link_Control>`_ is a data link
layer protocol intended for serial communication between devices. HDLC is
standardized as `ISO/IEC 13239:2002 <https://www.iso.org/standard/37010.html>`_.

The ``pw_hdlc_lite`` module provides a simple, robust frame-oriented
transport that uses a subset of the HDLC protocol. ``pw_hdlc_lite`` supports
sending between embedded devices or the host. It can be used with
:ref:`module-pw_rpc` to enable remote procedure calls (RPCs) on embedded on
devices.

**Why use the pw_hdlc_lite module?**

  * Enables the transmission of RPCs and other data between devices over serial.
  * Detects corruption and data loss.
  * Light-weight, simple, and easy to use.
  * Supports streaming to transport without buffering, since the length is not
    encoded.

.. admonition:: Try it out!

  For an example of how to use HDLC with :ref:`module-pw_rpc`, see the
  :ref:`module-pw_hdlc_lite-rpc-example`.

.. toctree::
  :maxdepth: 1
  :hidden:

  rpc_example/docs

Protocol Description
====================

Frames
------
The HDLC implementation in ``pw_hdlc_lite`` supports only HDLC information
frames. These frames are encoded as follows:

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
-------------------------
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
-----------------------
Frames may be received in multiple parts, so we need to store the received data
in a buffer until the ending frame delimiter (0x7E) is read. When the
``pw_hdlc_lite`` decoder receives data, it unescapes it and adds it to a buffer.
When the frame is complete, it calculates and verifies the frame check sequence
and does the following:

* If correctly verified, the decoder returns the decoded frame.
* If the checksum verification fails, the frame is discarded and an error is
  reported.

API Usage
=========
There are two primary functions of the ``pw_hdlc_lite`` module:

  * **Encoding** data by constructing a frame with the escaped payload bytes and
    frame check sequence.
  * **Decoding** data by unescaping the received bytes, verifying the frame
    check sequence, and returning successfully decoded frames.

Encoder
-------
The Encoder API provides a single function that encodes data as an HDLC
information frame.

C++
^^^
.. cpp:namespace:: pw

.. cpp:function:: Status hdlc_lite::WriteInformationFrame(uint8_t address, ConstByteSpan data, stream::Writer& writer)

  Writes a span of data to a :ref:`pw::stream::Writer <module-pw_stream>` and
  returns the status. This implementation uses the :ref:`module-pw_checksum`
  module to compute the CRC-32 frame check sequence.

.. code-block:: cpp

  #include "pw_hdlc_lite/encoder.h"
  #include "pw_hdlc_lite/sys_io_stream.h"

  int main() {
    pw::stream::SysIoWriter serial_writer;
    Status status = WriteInformationFrame(123 /* address */,
                                          data,
                                          serial_writer);
    if (!status.ok()) {
      PW_LOG_INFO("Writing frame failed! %s", status.str());
    }
  }

Python
^^^^^^
.. automodule:: pw_hdlc_lite.encode
  :members:

.. code-block:: python

  import serial
  from pw_hdlc_lite import encode

  ser = serial.Serial()
  ser.write(encode.information_frame(b'your data here!'))

Decoder
-------
The decoder class unescapes received bytes and adds them to a buffer. Complete,
valid HDLC frames are yielded as they are received.

C++
^^^
.. cpp:class:: pw::hdlc_lite::Decoder

  .. cpp:function:: pw::Result<Frame> Process(std::byte b)

    Parses a single byte of an HDLC stream. Returns a Result with the complete
    frame if the byte completes a frame. The status is the following:

      - OK - A frame was successfully decoded. The Result contains the Frame,
        which is invalidated by the next Process call.
      - UNAVAILABLE - No frame is available.
      - RESOURCE_EXHAUSTED - A frame completed, but it was too large to fit in
        the decoder's buffer.
      - DATA_LOSS - A frame completed, but it was invalid. The frame was
        incomplete or the frame check sequence verification failed.

  .. cpp:function:: void Process(pw::ConstByteSpan data, F&& callback, Args&&... args)

    Processes a span of data and calls the provided callback with each frame or
    error.

This example demonstrates reading individual bytes from ``pw::sys_io`` and
decoding HDLC frames:

.. code-block:: cpp

  #include "pw_hdlc_lite/decoder.h"
  #include "pw_sys_io/sys_io.h"

  int main() {
    std::byte data;
    while (true) {
      if (!pw::sys_io::ReadByte(&data).ok()) {
        // Log serial reading error
      }
      Result<Frame> decoded_frame = decoder.Process(data);

      if (decoded_frame.ok()) {
        // Handle the decoded frame
      }
    }
  }

Python
^^^^^^
.. autoclass:: pw_hdlc_lite.decode.FrameDecoder
  :members:

Below is an example using the decoder class to decode data read from serial:

.. code-block:: python

  import serial
  from pw_hdlc_lite import decode

  ser = serial.Serial()
  decoder = decode.FrameDecoder()

  while True:
      for frame in decoder.process_valid_frames(ser.read()):
          # Handle the decoded frame

Additional features
===================

pw::stream::SysIoWriter
------------------------
The ``SysIoWriter`` C++ class implements the ``Writer`` interface with
``pw::sys_io``. This Writer may be used by the C++ encoder to send HDLC frames
over serial.

HdlcRpcClient
-------------
.. autoclass:: pw_hdlc_lite.rpc.HdlcRpcClient
  :members:

Roadmap
=======
- **Expanded protocol support** - ``pw_hdlc_lite`` currently only supports
  information frames with a single address byte and control byte. Support for
  different frame types and extended address or control fields may be added in
  the future.

- **Higher performance** - We plan to improve the overall performance of the
  decoder and encoder implementations by using SIMD/NEON.

Compatibility
=============
C++17
