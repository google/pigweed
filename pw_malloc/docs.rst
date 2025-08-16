.. _module-pw_malloc:

=========
pw_malloc
=========
.. pigweed-module::
   :name: pw_malloc

This module defines an interface for replacing the standard libc dynamic memory
operations.

This facade doesn't implement any heap structure or dynamic memory methods. It
only requires that backends implements ``pw::malloc::GetSystemAllocator`` and
optionally ``pw::malloc::InitSystemAllocator``. These functions are called
before static initialization, and are responsible for initializing global data
structures required by the ``malloc`` implementation.

The intent of this module is to provide an interface for user-provided dynamic
memory operations that is compatible with different implementations.

-----
Setup
-----
This module requires the following setup:

1. Choose a ``pw_malloc`` backend, or write one yourself.
2. Select a backend in your build system. If using GN build, Specify the
   ``pw_malloc_BACKEND`` GN build arg to point to the library that provides a
   ``pw_malloc`` backend. If using the Bazel build, add the constraint value for
   the backend library that provides a ``pw_malloc`` backend.
3. Add a dependency on the ``pw_malloc`` facade in your targets ``executable``
   build template, e.g.:

.. code-block::

   template("platform_executable") {
      target("executable", target_name) {
         deps = []
         config = []
         forward_variables_from(invoker, "*")
         if (pw_malloc_BACKEND != "") {
            deps += [ dir_pw_malloc ]
         }
      }
   }

Compile-time configuration
==========================
This module has configuration options that globally affect the behavior of
``pw_malloc`` via compile-time configuration of this module.

* :doxylink:`PW_MALLOC_LOCK_TYPE`
* :doxylink:`PW_MALLOC_METRICS_TYPE`
* :doxylink:`PW_MALLOC_BLOCK_OFFSET_TYPE`
* :doxylink:`PW_MALLOC_MIN_BUCKET_SIZE`
* :doxylink:`PW_MALLOC_NUM_BUCKETS`
* :doxylink:`PW_MALLOC_DUAL_FIRST_FIT_THRESHOLD`

See the
:ref:`module documentation <module-structure-compile-time-configuration>` for
more details.

Heap initialization
===================
You can provide a region of memory to use as heap as either a byte span or a
pair of addresses to ``pw::malloc::InitSystemAllocator``.

-----
Usage
-----
Once the heap is initialized, you may simply use ``malloc`` and ``free`` as you
would normally, as well as standard library functions like ``strdup`` and
built-in routines like ``new`` that use those functions.

If you configured a ``PW_MALLOC_METRICS_TYPE``, you can retrieve metrics using
``pw::malloc::GetSystemMetrics()``.

.. toctree::
   :maxdepth: 1
   :hidden:

   backends
