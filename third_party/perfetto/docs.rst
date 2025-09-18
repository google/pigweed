.. _module-pw_third_party_perfetto:

==========
Perfetto
==========
The ``$pw_external_perfetto/`` module provides the perfetto protos.  Only
the protos are provided, and not the entire distribution, as there is no need
for it.

--------------------
Code synchronization
--------------------
Unlike other third party libraries used by Pigweed, some perfetto source code is
included in tree. A few factors drove this decision:

- Core Pigweed tracing depend on these perfetto protos. Including the protos
  in-tree avoids having Pigweed require an external repository.
- The perfetto repository is too large to require downstream projects to clone.
- It provides a consistent approach between between bazel and GN builds.
- Perfetto is used as part of the default build.  ``pw package`` doesn't
  currently have a good way to automatically install a required package.

Files are synced from perfetto repository to the ``third_party/perfetto/repo``
directory in Pigweed. The files maintain their original paths under that
directory.

Process
=======
Code is synchronized between the `perfetto repository
<https://android.googlesource.com/platform/external/perfetto>`_ and the `Pigweed repository
<https://pigweed.googlesource.com/pigweed/pigweed>`_ using the
`Copybara <https://github.com/google/copybara>`_ script located at
:cs:`third_party/perfetto/copy.bara.sky`.

To synchronize with the pigweed repository, run the ``copybara`` tool with the
script:

.. code-block:: bash

   copybara third_party/perfetto/copy.bara.sky

That creates a Gerrit change with updates from the perfetto repo, if any.

If the ``copybara`` command fails, the Copybara script may need to be updated.
Try the following:

- Ensure that the source files in ``copy.bara.sky`` are up-to-date. Fix the list
  if any files were renamed in Fuchsia.
