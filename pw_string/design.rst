.. _module-pw_string-design:

================
pw_string design
================
..
  This doc provides background on how a module works internally, the assumptions
  inherent in its design, why this particular design was chosen over others, and
  other topics of that nature.

:cpp:type:`pw::InlineString` / :cpp:class:`pw::InlineBasicString` follows the
``std::string`` / ``std::basic_string<T>`` API, with a few variations:

- :cpp:type:`pw::InlineString` provides overloads specific to character arrays.
  These perform compile-time capacity checks and are used for class template
  argument deduction. Like ``std::string``, character arrays are treated as
  null-terminated strings.
- :cpp:type:`pw::InlineString` allows implicit conversions from
  ``std::string_view``. Specifying the capacity parameter is cumbersome, so
  implicit conversions are helpful. Also, implicitly creating a
  :cpp:type:`pw::InlineString` is less costly than creating a ``std::string``.
  As with ``std::string``, explicit conversions are required from types that
  convert to ``std::string_view``.
- Functions related to dynamic memory allocation are not present (``reserve()``,
  ``shrink_to_fit()``, ``get_allocator()``).
- ``resize_and_overwrite()`` only takes the ``Operation`` argument, since the
  underlying string buffer cannot be resized.

See the `std::string documentation
<https://en.cppreference.com/w/cpp/string/basic_string>`_ for full details.

Key differences from ``std::string``
------------------------------------
- **Fixed capacity** -- Operations that add characters to the string beyond its
  capacity are an error. These trigger a ``PW_ASSERT`` at runtime. When
  detectable, these situations trigger a ``static_assert`` at compile time.
- **Minimal overhead** -- :cpp:type:`pw::InlineString` operations never
  allocate. Reading the contents of the string is a direct memory access within
  the string object, without pointer indirection.
- **Constexpr support** -- :cpp:type:`pw::InlineString` works in ``constexpr``
  contexts, which is not supported by ``std::string`` until C++20.

Safe Length Checking
--------------------
This module provides two safer alternatives to ``std::strlen`` in case the
string is extremely long and/or potentially not null-terminated.

First, a constexpr alternative to C11's ``strnlen_s`` is offerred through
:cpp:func:`pw::string::ClampedCString`. This does not return a length by
design and instead returns a string_view which does not require
null-termination.

Second, a constexpr specialized form is offered where null termination is
required through :cpp:func:`pw::string::NullTerminatedLength`. This will only
return a length if the string is null-terminated.

Exceeding the capacity
----------------------
Any :cpp:type:`pw::InlineString` operations that exceed the string's capacity
fail an assertion, resulting in a crash. Helpers are provided in
``pw_string/util.h`` that return ``pw::Status::ResourceExhausted()`` instead of
failing an assert when the capacity would be exceeded.
