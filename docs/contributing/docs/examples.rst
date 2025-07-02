.. Instructions for AI agents (e.g. Gemini CLI):
.. #. Follow the workflow described in :ref:`contributing-docs-quickstart-a`.
..    Do not use Option B.
.. #. Inspect the files in ``//pw_string/examples``. This is a working example
..    of the kind of code that you need to create.
.. #. Create the code example, unit test, and build targets. The first
..    iteration of the unit test should fail.
.. #. Run ``bazelisk test //...`` to verify that your new unit test fails.
.. #. Fix the unit test.
.. #. Run ``bazelisk test //...`` again and verify that the new unit test now
..    passes.
.. #. Run ``bazelisk build //docs`` to verify that the docs build succeeds.
.. #. Run ``bazelisk run //:format`` to fix formatting errors.

.. _contributing-docs-examples:

===================================================
Guidelines for buildable and testable code examples
===================================================
This guide shows :ref:`docs-glossary-upstream` maintainers how to write
buildable and testable code examples for ``pigweed.dev``. It's focused on
C++ but the general pattern is theoretically applicable to any language.

.. _contributing-docs-philosophy:

----------
Philosophy
----------
Our goal is to make all code examples on ``pigweed.dev``:

* Buildable. The examples should be in the critical path of the "build
  everything" command (``bazelisk build //...``) so that upstream Pigweed
  contributors receive immediate feedback that their code changes are breaking
  an example that appears on ``pigweed.dev``.
* Testable. Each example should have one or more unit tests that verify that
  the example actually does what it claims to do. These tests should likewise
  be in the critical path of the "test everything" command
  (``bazelisk test //...``).
* Minimal. Each example should clearly and simply demonstrate one use case.
  ``pigweed.dev`` readers should not need to weed through a lot of unrelated,
  complex code.
* Complete. ``pigweed.dev`` readers should be able to copy-and-paste the code
  example and have it just work, with little or zero modification needed.

.. _contributing-docs-quickstart:

----------
Quickstart
----------

Choose either :ref:`contributing-docs-quickstart-a` (recommended) or
:ref:`contributing-docs-quickstart-b`. They're mutually exclusive workflows.

.. _contributing-docs-quickstart-a:

Option A: Separate file for each example
========================================
Placing each example into its own file with its own build target is the
recommended approach because it provides the strongest guarantee that the
example completely compiles and passes tests on its own, without interference
from other examples. However, if this approach feels too toilsome, you're
welcome to use the more lightweight :ref:`contributing-docs-quickstart-b`
instead.

.. _contributing-docs-quickstart-a-dir:

Create an examples directory
----------------------------
Create a directory named ``examples`` for your Pigweed module,
e.g. ``//pw_string/examples/``.

Having a dedicated directory makes it easy to run the unit tests
for the examples with a single wildcard command, like this:

.. code-block:: console

   bazelisk build //pw_string/examples/...

.. _contributing-docs-quickstart-a-file:

Create the code example
-----------------------
#. Create a file for your code example. The filename should end with
   ``_test.cc``, e.g. ``build_string_in_buffer_test.cc``. The first part of
   the filename (``build_string_in_buffer``) should describe the use case.

   .. literalinclude:: ../../../pw_string/examples/build_string_in_buffer_test.cc
      :language: cpp
      :start-after: // DOCSTAG: [contributing-docs-examples]
      :end-before: // DOCSTAG: [contributing-docs-examples]

.. _contributing-docs-quickstart-a-key-points:

Key points
~~~~~~~~~~
Guidelines for your code example file:

* Only the code between the  ``DOCSTAG`` comments will be pulled
  into the docs. See :ref:`module-pw_string` to view how the file
  above is actually rendered in the docs.

* Headers that the example code relies on, like ``pw_log/log.h`` and
  ``pw_string/string_builder.h``, should be shown in the user-facing code
  example.

* Wrap the example code in the ``examples`` namespace.

* The primary example code is usually wrapped in a function like
  ``BuildStringInBuffer()``. This makes the example easier to unit test.

* Each file should only contain one example. The main reason for this
  is to ensure that each example actually compiles on its own, without
  interference from other examples.

* If you need another function to demonstrate usage of the primary
  example code, use ``void main()`` for the signature of this
  secondary function.

* Create one or more unit tests for the primary example code.
  Wrap the unit test in an anonymous namespace. All code examples
  across all Pigweed modules should use the ``ExampleTests`` test suite.

* Follow the usual unit testing best practice of making sure that the
  test initially fails.

.. _contributing-docs-quickstart-a-targets:

Create build targets
--------------------
Create build targets for upstream Pigweed's Bazel, GN, and CMake build systems.

.. _contributing-docs-quickstart-a-targets-bazel:

Bazel
~~~~~
.. _//pw_string/examples/BUILD.bazel: https://cs.opensource.google/pigweed/pigweed/+/main:pw_string/examples/BUILD.bazel

#. Create a ``BUILD.bazel`` file in your ``examples`` directory.

   .. literalinclude:: ../../../pw_string/examples/BUILD.bazel
      :language: starlark
      :start-after: # DOCSTAG: [contributing-docs-examples]
      :end-before: # DOCSTAG: [contributing-docs-examples]

   The ``sphinx_docs_library`` rule is how you pull the code example into the
   docs build. There is no equivalent of this in the GN or CMake files
   because the docs are only built with Bazel.

.. _contributing-docs-quickstart-a-targets-gn:

GN
~~
#. Create a ``BUILD.gn`` file in your ``examples`` directory.

   .. literalinclude:: ../../../pw_string/examples/BUILD.gn
      :language: starlark
      :start-after: # DOCSTAG: [contributing-docs-examples]
      :end-before: # DOCSTAG: [contributing-docs-examples]

#. Update the top-level ``BUILD.gn`` file for your module
   (e.g. ``//pw_string/BUILD.gn``) so that the new code example
   unit tests are run as part of the module's default unit test
   suite.

   .. literalinclude:: ../../../pw_string/BUILD.gn
      :language: starlark
      :start-after: # DOCSTAG: [contributing-docs-examples]
      :end-before: # DOCSTAG: [contributing-docs-examples]

   Notice how ``$dir_pw_string/examples:tests`` is included in
   the list of tests.

.. _contributing-docs-quickstart-a-targets-cmake:

CMake
~~~~~
#. Create a ``CMakeLists.txt`` file in your ``examples`` directory.

   .. literalinclude:: ../../../pw_string/examples/CMakeLists.txt
      :language: text
      :start-after: # DOCSTAG: [contributing-docs-examples]
      :end-before: # DOCSTAG: [contributing-docs-examples]

.. _contributing-docs-quickstart-a-include:

Pull the example into a doc
---------------------------
#. In your module's top-level ``BUILD.bazel`` file (e.g.
   ``//pw_string/BUILD.bazel``), update the ``sphinx_docs_library`` target:

   .. literalinclude:: ../../../pw_string/BUILD.bazel
      :language: starlark
      :start-after: # DOCSTAG: [contributing-docs-examples]
      :end-before: # DOCSTAG: [contributing-docs-examples]

   Notice how ``//pw_string/examples`` is included in the ``deps``
   of the ``docs`` target. This is how you make the example source code
   available to Sphinx when it builds the docs.

#. Use a ``literalinclude`` directive in your reStructuredText to pull
   the code example into your doc:

   .. literalinclude:: ../../../pw_string/docs.rst
      :language: rst
      :dedent:
      :start-after: .. DOCSTAG: [contributing-docs-examples]
      :end-before: .. DOCSTAG: [contributing-docs-examples]

You're done!

.. _contributing-docs-quickstart-b:

Option B: Single file for all examples
======================================
In the Option B approach you place all of your examples and unit tests in a
single file and build target. The main drawback with this approach is that it's
easy to accidentally make your code example incomplete. E.g. you forget to
include a header in an example, because an earlier example already included
that same header. The build target is also harder to read, because all of the
dependencies for all of the code examples are mixed into a single target.

However, Option B is still a major improvement over the status quo
of not building or testing code examples, so you're welcome to use Option B if
:ref:`contributing-docs-quickstart-a` feels too toilsome.

Create a file for the code examples
-----------------------------------
#. Create an ``examples.cc`` file in the root directory of your module. All of
   your code examples and unit tests will go in this single file.

   .. literalinclude:: ../../../pw_assert/examples.cc
      :language: cpp
      :start-after: // DOCSTAG: [contributing-docs-examples]
      :end-before: // DOCSTAG: [contributing-docs-examples]

#. Make sure that your code examples and unit tests follow all of the
   guidelines that are described in
   :ref:`contributing-docs-quickstart-a-key-points`.

Create build targets
--------------------
#. In your module's top-level build files (e.g. ``//pw_assert/BUILD.bazel``,
   ``//pw_assert/BUILD.gn``, and ``//pw_assert/CMakeLists.txt``) create build
   targets for your new ``examples.cc`` file.

   Here are examples for each build system:

   Bazel:

   .. literalinclude:: ../../../pw_assert/BUILD.bazel
      :language: starlark
      :start-after: # DOCSTAG: [contributing-docs-examples]
      :end-before: # DOCSTAG: [contributing-docs-examples]

   GN:

   .. literalinclude:: ../../../pw_assert/BUILD.gn
      :language: py
      :start-after: # DOCSTAG: [contributing-docs-examples]
      :end-before: # DOCSTAG: [contributing-docs-examples]

   CMake:

   .. literalinclude:: ../../../pw_assert/CMakeLists.txt
      :language: text
      :start-after: # DOCSTAG: [contributing-docs-examples]
      :end-before: # DOCSTAG: [contributing-docs-examples]

Pull the example into a doc
---------------------------
#. Use a ``literalinclude`` directive in your reStructuredText to pull
   the code example into your doc:

   .. literalinclude:: ../../../pw_assert/docs.rst
      :language: rst
      :dedent:
      :start-after: .. DOCSTAG: [contributing-docs-examples]
      :end-before: .. DOCSTAG: [contributing-docs-examples]

You're done!
