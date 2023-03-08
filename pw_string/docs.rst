.. _module-pw_string:

.. rst-class:: with-subtitle

=========
pw_string
=========

.. pigweed-module::
   :name: pw_string
   :tagline: Efficient, easy, and safe string manipulation
   :status: stable
   :languages: C++14, C++17
   :code-size-impact: 500 to 1500 bytes
   :get-started: module-pw_string-get-started
   :design: module-pw_string-design
   :guides: module-pw_string-guide
   :api: module-pw_string-api

   - **Efficient**: No memory allocation, no pointer indirection.
   - **Easy**: Use the string API you already know.
   - **Safe**: Never worry about buffer overruns or undefined behavior.

   *Pick three!* If you know how to use ``std::string``, just use
   :cpp:type:`pw::InlineString` in the same way:

   .. code:: cpp

      // Create a string from a C-style char array; storage is pre-allocated!
      pw::InlineString<16> my_string = "Literally";

      // We have some space left, so let's add to the string.
      my_string.append('?', 3);  // "Literally???"

      // Let's try something evil and extend this past its capacity 😈
      my_string.append('!', 8);
      // Foiled by a crash! No mysterious bugs or undefined behavior.

   Need to build up a string? :cpp:type:`pw::StringBuilder` works like
   ``std::ostringstream``, but with most of the efficiency and memory benefits
   of :cpp:type:`pw::InlineString`:

   .. code:: cpp

      // Create a pw::StringBuilder with a built-in buffer
      pw::StringBuffer<32> my_string_builder = "Is it really this easy?";

      // Add to it with idiomatic C++
      my_string << " YES!";

      // Use it like any other string
      PW_LOG_DEBUG("%s", my_string_builder.c_str());

   Check out :ref:`module-pw_string-guide` for more code samples.

----------
Background
----------
String manipulation is a very common operation, but the standard C and C++
string libraries have drawbacks. The C++ functions are easy-to-use and powerful,
but require too much flash and memory for many embedded projects. The C string
functions are lighter weight, but can be difficult to use correctly. Mishandling
of null terminators or buffer sizes can result in serious bugs.

------------
Our solution
------------
The ``pw_string`` module provides the flexibility, ease-of-use, and safety of
C++-style string manipulation, but with no dynamic memory allocation and a much
smaller binary size impact. Using ``pw_string`` in place of the standard C
functions eliminates issues related to buffer overflow or missing null
terminators.

---------------
Who this is for
---------------
``pw_string`` is potentially useful for anyone who is working with strings in
C++.

Is it right for you?
--------------------
Here are some size reports that may affect whether ``pw_string`` is right for
you.

Size comparison: snprintf versus pw::StringBuilder
--------------------------------------------------
:cpp:type:`pw::StringBuilder` is safe, flexible, and results in much smaller code size than
using ``std::ostringstream``. However, applications sensitive to code size
should use :cpp:type:`pw::StringBuilder` with care.

The fixed code size cost of :cpp:type:`pw::StringBuilder` is significant, though smaller than
``std::snprintf``. Using :cpp:type:`pw::StringBuilder`'s ``<<`` and ``append`` methods exclusively in
place of ``snprintf`` reduces code size, but ``snprintf`` may be difficult to
avoid.

The incremental code size cost of :cpp:type:`pw::StringBuilder` is comparable to ``snprintf`` if
errors are handled. Each argument to :cpp:type:`pw::StringBuilder`'s ``<<`` method expands to a
function call, but one or two :cpp:type:`pw::StringBuilder` appends may have a smaller code size
impact than a single ``snprintf`` call.

.. include:: string_builder_size_report

Size comparison: snprintf versus pw::string::Format
---------------------------------------------------
The ``pw::string::Format`` functions have a small, fixed code size
cost. However, relative to equivalent ``std::snprintf`` calls, there is no
incremental code size cost to using ``pw::string::Format``.

.. include:: format_size_report

Roadmap
-------
* StringBuilder's fixed size cost can be dramatically reduced by limiting
  support for 64-bit integers.
* Consider integrating with the tokenizer module.

---------------
Who this is for
---------------
..
  Highlight the use cases that are appropriate for this module. This is the place
  to make the sales pitch-why should developers use this module, and what makes
  this module stand out from alternative solutions that try to address this
  problem.

--------------------
Is it right for you?
--------------------
..
  This module may not solve all problems or be appropriate for all circumstances.
  This is the place to add those caveats. Highly-experimental modules that are
  under very active development might be a poor fit for any developer right now.
  If so, mention that here.

..
  https://issues.pigweed.dev/issues/269671709

Compatibility
-------------
C++17, C++14 (:cpp:type:`pw::InlineString`)

.. _module-pw_string-get-started:

---------------
Getting started
---------------

GN
--

Add ``$dir_pw_string`` to the ``deps`` list in your ``pw_executable()`` build
target:

.. code::

  pw_executable("...") {
    # ...
    deps = [
      # ...
      "$dir_pw_string",
      # ...
    ]
  }

See `//source/BUILD.gn <https://pigweed.googlesource.com/pigweed/sample_project/+/refs/heads/main/source/BUILD.gn>`_
in the Pigweed Sample Project for an example.

Zephyr
------
Add ``CONFIG_PIGWEED_STRING=y`` to the Zephyr project's configuration.

---------------------
Design considerations
---------------------
``pw_string`` is designed to prioritize safety and static allocation. It matches
the ``std::string`` API as closely as possible, but isn't intended to provide
complete API compatibility. See :ref:`module-pw_string-design` for more
details.

-------
Roadmap
-------
* The fixed size cost of :cpp:type:`pw::StringBuilder` can be dramatically reduced by
  limiting support for 64-bit integers.
* ``pw_string`` may be integrated with :ref:`module-pw_tokenizer`.

----------
Learn more
----------
.. toctree::
   :maxdepth: 1

   design
   guide
   api
