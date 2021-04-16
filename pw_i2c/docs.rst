.. _module-pw_i2c:

------
pw_i2c
------

.. warning::
  This module is under construction, not ready for use, and the documentation
  is incomplete.

pw_i2c contains interfaces and utility functions for using I2C.

Features
========

pw::i2c::Initiator
------------------
The common interface for initiating transactions with devices on an I2C bus.
Other documentation sources may call this style of interface an I2C "master",
"central" or "controller".

pw::i2c::Device
---------------
The common interface for interfacing with generic I2C devices. This object
contains ``pw::i2c::Address`` and wraps the ``pw::i2c::Initiator`` API.
Common use case includes streaming arbitrary data (Read/Write). Only works
with devices with a single device address.

pw::i2c::RegisterDevice
-----------------------
The common interface for interfacing with register devices. Contains methods
to help read and write registers from and to the device. Users should have a
understanding of the capabilities of their device such as register address
sizes, register data sizes, byte addressability, bulk transactions, etc in
order to effectively use this interface.
