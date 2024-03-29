.. TODO: b/331286149 - Rename this file

.. _module-pw_i2c-impl:

===============
Implementations
===============
.. pigweed-module-subpage::
   :name: pw_i2c

To use the ``pw_i2c`` API, you need to specify in your build system an
implementation of the ``pw_i2c`` virtual interface.

------------------------
Existing implementations
------------------------
The following Pigweed modules are ready-made ``pw_i2c`` implementations that
you can use in your projects:

* :ref:`module-pw_i2c_linux` for Linux userspace.
* :ref:`module-pw_i2c_mcuxpresso` for the NXP MCUXpresso SDK.
* :ref:`module-pw_i2c_rp2040` for the Raspberry Pi Pico SDK.

See :ref:`module-pw_i2c-quickstart` for build system configuration examples.

------------------------------
Create your own implementation
------------------------------
If the available implementations don't meet your needs, you can create your
own:

.. _common_pico.cc: https://pigweed.googlesource.com/pigweed/kudzu/+/refs/heads/main/applications/app_common_impl/common_pico.cc

#. Implement the :cpp:class:`pw::i2c::Initiator` interface. See
   :ref:`module-pw_i2c_rp2040` for an example Raspberry Pi Pico SDK
   implementation and `common_pico.cc`_ for example usage of ``pw_i2c_rp2040``.

#. In your build system, take a dependency on the ``pw_i2c`` API headers. See
   :ref:`module-pw_i2c-quickstart` for build system configuration examples.

.. toctree::
   :maxdepth: 1
   :hidden:

   Linux <../pw_i2c_linux/docs>
   MCUXpresso <../pw_i2c_mcuxpresso/docs>
   Pico SDK <../pw_i2c_rp2040/docs>
