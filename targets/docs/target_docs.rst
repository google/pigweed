.. _chapter-docs:

.. default-domain:: cpp

.. highlight:: sh

----
docs
----
The Pigweed docs target assembles Pigweed's reStructuredText and markdown
documentation into a collection of HTML pages.

Building
========
To build for this target, change the ``pw_target_config`` GN build arg to point
to this target's configuration file.

.. code:: sh

  $ gn gen out
  $ ninja -C out docs

Output
======
Final HTML documentation will be placed in the ``out/docs/gen/docs/html``
directory of the build.
