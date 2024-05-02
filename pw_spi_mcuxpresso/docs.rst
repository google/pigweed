.. _module-pw_spi_mcuxpresso:

=================
pw_spi_mcuxpresso
=================
``pw_spi_mcuxpresso`` implements the :ref:`module-pw_spi` interface using the
NXP MCUXpresso SDK.

There are two implementations corresponding to the SPI and FLEXIO_SPI drivers in the
SDK. SPI transfer can be configured to use a blocking (by polling) method or
non-blocking under the covers. The API is synchronous regardless.

-----
Setup
-----
Use of this module requires setting up the MCUXpresso SDK for use with Pigweed. Follow
the steps in :ref:`module-pw_build_mcuxpresso` to create a ``pw_source_set`` for an
MCUXpresso SDK. Include the GPIO and PINT driver components in this SDK definition.

This example shows what your SDK setup would look like if using an RT595 EVK.

.. code-block:: text

   import("$dir_pw_third_party/mcuxpresso/mcuxpresso.gni")

   pw_mcuxpresso_sdk("sample_project_sdk") {
     manifest = "$dir_pw_third_party/mcuxpresso/evkmimxrt595/EVK-MIMXRT595_manifest_v3_8.xml"
     include = [
       "component.serial_manager_uart.MIMXRT595S",
       "platform.drivers.flexio_spi.MIMXRT595S",
       "platform.drivers.flexspi.MIMXRT595S",
       "project_template.evkmimxrt595.MIMXRT595S",
       "utility.debug_console.MIMXRT595S",
     ]
   }

Next, specify the ``pw_third_party_mcuxpresso_SDK`` GN global variable to specify
the name of this source set. Edit your GN args with ``gn args out``.

.. code-block:: text

   pw_third_party_mcuxpresso_SDK = "//targets/mimxrt595_evk:sample_project_sdk"

Then, depend on this module in your BUILD.gn to use.

.. code-block:: text

   deps = [ dir_pw_spi_mcuxpresso ]

-------
Example
-------
Example write using the FLEXIO_SPI initiator:

.. code-block:: text

   McuxpressoFlexIoInitiator spi(
      flexio_spi_config, CLOCK_GetFlexioClkFreq(), baud_rate_bps, blocking);
   spi.Configure(configuration);

   spi.WriteRead(source, destination);
