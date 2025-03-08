.. _module-pw_i2c-reference:

=========
Reference
=========
.. pigweed-module-subpage::
   :name: pw_i2c

.. _module-pw_i2c-reference-overview:

--------
Overview
--------
* :cpp:class:`pw::i2c::Address` is a helper class for representing I2C
  addresses.
* :cpp:class:`pw::i2c::Message` is a helper class for representing individual
  read and write components within a single i2c transaction.
* :cpp:class:`pw::i2c::Initiator` is the common, base driver interface for
  communicating with I2C devices.
* :cpp:class:`pw::i2c::Device` is a helper class that takes a reference
  to an :cpp:class:`pw::i2c::Initiator` instance and provides easier access
  to a single I2C device.
* :cpp:class:`pw::i2c::RegisterDevice` extends :cpp:class:`pw::i2c::Device`
  for easier access to a single I2C device's registers.
* :cpp:class:`pw::i2c::I2cService` is a service for performing I2C
  transactions over RPC.
* :cpp:class:`pw::i2c::MockMessageInitiator` is a generic mocked backend for
  :cpp:class:`pw::i2c::Initiator`. It accepts multiple
  :cpp:class:`pw::i2c::MockMessageTransaction`, each of which is mock
  transmitted as one bus transaction.
* :cpp:class:`pw::i2c::MockMessageTransaction` represents a test i2c
  transaction. Each transaction consists of an arbitrary sequence of
  :cpp:class:`pw::i2c::MockMessage` objects that are transmitted in one bus
  operation.
* :cpp:class:`pw::i2c::MockMessage` represents one read or write element of
  an i2c transaction.

--------------------
``pw::i2c::Address``
--------------------
.. doxygenclass:: pw::i2c::Address
   :members:

--------------------
``pw::i2c::Message``
--------------------
.. doxygenclass:: pw::i2c::Message
   :members:

----------------------
``pw::i2c::Initiator``
----------------------
.. doxygenclass:: pw::i2c::Initiator
   :members:

-------------------
``pw::i2c::Device``
-------------------
.. doxygenclass:: pw::i2c::Device
   :members:

---------------------------
``pw::i2c::RegisterDevice``
---------------------------
See :ref:`module-pw_i2c-guides-registerdevice` for example usage of
``pw::i2c::RegisterDevice``.

.. doxygenclass:: pw::i2c::RegisterDevice
   :members:

-----------------------
``pw::i2c::I2cService``
-----------------------
.. doxygenclass:: pw::i2c::I2cService
   :members:

---------------------------------
``pw::i2c::MockMessageInitiator``
---------------------------------
.. doxygenclass:: pw::i2c::MockMessageInitiator
   :members:

``pw::i2c::MockMessageTransaction``
===================================
.. doxygenclass:: pw::i2c::MockMessageTransaction
   :members:

``pw::i2c::MockMessage``
===================================
.. doxygenclass:: pw::i2c::MockMessage
   :members:

``pw::i2c::MockReadMessage``
============================
.. doxygenfunction:: pw::i2c::MockReadMessage

``pw::i2c::MockWriteMessage``
=============================
.. doxygenfunction:: pw::i2c::MockWriteMessage

``pw::i2c::MockProbeMessage``
=============================
.. doxygenfunction:: pw::i2c::MockProbeMessage

---------------------------
``pw::i2c::GmockInitiator``
---------------------------
.. doxygenclass:: pw::i2c::GmockInitiator
   :members:
