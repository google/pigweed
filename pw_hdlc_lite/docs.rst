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
* ``pw_preprocessor``
* ``pw_status``
* ``pw_span``
* ``pw_sys_io``
* ``pw_stream``

Features
========

pw::stream::SerialWriter
------------------------
The ``SerialWriter`` class implements the ``Writer`` interface by using sys_io
to write data over a communication channel.


Future work
^^^^^^^^^^^
- Adding the code for the Encoder and Decoder.
