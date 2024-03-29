.. _module-pw_i2c:

======
pw_i2c
======
.. pigweed-module::
   :name: pw_i2c

.. tab-set::

   .. tab-item:: app.cpp

      .. include:: ../pw_i2c_rp2040/docs.rst
         :start-after: .. pw_i2c_rp2040-example-start
         :end-before: .. pw_i2c_rp2040-example-end

   .. tab-item:: BUILD.bazel

      .. code-block:: py

         cc_library(
           # ...
           deps = [
             # ...
             "@pigweed//pw_i2c:address",
             "@pigweed//pw_i2c:device",
             # ...
           ] + select({
             "@platforms//os:freertos": [
               "@pigweed//pw_i2c_rp2040:initiator",
             ],
             "//conditions:default": [
               # Fake example of a custom implementation.
               "//lib/pw_i2c_my_device:initiator",
             ],
           }),
         )

``pw_i2c`` provides C++ libraries and helpers for interacting with I2C
devices.

.. grid:: 2

   .. grid-item-card:: :octicon:`rocket` Quickstart
      :link: module-pw_i2c-quickstart
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      How to set up ``pw_i2c`` in your build system
      and interact with an I2C device via the C++ API.

   .. grid-item-card:: :octicon:`list-unordered` Guides
      :link: module-pw_i2c-guides
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      How to mock up I2C transactions, configure and read from a device's
      register, communicate with an I2C device over RPC, and more.

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` Reference
      :link: module-pw_i2c-reference
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      API references for ``pw::i2c::Initiator``, ``pw::i2c::Address``,
      ``pw::i2c::Device``, and more.

   .. grid-item-card:: :octicon:`stack` Implementations
      :link: module-pw_i2c-impl
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      A summary of the existing ``pw_i2c`` implementations and a guide
      on how to create your own.

.. toctree::
   :hidden:
   :maxdepth: 1

   guides
   reference
   backends
