.. _module-pw_hdlc-guide:

=====================
pw_hdlc: How-to guide
=====================
.. pigweed-module-subpage::
   :name: pw_hdlc
   :tagline: Lightweight, simple, and easy serial communication

This page shows you how to use the :ref:`module-pw_hdlc-api-encoder` and
:ref:`module-pw_hdlc-api-decoder` APIs of :ref:`module-pw_hdlc`.

--------
Encoding
--------
.. tabs::

   .. group-tab:: C++

      ..
        TODO(b/279648188): Share this code between api.rst and guide.rst.

      .. code-block:: cpp

         // Writes a span of data to a pw::stream::Writer and returns the status. This
         // implementation uses the pw_checksum module to compute the CRC-32 frame check
         // sequence.

         #include "pw_hdlc/encoder.h"
         #include "pw_hdlc/sys_io_stream.h"

         int main() {
           pw::stream::SysIoWriter serial_writer;
           Status status = WriteUIFrame(123 /* address */, data, serial_writer);
           if (!status.ok()) {
             PW_LOG_INFO("Writing frame failed! %s", status.str());
           }
         }

   .. group-tab:: Python

      ..
        TODO(b/279648188): Share this code between api.rst and guide.rst.

      .. code-block:: python

         # Read bytes from serial and encode HDLC frames

         import serial
         from pw_hdlc import encode

         ser = serial.Serial()
         address = 123
         ser.write(encode.ui_frame(address, b'your data here!'))

Allocating buffers when encoding
================================
.. tabs::

   .. group-tab:: C++

      Since HDLC's encoding overhead changes with payload size and what data is being
      encoded, this module provides helper functions that are useful for determining
      the size of buffers by providing worst-case sizes of frames given a certain
      payload size and vice-versa.

      .. code-block:: cpp

         #include "pw_assert/check.h"
         #include "pw_bytes/span.h"
         #include "pw_hdlc/encoder"
         #include "pw_hdlc/encoded_size.h"
         #include "pw_status/status.h"

         // The max on-the-wire size in bytes of a single HDLC frame after encoding.
         constexpr size_t kMtu = 512;
         constexpr size_t kRpcEncodeBufferSize = pw::hdlc::MaxSafePayloadSize(kMtu);
         std::array<std::byte, kRpcEncodeBufferSize> rpc_encode_buffer;

         // Any data encoded to this buffer is guaranteed to fit in the MTU after
         // HDLC encoding.
         pw::ConstByteSpan GetRpcEncodeBuffer() {
           return rpc_encode_buffer;
         }

--------
Decoding
--------
.. tabs::

   .. group-tab:: C++

      ..
        TODO(b/279648188): Share this code between api.rst and guide.rst.

      .. code-block:: cpp

         // Read individual bytes from pw::sys_io and decode HDLC frames.

         #include "pw_hdlc/decoder.h"
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

   .. group-tab:: Python

      ..
        TODO(b/279648188): Share this code between api.rst and guide.rst.

      .. code-block:: python

         # Decode data read from serial

         import serial
         from pw_hdlc import decode

         ser = serial.Serial()
         decoder = decode.FrameDecoder()

         while True:
             for frame in decoder.process_valid_frames(ser.read()):
                 # Handle the decoded frame

Allocating buffers when decoding
================================
.. tabs::

   .. group-tab:: C++

      The HDLC ``Decoder`` has its own helper for allocating a buffer since it doesn't
      need the entire escaped frame in-memory to decode, and therefore has slightly
      lower overhead.

      .. code-block:: cpp

         #include "pw_hdlc/decoder.h"

         // The max on-the-wire size in bytes of a single HDLC frame after encoding.
         constexpr size_t kMtu = 512;

         // Create a decoder given the MTU constraint.
         constexpr size_t kDecoderBufferSize =
             pw::hdlc::Decoder::RequiredBufferSizeForFrameSize(kMtu);
         pw::hdlc::DecoderBuffer<kDecoderBufferSize> decoder;
