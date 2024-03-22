.. _module-pw_bluetooth_proxy:

==================
pw_bluetooth_proxy
==================
.. pigweed-module::
   :name: pw_bluetooth_proxy

The ``pw_bluetooth_proxy`` module provides a lightweight proxy host that
can be placed between a Bluetooth host and Bluetooth controller to add
additional functionality or inspection. All without modifying the host or the
controller.

An example use case could be offloading some functionality from a main host
located on the application processor to instead be handled on the MCU (to reduce
power usage).

The proxy acts as a proxy of all host controller interface (HCI) packets between
the host and the controller. Policies can then be added to the proxy to view,
insert, or modify HCI packets to provide additional functionality.

:cpp:class:`pw::bluetooth::proxy::HciProxy` acts as the main coordinator for
proxy functionality. To use it, instantiate it with
:cpp:class:`pw::bluetooth::proxy::ProxyPolicy` instances that provide the proxy
behavior you want. Then pass all HCI packets through the proxy.

.. literalinclude:: passthrough_test.cc
   :language: cpp
   :start-after: [pw_bluetooth_proxy-examples-basic]
   :end-before: [pw_bluetooth_proxy-examples-basic]

.. grid:: 2

   .. grid-item-card:: :octicon:`rocket` Get Started
      :link: module-pw_bluetooth_proxy-getstarted
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      How to set up in your build system

   .. grid-item-card:: :octicon:`code-square` API Reference
      :link: module-pw_bluetooth_proxy-reference
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Reference information about the API

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` Roadmap
      :link: module-pw_bluetooth_proxy-roadmap
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Upcoming plans

   .. grid-item-card:: :octicon:`code-square` Code size analysis
      :link: module-pw_bluetooth_proxy-size-reports
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Understand code footprint and savings potential


.. _module-pw_bluetooth_proxy-getstarted:

-----------
Get started
-----------
.. repository: https://bazel.build/concepts/build-ref#repositories

.. tab-set::

   .. tab-item:: Bazel

      Bazel isn't supported yet.

   .. tab-item:: GN
      :selected:

      First add emboss to your project as described in
      :ref:`module-pw_third_party_emboss`.

      Then add ``$dir_pw_bluetooth_proxy`` to
      the ``deps`` list in your ``pw_executable()`` build target:

      .. code-block::

         pw_executable("...") {
           # ...
           deps = [
             # ...
             "$dir_pw_bluetooth_proxy",
             # ...
           ]
         }

   .. tab-item:: CMake

      CMake isn't supported yet (but will be soon).


.. _module-pw_bluetooth_proxy-reference:

-------------
API reference
-------------

pw::bluetooth::proxy::HciProxy
==============================
.. doxygenclass:: pw::bluetooth::proxy::HciProxy
   :members:

pw::bluetooth::proxy::ProxyPolicy
=================================
.. doxygenclass:: pw::bluetooth::proxy::ProxyPolicy
   :members:

pw::bluetooth::proxy::PassthroughPolicy
=======================================
.. doxygenclass:: pw::bluetooth::proxy::PassthroughPolicy
   :members:


.. _module-pw_bluetooth_proxy-size-reports:

------------------
Code size analysis
------------------
Delta when constructing a passthrough proxy and sending packets through.

.. include:: use_passthrough_proxy_size_report


.. _module-pw_bluetooth_proxy-roadmap:

-------
Roadmap
-------
- ACL flow control
- Sending GATT notifications
- CMake support
- Receiving GATT notifications
- Taking ownership of a L2CAP channel
- Bazel support
- And more...
