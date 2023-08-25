.. _target-docs:

----
docs
----
The Pigweed docs target assembles Pigweed's reStructuredText and markdown
documentation into a collection of HTML pages.

The docs target uses the :ref:`target-stm32f429i-disc1` toolchain with C++20 for
size reports.

Building
========
To build for this target, invoke ninja with the top-level "docs" group as the
target to build.

.. code-block:: sh

  $ gn gen out
  $ ninja -C out docs

Output
======
Final HTML documentation will be placed in the ``out/docs/gen/docs/html``
directory of the build.
