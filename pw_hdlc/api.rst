.. _module-pw_hdlc-api:

======================
pw_hdlc: API reference
======================
.. pigweed-module-subpage::
   :name: pw_hdlc
   :tagline: Lightweight, simple, and easy serial communication

This page describes the :ref:`module-pw_hdlc-api-encoder`, :ref:`module-pw_hdlc-api-decoder`,
and :ref:`module-pw_hdlc-api-rpc` APIs of ``pw_hdlc``.

.. _module-pw_hdlc-api-encoder:

Encoder
=======
The Encoder API provides a single function that encodes data as an HDLC
unnumbered information frame.

.. tab-set::

   .. tab-item:: C++

      .. doxygenfunction:: pw::hdlc::WriteUIFrame(uint64_t address, ConstByteSpan data, stream::Writer &writer)

      Example:

      .. TODO: b/279648188 - Share this code between api.rst and guide.rst.

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

   .. tab-item:: Python

      .. automodule:: pw_hdlc.encode
         :members:
         :noindex:

      Example:

      .. TODO: b/279648188 - Share this code between api.rst and guide.rst.

      .. code-block:: python

         # Read bytes from serial and encode HDLC frames

         import serial
         from pw_hdlc import encode

         ser = serial.Serial()
         address = 123
         ser.write(encode.ui_frame(address, b'your data here!'))

   .. tab-item:: TypeScript

      The Encoder class provides a way to build complete, escaped HDLC UI frames.

      .. js:method:: Encoder.uiFrame(address, data)
         :noindex:

         :param number address: frame address.
         :param Uint8Array data: frame data.
         :returns: Uint8Array containing a complete HDLC frame.

.. _module-pw_hdlc-api-decoder:

Decoder
=======


.. tab-set::

   .. tab-item:: C++

      .. doxygenclass:: pw::hdlc::Decoder
         :members:

      Example:

      .. TODO: b/279648188 - Share this code between api.rst and guide.rst.

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

   .. tab-item:: Python

      .. autoclass:: pw_hdlc.decode.FrameDecoder
         :members:
         :noindex:

      Example:

      .. TODO: b/279648188 - Share this code between api.rst and guide.rst.

      .. code-block:: python

         # Decode data read from serial

         import serial
         from pw_hdlc import decode

         ser = serial.Serial()
         decoder = decode.FrameDecoder()

         while True:
             for frame in decoder.process_valid_frames(ser.read()):
                 # Handle the decoded frame

      It is possible to decode HDLC frames from a stream using different protocols or
      unstructured data. This is not recommended, but may be necessary when
      introducing HDLC to an existing system.

      The ``FrameAndNonFrameDecoder`` Python class supports working with raw data and
      HDLC frames in the same stream.

      .. autoclass:: pw_hdlc.decode.FrameAndNonFrameDecoder
        :members:
        :noindex:

   .. tab-item:: TypeScript

      The decoder class unescapes received bytes and adds them to a buffer. Complete,
      valid HDLC frames are yielded as they are received.

      .. js:method:: Decoder.process(data)
         :noindex:

         :param Uint8Array data: bytes to be decoded.
         :yields: HDLC frames, including corrupt frames.
                  The Frame.ok() method whether the frame is valid.

      .. js:method:: processValidFrames(data)
         :noindex:

         :param Uint8Array data: bytes to be decoded.
         :yields: Valid HDLC frames, logging any errors.

.. _module-pw_hdlc-api-rpc:

RPC
===

.. tab-set::

   .. tab-item:: C++

      .. autoclass:: pw_hdlc.rpc.HdlcRpcClient
         :members:
         :noindex:

      .. autoclass:: pw_hdlc.rpc.HdlcRpcLocalServerAndClient
         :members:
         :noindex:

      The ``RpcChannelOutput`` implements pw_rpc's ``pw::rpc::ChannelOutput``
      interface, simplifying the process of creating an RPC channel over HDLC. A
      ``pw::stream::Writer`` must be provided as the underlying transport
      implementation.

      If your HDLC routing path has a Maximum Transmission Unit (MTU) limitation,
      using the ``FixedMtuChannelOutput`` is strongly recommended to verify that the
      currently configured max RPC payload size (dictated by pw_rpc's static encode
      buffer) will always fit safely within the limits of the fixed HDLC MTU *after*
      HDLC encoding.

.. _module-pw_hdlc-py:

----------------------
pw_hdlc Python package
----------------------
The ``pw_hdlc`` Python package includes utilities to HDLC-encode and decode RPC
packets, with examples of RPC Client implementations in Python. It also provides
abstractions for interfaces used to receive RPC Packets.

pw_hdlc.rpc
===========
.. automodule:: pw_hdlc.rpc
  :members:
    channel_output,
    CancellableReader,
    SelectableReader,
    SocketReader,
    SerialReader,
    DataReaderAndExecutor,
    default_channels,
    RpcClient,
    HdlcRpcClient,
    NoEncodingSingleChannelRpcClient,
    SocketSubprocess,
    HdlcRpcLocalServerAndClient

