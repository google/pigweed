.. _module-pw_alignment:

============
pw_alignment
============
.. pigweed-module::
   :name: pw_alignment
   :tagline: Natural object alignment, guaranteed
   :status: stable
   :languages: C++17

- **Transparent**: Enforce natural alignment without any changes to how your
  objects are used.
- **Efficient**: Prevent your compiler from emitting libcalls to builtin
  atomic functions and ensure it uses native atomic instructions instead.

.. code-block:: c++

   #include "pw_alignment/alignment.h"

   // Passing a `std::optional<bool>` to `__atomic_exchange` might not replace
   // the call with native instructions if the compiler cannot determine all
   // instances of this object will happen to be aligned.
   std::atomic<std::optional<bool>> maybe_nat_aligned_obj;

   // `pw::NaturallyAligned<...>` forces the object to be aligned to its size,
   // so passing it to `__atomic_exchange` will result in a replacement with
   // native instructions.
   std::atomic<pw::NaturallyAligned<std::optional<bool>>> nat_aligned_obj;

   // Shorter spelling for the same as above.
   pw::AlignedAtomic<std::optional<bool>> also_nat_aligned_obj;

``pw_alignment`` portably ensures that your compiler uses native atomic
instructions rather than libcalls to builtin atomic functions. Take for example
``std::atomic<std::optional<bool>>``. Accessing the underlying object normally
involves a call to a builtin ``__atomic_*`` function. The problem is that the
compiler sometimes can't determine that the size of the object is the same
as the size of its alignment. ``pw_alignment`` ensures that an object aligns to
its size so that compilers can always make this optimization.

.. grid:: 2

   .. grid-item-card:: :octicon:`rocket` Get Started
      :link: module-pw_alignment-getstarted
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      How to set up ``pw_alignment`` in your build system.

   .. grid-item-card:: :octicon:`code-square` API Reference
      :link: module-pw_alignment-reference
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Reference information about the ``pw_alignment`` API.

.. _module-pw_alignment-getstarted:

-----------
Get started
-----------
.. repository: https://bazel.build/concepts/build-ref#repositories

.. tab-set::

   .. tab-item:: Bazel

      Add ``@pigweed//pw_alignment`` to the ``deps`` list in your Bazel target:

      .. code-block::

         cc_library("...") {
           # ...
           deps = [
             # ...
             "@pigweed//pw_alignment",
             # ...
           ]
         }

      This assumes that your Bazel ``WORKSPACE`` has a `repository
      <https://bazel.build/concepts/build-ref#repositories>`_ named ``@pigweed``
      that points to the upstream Pigweed repository.

   .. tab-item:: GN

      Add ``$dir_pw_alignment`` to the ``deps`` list in your ``pw_executable()``
      build target:

      .. code-block::

         pw_executable("...") {
           # ...
           deps = [
             # ...
             "$dir_pw_alignment",
             # ...
           ]
         }

   .. tab-item:: CMake

      Add ``pw_alignment`` to your ``pw_add_library`` or similar CMake target:

      .. code-block::

         pw_add_library(my_library STATIC
           HEADERS
             ...
           PRIVATE_DEPS
             # ...
             pw_alignment
             # ...
         )

.. _module-pw_alignment-reference:

-------------
API reference
-------------
.. doxygengroup:: pw_alignment
   :members:
