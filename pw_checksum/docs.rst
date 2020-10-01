.. _module-pw_checksum:

-----------
pw_checksum
-----------
The ``pw_checksum`` module provides functions for calculating checksums.

pw_checksum/crc16_ccitt.h
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

pw_checksum/crc32.h
===================

.. cpp:var:: constexpr uint32_t kCrc32InitialValue = 0xFFFFFFFF

  The initial value for the CRC32.

.. cpp:function:: uint32_t Crc32(span<const std::byte> data)

  Calculates the initial / one-time CRC32 of the provided data using polynomial
  0x4C11DB7, with an initial value of :cpp:expr:`0xFFFFFFFF`.

  .. code-block:: cpp

    uint32_t crc = Crc32(my_data);

.. cpp:function:: uint32_t Crc32(span<const std::byte> data, uint32_t previous_result)

  Incrementally append calculation of a CRC32, need to pass in the previous
  result.

  .. code-block:: cpp

    uint32_t crc = Crc32(my_data);
    crc = Crc32(more_data, crc);

Compatibility
=============
* C
* C++17

Dependencies
============
* ``pw_span``
