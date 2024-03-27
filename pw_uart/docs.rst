.. _module-pw_uart:

=======
pw_uart
=======

The Uart interface defines the core methods for UART communication. This serves
as a blueprint for concrete UART implementations. You will need to write the
backend code tailored to your specific hardware device to interact with the
UART peripheral.

-----------
Get started
-----------
.. repository: https://bazel.build/concepts/build-ref#repositories

.. tab-set::

   .. tab-item:: Bazel

      Add ``@pigweed//pw_uart`` to the ``deps`` list in your Bazel target:

      .. code-block::

         cc_library("...") {
           # ...
           deps = [
             # ...
             "@pigweed//pw_uart",
             # ...
           ]
         }

      This assumes that your Bazel ``WORKSPACE`` has a `repository
      <https://bazel.build/concepts/build-ref#repositories>`_ named ``@pigweed``
      that points to the upstream Pigweed repository.

   .. tab-item:: GN

      Add ``$dir_pw_uart`` to the ``deps`` list in your ``pw_executable()``
      build target:

      .. code-block::

         pw_executable("...") {
           # ...
           deps = [
             # ...
             "$dir_pw_uart",
             # ...
           ]
         }

   .. tab-item:: CMake

      Add ``pw_uart`` to your ``pw_add_library`` or similar CMake target:

      .. code-block::

         pw_add_library(my_library STATIC
           HEADERS
             ...
           PRIVATE_DEPS
             # ...
             pw_uart
             # ...
         )

.. _module-pw_uart-reference:

-------------
API reference
-------------
.. doxygengroup:: pw_uart
   :content-only:
   :members:
