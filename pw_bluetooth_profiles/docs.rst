.. _module-pw_bluetooth_profiles:

=====================
pw_bluetooth_profiles
=====================

.. attention::

   :bdg-ref-primary-line:`module-pw_bluetooth_profiles` is under construction,
   depends on the experimental :bdg-ref-primary-line:`module-pw_bluetooth`
   module and may see significant breaking API changes.

The ``pw_bluetooth_profiles`` module provides a collection of implementations
for basic Bluetooth profiles built on top of the ``pw_bluetooth`` module API.
These profiles are independent from each other

--------------------------
Device Information Service
--------------------------
The ``device_info_service`` target implements the Device Information Service
(DIS) as defined in the specification version 1.1. It exposes up to nine
different basic properties of the device such as the model, manufacturer or
serial number, all of which are optional. This module implements the GATT
server-side service (``bluetooth::gatt::LocalServiceDelegate``) with the
following limitations:

- The subset of properties exposed and their values are constant throughout the
  life of the service.
- The subset of properties is defined at compile time, but the values may be
  defined at runtime before service initialization. For example, the serial
  number property might be different for different devices running the same
  code.
- All property values must be available in memory while the service is
  published. Rather than using a callback mechanism to let the user produce the
  property value at run-time, this module expects those values to be readily
  available when initialized, but they can be stored in read-only memory.

Usage
-----
The main intended usage of the service consists on creating and publishing the
service, leaving it published forever referencing the values passed on
initialization.

The subset of properties exposed is a template parameter bit field
(``DeviceInfo::Field``) and can't be changed at run-time. The ``pw::span``
values referenced in the ``DeviceInfo`` struct must remain available after
initialization to avoid copying them to RAM in the service, but the
``DeviceInfo`` struct itself can be destroyed after initialization.

Example code:

.. code-block:: cpp

   using pw::bluetooth_profiles::DeviceInfo;
   using pw::bluetooth_profiles::DeviceInfoService;

   // Global serial number for the device, initialized elsewhere.
   pw::InlineString serial_number(...);

   // Select which fields to expose at compile-time with a constexpr template
   // parameter.
   constexpr auto kUsedFields = DeviceInfo::Field::kModelNumber |
                                DeviceInfo::Field::kSerialNumber |
                                DeviceInfo::Field::kSoftwareRevision;

   // Create a DeviceInfo with the values. Values are referenced from the
   // service, not copied, so they must remain available while the service is
   // published.
   DeviceInfo device_info = {};
   device_info.model_number = pw::as_bytes(pw::span{"My Model"sv});
   device_info.software_revision = pw::as_bytes(pw::span{REVISION_MACRO});
   device_info.serial_number = pw::as_bytes(
       pw::span(serial_number.data(), serial_number.size()));

   DeviceInfoService<kUsedFields, pw::bluetooth::gatt::Handle{123}>
       device_info_service{device_info};
   device_info_service.PublishService(...);

