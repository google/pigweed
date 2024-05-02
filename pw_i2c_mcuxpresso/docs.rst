.. _module-pw_i2c_mcuxpresso:

=================
pw_i2c_mcuxpresso
=================
.. pigweed-module::
   :name: pw_i2c_mcuxpresso

``pw_i2c_mcuxpresso`` implements the ``pw_i2c`` interface using the
NXP MCUXpresso SDK.

The implementation is based on the i2c driver in SDK. I2C transfers use
non-blocking driver API.

-----
Setup
-----
This module requires following setup:

1. Use ``pw_build_mcuxpresso`` to create a ``pw_source_set`` for an
   MCUXpresso SDK.
2. Include the i2c driver component in this SDK definition.
3. Specify the ``pw_third_party_mcuxpresso_SDK`` GN global variable to specify
   the name of this source set.
4. Use ``pw::i2c::McuxpressoInitiator`` implementation of
   ``pw::i2c::Initiator`` while creating ``pw::i2c::Device`` or
   ``pw::i2c::RegisterDevice`` interface to access the I2C devices connected to
   target.

-----
Usage
-----
.. code-block:: cpp

   constexpr uint32_t kI2CBaudRate = 100000;
   constexpr McuxpressoInitiator::Config kConfig = {
       .flexcomm_address = I2C11_BASE,
       .clock_name = kCLOCK_Flexcomm11Clk,
       .baud_rate_bps = kI2CBaudRate,
   };
   McuxpressoInitiator initiator{kConfig};
   initiator.Enable();
