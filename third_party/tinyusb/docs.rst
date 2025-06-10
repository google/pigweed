.. _module-pw_third_party_tinyusb:

=======
TinyUSB
=======

The ``$pw_external_tinyusb/`` module contains the build files needed to
integrate TinyUSB into a Pigweed build.

-------------
Build Support
-------------
This module provides support to compile TinyUSB with GN and Bazel, however it
doesn't include the source code of the
`tinyusb <https://github.com/hathach/tinyusb>`_ project.

GN
==
In order to use this you are expected to configure the following variables from
``$pw_external_tinyusb:tinyusb.gni``:

#. Set the GN ``pw_third_party_tinyusb_SOURCE`` to the path of the TinyUSB
   source code directory. This is the directory that contains the ``src/``
   sub-directory.
#. Set ``pw_third_party_tinyusb_CONFIG`` to a ``pw_source_set`` which provides
   the TinyUSB ``tusb_config.h`` config header. While it is possible to provide
   public definitions in this ``pw_source_set`` the configuration header file is
   still required by TinyUSB.
#. Set ``pw_third_party_tinyusb_PORT`` to a ``pw_source_set`` which provides
   the TinyUSB port sources for the specific MCU. Several MCUs are supported by
   the upstream TinyUSB project, check the ``src/portable/`` path for your MCU.

After this is done multiple ``pw_source_set`` entries for the different parts of
TinyUSB library are created at ``$pw_external_tinyusb``.

Bazel
=====
The TinyUSB build is configured through `constraint_settings
<https://bazel.build/reference/be/platforms-and-toolchains#constraint_setting>`_.
The `platform <https://bazel.build/extending/platforms>`_ you are building for
must specify values for the following settings:

*   ``@tinyusb//:port``, to set which TinyUSB port to use. You can
    select a value from those defined in
    ``third_party/tinyusb/tinyusb.BUILD.bazel`` (for example,
    ``@tinyusb//:port_synopsys_stm32``).
*   ``@tinyusb//:tinyusb_config`` label flag to point to the library target
    providing the TinyUSB config header.
*   ``@tinyusb//:tinyusb_descriptors`` label flag to point to the library target
    providing the TinyUSB descriptors.

See :ref:`docs-build_system-bazel_configuration` for a discussion of how to work
with our label flags.

.. _third_party-tinyusb_classes:

Adding TinyUSB dependency
=========================
TinyUSB library is split into a device side and host side, with a few common
sources between the two. It is possible to depend on both at compile time, but
initialize only one side. Device and host both provide classes on top such as
Communications Device Class (CDC) or Mass Storage Class (MSC) which can be
selectively included in the build by depending on the respective GN/Bazel
targets.
