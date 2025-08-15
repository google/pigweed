.. _showcase-sense-tutorial-analysis:

==============================
6. Static and runtime analysis
==============================
Beyond unit testing, Pigweed provides well-lit paths for integrating a variety
of tools for :ref:`docs-automated-analysis`.

In this section we'll show you how to run some of these tools for Sense, and
discuss actual issues that we caught by using them.

--------------------------
UndefinedBehaviorSanitizer
--------------------------
`UndefinedBehaviorSanitizer
<https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html>`__, or ubsan, is a
fast undefined behavior detector.

In general, tools like ubsan are unlikely to run on-device. But as you just
learned in :ref:`showcase-sense-tutorial-hosttests`, all unit tests in Sense can
run on your host! So, we can rebuild these tests with ubsan instrumentation and
run them via:

.. code-block:: console

   bazelisk test --config=ubsan //...

Example issue
=============
We used ubsan to catch a real issue when developing Sense: :bug:`385400496`.  We
originally initialized an `std::chrono::time_point
<https://en.cppreference.com/w/cpp/chrono/time_point.html>`__ class data member
to ``std::chrono::time_point::min()``.  This *looks* like a reasonable thing to
do, but it turns out ``std::chrono::time_point::min()`` corresponds to the
earliest time representable by the underlying type (sometime on September 21,
1677).  When you try to compute the time difference between the current time and
this initial time in the 17th century, you get signed integer
overflow---undefined behavior in C++!

This bug nicely illustrates the value of ubsan:

*  Although the test is executed on the host, it caught a bug that was also
   present in the embedded code.
*  The bug would be very difficult to identify by inspecting the code. It's caused
   by an implementation detail (the numerical type used to represent time
   points) that ``std::chrono`` attempts, but fails, to abstract.

----------------
AddressSanitizer
----------------
`AddressSanitizer <https://clang.llvm.org/docs/AddressSanitizer.html>`__, or
asan, detects memory errors such as out-of-bounds access and use-after-free.

Similarly to ubsan, you can rebuild and rerun the tests with asan instrumentation via:

.. code-block:: console

   bazelisk test --config=asan //...

Example issue
=============
AddressSanitizer identified a lifetime issue in a multithreaded test
(:bug:`352163490`): some objects referenced by pubsub subscribers failed to
outlive the pubsub.

In this case, we didn't need asan to *detect* there was an issue. The test
failed in the regular build configuration, too, but with cryptic test assertion
errors. AddressSanitizer pin-pointed that the bug was accessing memory after it
had been freed, and provided stack traces of the incorrect access.

Fixing this required `restructuring the test
<https://cs.opensource.google/pigweed/showcase/sense/+/4d9555ba24a99453235fb2f55eeb95edb39e6196>`__
to ensure all objects outlived their references.

----------
clang-tidy
----------
`clang-tidy <https://clang.llvm.org/extra/clang-tidy/>`__ is a C++ "linter" and
static analysis tool. It identifies bug-prone patterns (e.g., use after move),
non-idiomatic usage (e.g., creating ``std::unique_ptr`` with ``new`` rather than
``std::make_unique``), and performance issues (e.g., unnecessary copies of loop
variables).

You can run clang-tidy on all C++ build targets in Sense by running:

.. code-block:: console

   bazelisk build --config=clang-tidy //...

.. note::

   It's ``bazelisk build``, not ``bazelisk test``, because clang-tidy does not
   require executing the code. It's a static check performed at build time.

Example issues
==============
When we enabled clang-tidy in Sense in http://pwrev.dev/266915, we found a
number of pre-existing issues:

*  `Missing override keywords
   <https://clang.llvm.org/extra/clang-tidy/checks/modernize/use-override.html>`__
   A function or destructor marked ``override`` that is not an override of a base
   class virtual function will not compile, and this helps catch common errors.
   Not using this keyword on methods which *are* overrides prevents the compiler
   from helping you!
*  `Mismatched parameter names in definitions and declarations
   <https://clang.llvm.org/extra/clang-tidy/checks/readability/inconsistent-declaration-parameter-name.html>`__.
   This hinders readability: the same parameter is referred to by a different
   name in different parts of the code!
*  `Unused using declarations
   <https://clang.llvm.org/extra/clang-tidy/checks/misc/unused-using-decls.html>`__.
*  ``static`` functions in headers at namespace scope. (This is an error because
   a ``static`` function in a header generates a copy in every translation unit
   it is used in. Such functions should be marked ``inline`` instead of
   ``static``.)

------
Pylint
------
`Pylint`_ is a customizable Python linter that detects problems such as overly
broad ``catch`` statements, unused arguments/variables, and mutable default
parameter values.

You can run it on the Python targets in Sense by running:

.. code-block:: console

   bazelisk build --config=pylint //...

Example issues
==============
We enabled Pylint for Sense in http://pwrev.dev/309782. We found no real bugs,
but many missing docstrings, too-long-lines, unused imports, and unused
variables.

--------------
Layering check
--------------
Pigweed toolchains have support for `layering check
<https://maskray.me/blog/2022-09-25-layering-check-with-clang>`__. The layering
check makes it a compile-time error to ``#include`` a header that's not in the
``hdrs`` of a ``cc_library`` you *directly* depend on. This produces cleaner
dependency graphs and makes future code refactorings easier.  See
:ref:`module-pw_toolchain-bazel-layering-check` for more details.

There's no separate command for running the layering check: it's always enforced
when building the code.

Example issues
==============
We enabled layering check for Sense in http://pwrev.dev/261652. As you can see,
even in this relatively small project we accumulated a large number of
dependencies that were not properly reflected in the build graph!

-------
Summary
-------
Now it's time for the fun stuff. Head over to :ref:`showcase-sense-tutorial-sim`
to try out the bringup app, ``blinky``.
