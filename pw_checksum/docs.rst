.. default-domain:: cpp

.. highlight:: sh

-----------
pw_checksum
-----------
The pw_checksum module provides functions for calculating checksums.

pw_checksum/ccitt_crc16.h
=========================

.. cpp:namespace:: pw::checksum

.. cpp:var:: constexpr uint16_t kCcittCrc16DefaultInitialValue = 0xFFFF

  The default initial value for the CRC16.

.. cpp:function:: uint16_t CcittCrc16(span<const std::byte> data, uint16_t initial_value = kCcittCrc16DefaultInitialValue)

  Calculates the CRC16 of the provided data using polynomial 0x1021, with a
  default initial value of :cpp:expr:`0xFFFF`.

  To incrementally calculate a CRC16, use the previous value as the initial
  value.

  .. code-block:: cpp

    uint16_t crc = CcittCrc16(my_data);

    crc  = CcittCrc16(more_data, crc);

Compatibility
=============
* C
* C++17

Dependencies
============
* pw_span
