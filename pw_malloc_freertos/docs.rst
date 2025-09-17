.. _module-pw_malloc_freertos:

------------------
pw_malloc_freertos
------------------
.. pigweed-module::
   :name: pw_malloc_freertos

``pw_malloc_freertos`` implements the ``pw_malloc`` facade using the FreeRTOS
heap functions.

- It implements an :cc:`pw::Allocator` using the
  ``pvPortMalloc`` and ``vPortFree`` heap functions from
  `FreeRTOS <https://www.freertos.org/a00111.html>`_.
- It implements a :ref:`module-pw_malloc` backend using its ``Allocator``.

  - The ``pw::malloc::InitSystemAllocator`` method is trivally empty as FreeRTOS
    defines its own heap variable storage.
