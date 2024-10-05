.. _contrib-docs-quickstart:

===========================
Docs contributor quickstart
===========================
This quickstart shows you how to contribute updates to ``pigweed.dev``
content.

.. important::

   This guide is incomplete. If you need to make content updates **right now**,
   follow the guidance in :ref:`docs-get-started-upstream`. The rest of this
   quickstart focuses on the new, work-in-progress Bazel-based docs system.
   You should ignore this quickstart unless a Pigweed teammate has specifically
   pointed you here.

.. _contrib-docs-quickstart-setup:

-----------------------------------
Set up your development environment
-----------------------------------
#. Complete :ref:`docs-first-time-setup`.

#. :ref:`docs-install-bazel`.

.. _contrib-docs-quickstart-build:

--------------
Build the docs
--------------
.. code-block:: console

   bazelisk build //docs/...

.. _contrib-docs-quickstart-preview:

---------------------------------
Locally preview the rendered docs
---------------------------------
.. code-block:: console

   bazelisk run //docs:docs.serve

A message like this should get printed to ``stdout``:

.. code-block:: console

   Serving...
     Address: http://0.0.0.0:8000
     Serving directory: /home/kayce/pigweed/pigweed/bazel-out/k8-fastbuild/bin/docs/docs/_build/html
         url: file:///home/kayce/pigweed/pigweed/bazel-out/k8-fastbuild/bin/docs/docs/_build/html
     Server CWD: /home/kayce/.cache/bazel/_bazel_kayce/9659373b1552c281136de1c8eeb3080d/execroot/_main/bazel-out/k8-fastbuild/bin/docs/docs.serve.runfiles/_main

You can access the rendered docs at the URL that's printed next to
**Address**.
