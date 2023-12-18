.. _module-pw_malloc_freertos:

------------------
pw_malloc_freertos
------------------

``pw_malloc_freertos`` implements the ``pw_malloc`` facade using the FreeRTOS
heap functions. Wrapper functions for ``malloc``, ``free``, ``realloc`` and
``calloc`` are provided that use the ``pvPortMalloc`` and ``vPortFree`` heap
functions from FreeRTOS. The start/end parameters to the ``pw_MallocInit``
function are ignored by this backend as FreeRTOS defines its own heap variable
storage.
