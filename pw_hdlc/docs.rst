.. _module-pw_hdlc:

.. rst-class:: with-subtitle

=======
pw_hdlc
=======
.. pigweed-module::
   :name: pw_hdlc

   - **Simple**: Transmit RPCs and other data between devices over serial
   - **Robust**: Detect corruption and data loss
   - **Efficient**: Stream to transport without buffering

.. include:: design.rst
   :start-after: .. pw_hdlc-overview-start
   :end-before: .. pw_hdlc-overview-end

Encoding looks like this:

.. pw_hdlc-encoding-example-start

.. tab-set::

   .. tab-item:: C++
      :sync: cpp

      .. code-block:: cpp

         // Writes a span of data to a pw::stream::Writer and returns the status. This
         // implementation uses the pw_checksum module to compute the CRC-32 frame check
         // sequence.

         #include "pw_hdlc/encoder.h"
         #include "pw_hdlc/sys_io_stream.h"

         int main() {
           pw::stream::SysIoWriter serial_writer;
           Status status = pw::hdlc::WriteUIFrame(123 /* address */, data, serial_writer);
           if (!status.ok()) {
             PW_LOG_INFO("Writing frame failed! %s", status.str());
           }
         }

   .. tab-item:: Python
      :sync: py

      .. code-block:: python

         # Read bytes from serial and encode HDLC frames

         import serial
         from pw_hdlc import encode

         ser = serial.Serial()
         address = 123
         ser.write(encode.ui_frame(address, b'your data here!'))

.. pw_hdlc-encoding-example-end

And decoding looks like this:

.. pw_hdlc-decoding-example-start

.. tab-set::

   .. tab-item:: C++
      :sync: cpp

      .. code-block:: cpp

         // Read individual bytes from pw::sys_io and decode HDLC frames.

         #include "pw_hdlc/decoder.h"
         #include "pw_sys_io/sys_io.h"

         int main() {
           std::byte data;
           std::array<std::byte, 128> decode_buffer;
           pw::hdlc::Decoder decoder(decode_buffer);
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

   .. tab-item:: Python
      :sync: py

      .. code-block:: python

         # Decode data read from serial

         import serial
         from pw_hdlc import decode

         ser = serial.Serial()
         decoder = decode.FrameDecoder()

         while True:
             for frame in decoder.process_valid_frames(ser.read()):
                 # Handle the decoded frame

.. pw_hdlc-decoding-example-end

.. pw_hdlc-nav-start

.. grid:: 1

   .. grid-item-card:: :octicon:`rocket` Get started & guides
      :link: module-pw_hdlc-guide
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      How to set up and use ``pw_hdlc``

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` API reference
      :link: module-pw_hdlc-api
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Reference details about the ``pw_hdlc`` API

   .. grid-item-card:: :octicon:`stack` Design
      :link: module-pw_hdlc-design
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Design details about ``pw_hdlc``

.. grid:: 2

   .. grid-item-card:: :octicon:`graph` Code size analysis
      :link: module-pw_hdlc-size
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      The code size impact of ``pw_hdlc``

   .. grid-item-card:: :octicon:`list-ordered` RPC over HDLC example
      :link: module-pw_hdlc-rpc-example
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      A step-by-step example of sending RPCs over HDLC

.. pw_hdlc-nav-end

.. toctree::
   :maxdepth: 1
   :hidden:

   guide
   api
   design
   size
   rpc_example/docs
