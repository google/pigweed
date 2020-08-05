.. _chapter-pw-hdlc:

.. default-domain:: cpp

.. highlight:: sh

------------
pw_hdlc_lite
------------
pw_hdlc_lite is a module that enables serial communication between devices
using the HDLC-Lite protocol.

Compatibility
=============
C++17

Dependencies
============
* ``pw_bytes``
* ``pw_log``
* ``pw_preprocessor``
* ``pw_result``
* ``pw_rpc``
* ``pw_status``
* ``pw_span``
* ``pw_stream``
* ``pw_sys_io``

HDLC-Lite Overview
==================
High-Level Data Link Control (HDLC) is a data link layer protocol which uses
synchronous serial transmissions for communication between two devices. Unlike
the standard HDLC protocol which uses six fields of embedded information, the
HDLC-Lite protocol is a minimal version that only uses the bare essentials.

The HDLC-Lite data frame in ``pw_hdlc_lite`` uses a start and end frame
delimiter (0x7E), the escaped binary payload and the CCITT-CRC16 value.
It looks like:

.. code-block:: text

                                        [More frames]
    _________________________________________   _______
    | |                              |  | | |...|   | |
    | |                              |  | | |...|   | |
    |_|______________________________|__|_|_|...|___|_|
     F         Payload               CRC F F     CRC F

Basic Overview
==============
The ``pw_hdlc_lite`` module provides a simple, reliable packet-oriented
transport that uses the HDLC-Lite protocol to send and receive data to and from
embedded devices. This is especially needed for making RPC calls on devices
during testing because the ``pw_rpc`` module does not handle the transmission of
RPCs. This module enables the transmission of RPCs and other bytes through a
serial connection.

There are essentially two main functions of the ``pw_hdlc_lite`` module:

  * **Encoding** the data by escaping the bytes of the payload, calculating the
    CCITT-CRC16 value, constructing a data frame and sending the
    resulting data packet through serial.
  * **Decoding** the data by unescaping the received bytes, verifying the
    CCITT-CRC16 value and returning the successfully decoded packets.

**Why use the ``pw_hdlc_lite`` module?**

  * Enables the transmission of RPCs and other data between devices over serial
  * Resilient to corruption and data loss.
  * Light-weight, simple and easy to use.
  * Supports streaming to transport without buffering - e.g. protocol buffers
    have length-prefix.

Protocol Description
====================

Encoding and sending data
-------------------------
This module first writes an initial frame delimiter byte (0x7E) to indicate the
beginning of the frame. Before sending any of the payload data through serial,
the special bytes are escaped accordingly:

            +-----------------------+----------------------+
            |Unescaped Special Bytes| Escaped Special Bytes|
            +=======================+======================+
            |       0x7E            |       0x7D5E         |
            +-----------------------+----------------------+
            |       0x7D            |       0x7D5D         |
            +-----------------------+----------------------+

The bytes of the payload are escaped and written in a single pass. The
CCITT-CRC16 value is calculated, escaped and written after. After this, a final
frame delimiter byte (0x7E) is written to mark the end of the frame.

Decoding received bytes
-----------------------
Packets may be received in multiple parts, so we need to store the received data
in a buffer until the ending frame delimiter (0x7E) is read. When the
pw_hdlc_lite decoder receives data, it unescapes it and adds it to a buffer.
When the frame is complete, it calculates and verifies the CCITT-CRC16 bytes and
does the following:

* If correctly verified, the decoder returns the decoded packet.
* If the checksum verification fails, the data packet is discarded.

During the decoding process, the decoder essentially transitions between 3
states, where each state indicates the method of decoding that particular byte:

NO_PACKET --> PACKET_ACTIVE --> (ESCAPE | NO_PACKET).

API Usage
=========

Encoder
-------
The Encoder API invloves a single function that encodes the data using the
HDLC-Lite protocol and sends the data through the serial.

C++
^^^
In C++, this function is called ``EncodeAndWritePayload`` and it accepts a
ConstByteSpan called payload and an object of type Writer& as arguments. It
returns a Status object that indicates if the write was successful. This
implementation uses the ``pw_checksum`` module to compute the CRC16 value. Since
the function writes a starting and ending frame delimiter byte at the beginnning
and the end of frames, it is safe to encode multiple spans. The usage of this
function is as follows:

.. code-block:: cpp

  #include "pw_hdlc_lite/encoder.h"
  #include "pw_hdlc_lite/sys_io_stream.h"

  int main() {
      pw::stream::SerialWriter serial_writer;
      constexpr std::array<byte, 1> test_array = { byte(0x41) };
      auto status = EncodeAndWritePayload(test_array, serial_writer);
  }

In the example above, we expect the encoder to send the following bytes:

- **0x7E** - Initial Frame Delimiter
- **0x41** - Payload
- **0x15** - LSB of the CCITT-CRC16 value
- **0xB9** - MSB of the CCITT-CRC16 value
- **0x7E** - End Frame Delimiter

Python
^^^^^^
In Python, the function is called ``encode_and_write_payload`` and it accepts
the payload as a byte object and a callable to which is used to write the data.
This function does not return anything, and uses the binascii library function
called crc_hqx to compute the CRC16 bytes. Like the C++ function, the Python
function also writes a frame delimiter at the beginnning and the end of frames
so it is safe to encode multiple spans consecutively. The usage of this function
is as follows:

.. code-block:: python

  import serial
  from pw_hdlc_lite import encoder

  ser = serial.Serial()
  encoder.encode_and_write_payload(b'A', ser.write)

We expect this example to give us the same result as the C++ example above since
it encodes the same payload.

Decoder
-------
The Decoder API involves a Decoder class whose main functionality is a function
that unescapes the received bytes, adds them to a buffer and returns the
successfully decoded packets. A class is used so that the user can call the
decoder object's adding bytes functionality on the currently received bytes
instead of waiting on the entire packet to arrive.

C++
^^^
The main functionality of the C++ ``Decoder`` class is the 'AddByte' function
which accepts a single byte as an argument, unescapes it and adds it to the
buffer. If the byte is the ending frame delimiter flag (0x7E) it attempts to
decode the packet and returns a ``Result`` object indicating the success of the
operation:

  * The returned ``pw::Result`` object will have status ``Status::OK`` and
    value ConstByteSpan containing the most recently decoded packet if it finds
    the end of the data-frame and successfully verifies the CRC.
  * The returned ``pw::Result`` object will have status ``Status::UNAVAILABLE``
    if it doesnt find the end of the data-frame during that function call.
  * The returned ``pw::Result`` object will have status ``Status::DATA_LOSS``
    if it finds the end of the data-frame, but the CRC-verification fails. It
    also returns this status if the packet in question does not have the
    2-byte CRC in it.
  * The returned ``pw::Result`` object will have status
    ``Status::RESOURCE_EXHAUSTED`` if the decoder buffer runs out of space.

Here's a C++ example of reading individual bytes from serial and then using the
decoder object to decode the received data:

.. code-block:: cpp

  #include "pw_hdlc_lite/decoder.h"
  #include "pw_sys_io/sys_io.h"

  int main() {
    byte data;
    while (true) {
      if (!pw::sys_io::ReadByte(&data).ok()) {
        // Log serial reading error
      }
      auto decoded_packet = decoder.AddByte(data);

      if (decoded_packet.ok()) {
       // Use decoded_packet to get access to the most recently decoded packet
      }
    }
  }

Python
^^^^^^
The main functionality of the Python ``Decoder`` class is the ``add_bytes``
generator which unescapes the bytes object argument and adds them to a buffer
until it encounters the ending frame delimiter (0x7E) flag. The generator yields
the decoded packets as byte objects upon successfully verification of the CRC16
value of the received bytes. If the CRC verification fails it raises a
CrcMismatchError exception.

Below is an example of the usage of the decoder class to decode bytes read from
serial:

.. code-block:: python

  import serial
  from pw_hdlc_lite import decoder

  ser = serial.Serial()
  decode = decoder.Decoder()

  while true:
    byte = ser.read(1)

    for decoded_packet in decode.add_bytes(byte):
      # Do something with the decoded packet

Like the C++ example, this reads individual bytes and adds them to the decoder.

Features
========

pw::stream::SerialWriter
------------------------
The ``SerialWriter`` class implements the ``Writer`` interface by using sys_io
to write data over a serial connection. This Writer object is used by the C++
encoder to send the encoded bytes to the device.

Roadmap & Status
================

- **Additional fields** - As it currently stands, ``pw_hdlc_lite`` uses only
  three fields of control bytes: starting frame delimiter, 2-byte CRC and an
  ending frame delimiter. However, if we decided to send larger, more
  complicated RPCs and if wanted to stream log debug and error messages, we will
  require additional fields of data to ensure the different packets are sent
  correctly. Thus, in the future, we plan to add additional channel and sequence
  byte fields that could enable separate channels for pw_rpc, QoS etc.

- **Higher performance** - We plan to improve the overall performance of the
  decoder and encoder implementations by using SIMD/NEON.
