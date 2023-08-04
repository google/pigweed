.. _module-pw_alignment:

============
pw_alignment
============
This module defines an interface for ensuring natural alignment of objects.

Avoiding Atomic Libcalls
========================
The main motivation is to provide a portable way for
preventing the compiler from emitting libcalls to builtin atomics
functions and instead use native atomic instructions. This is especially
useful for any pigweed user that uses ``std::atomic``.

Take for example ``std::atomic<std::optional<bool>>``. Accessing the underlying object
would normally involve a call to a builtin ``__atomic_*`` function provided by a builtins
library. However, if the compiler can determine that the size of the object is the same
as its alignment, then it can replace a libcall to ``__atomic_*`` with native instructions.

There can be certain situations where a compiler might not be able to assert this.
Depending on the implementation of ``std::optional<bool>``, this object could
have a size of 2 bytes but an alignment of 1 byte which wouldn't satisfy the constraint.

Basic usage
-----------
``pw_alignment`` provides a wrapper class ``pw::NaturallyAligned`` for enforcing
natural alignment without any
changes to how the object is used. Since this is commonly used with atomics, an
aditional class ``pw::AlignedAtomic`` is provided for simplifying things.

.. code-block:: c++

   // Passing a `std::optional<bool>` to `__atomic_exchange` might not replace the call
   // with native instructions if the compiler cannot determine all instances of this object
   // will happen to be aligned.
   std::atomic<std::optional<bool>> maybe_nat_aligned_obj;

   // `pw::NaturallyAligned<...>` forces the object to be aligned to its size, so passing
   // it to `__atomic_exchange` will result in a replacement with native instructions.
   std::atomic<pw::NaturallyAligned<std::optional<bool>>> nat_aligned_obj;

   // Shorter spelling for the same as above.
   pw::AlignedAtomic<std::optional<bool>> also_nat_aligned_obj;
