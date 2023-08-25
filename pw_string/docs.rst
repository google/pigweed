.. _module-pw_string:

=========
pw_string
=========
.. pigweed-module::
   :name: pw_string
   :tagline: Efficient, easy, and safe string manipulation
   :status: stable
   :languages: C++14, C++17
   :code-size-impact: 500 to 1500 bytes

   - **Efficient**: No memory allocation, no pointer indirection.
   - **Easy**: Use the string API you already know.
   - **Safe**: Never worry about buffer overruns or undefined behavior.

   *Pick three!* If you know how to use ``std::string``, just use
   :cpp:type:`pw::InlineString` in the same way:

   .. code-block:: cpp

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

   .. code-block:: cpp

      // Create a pw::StringBuilder with a built-in buffer
      pw::StringBuffer<32> my_string_builder = "Is it really this easy?";

      // Add to it with idiomatic C++
      my_string << " YES!";

      // Use it like any other string
      PW_LOG_DEBUG("%s", my_string_builder.c_str());

   Check out :ref:`module-pw_string-guide` for more code samples.

String manipulation on embedded systems can be surprisingly challenging.

- **C strings?** They're light-weight but come with many pitfalls for those who
  don't know the standard library deeply.

- **C++ strings?** STL string classes are safe and easy to use, but they consume
  way too much code space and are designed to be used with dynamic memory
  allocation.

- **Roll your own strings?** You don't have time! You have a product to ship!

Embedded systems need string functionality that is both *safe* and *suitable*
for resource-constrained platforms.

.. rst-class:: key-text

   ``pw_string`` provides safe string handling functionality with an API that
   closely matches that of ``std::string``, but without dynamic memory
   allocation and with a *much* smaller :ref:`binary size impact<module-pw_string-size-reports>`.

--------------------
Is it right for you?
--------------------
.. rst-class:: key-text

   ``pw_string`` is useful any time you need to handle strings in embedded C++.

If your project written in C, ``pw_string`` is not a good fit since we don't
currently expose a C API.

On the other hand, for larger platforms where code space isn't in short supply
and dynamic memory allocation isn't a problem, you may find that ``std::string``
meets your needs.

.. tip::
   ``pw_string`` works just as well on larger embedded platforms and host
   systems. Using ``pw_string`` even when you could get away with ``std:string``
   gives you the flexibility to move to smaller platforms later with much less
   rework.

.. _module-pw_string-get-started:

---------------
Getting Started
---------------

.. tab-set::

   .. tab-item:: GN

      Add ``$dir_pw_string`` to the ``deps`` list in your ``pw_executable()``
      build target:

      .. code-block::

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

   .. tab-item:: Zephyr

      Add ``CONFIG_PIGWEED_STRING=y`` to the Zephyr project's configuration.

-------
Roadmap
-------
* The fixed size cost of :cpp:type:`pw::StringBuilder` can be dramatically
  reduced by limiting support for 64-bit integers.
* ``pw_string`` may be integrated with :ref:`module-pw_tokenizer`.

.. toctree::
   :hidden:
   :maxdepth: 1

   design
   guide
   api
