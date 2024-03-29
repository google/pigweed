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
* :cpp:class:`pw::i2c::Initiator` is the common, base driver interface for
  communicating with I2C devices.
* :cpp:class:`pw::i2c::Device` is a helper class that takes a reference
  to an :cpp:class:`pw::i2c::Initiator` instance and provides easier access
  to a single I2C device.
* :cpp:class:`pw::i2c::RegisterDevice` extends :cpp:class:`pw::i2c::Device`
  for easier access to a single I2C device's registers.
* :cpp:class:`pw::i2c::I2cService` is a service for performing I2C
  transactions over RPC.
* :cpp:class:`pw::i2c::MockInitiator` is a generic mocked backend for
  :cpp:class:`pw::i2c::Initiator`. It uses :cpp:class:`pw::i2c::Transaction`
  to represent expected I2C transactions.

--------------------
``pw::i2c::Address``
--------------------
.. doxygenclass:: pw::i2c::Address
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

--------------------------
``pw::i2c::MockInitiator``
--------------------------
.. doxygenclass:: pw::i2c::MockInitiator
   :members:

``pw::i2c::Transaction``
========================
.. doxygenclass:: pw::i2c::Transaction
   :members:

``pw::i2c::ReadTransaction``
============================
.. doxygenfunction:: pw::i2c::ReadTransaction

``pw::i2c::WriteTransaction``
=============================
.. doxygenfunction:: pw::i2c::WriteTransaction

``pw::i2c::ProbeTransaction``
=============================
.. doxygenfunction:: pw::i2c::ProbeTransaction

---------------------------
``pw::i2c::GmockInitiator``
---------------------------
.. doxygenclass:: pw::i2c::GmockInitiator
   :members:
