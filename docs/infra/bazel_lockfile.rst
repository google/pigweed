.. _docs-bazel-lockfile:

===========================
Managing the Bazel lockfile
===========================
The `Bazel lockfile <https://bazel.build/external/lockfile>`_
(``MODULE.bazel.lock``) is checked into the Pigweed repository. Some changes to
the Bazel build (especially changes to the ``MODULE.bazel`` file) may require
regenerating the lockfile. This document describes how to do this.

------------------------------
Simple case: automatic updates
------------------------------
In the simplest case, Bazel will automatically update the lockfile in the
course of your development work, as you execute commands like ``bazel build``.
You just need to commit the changes as part of your CL.

---------------------------------
Complex case: platform-dependency
---------------------------------
Occasionally, some (transitive) dependency added to the build will be
platform-dependent. An example of this are Rust crates added using the
`crates.from_specs
<https://bazelbuild.github.io/rules_rust/crate_universe_bzlmod.html#from_specs>`__
module extension. If this happens, you will see errors like the following in CQ
builders running on platforms different from the one you developed on:

.. code-block:: console

   ERROR: The module extension 'ModuleExtensionId{bzlFileLabel=@@rules_rust+//crate_universe:extension.bzl, extensionName=crate, isolationKey=Optional.empty}' for platform os:osx,arch:x86_64 does not exist in the lockfile.

What's going on here is that the exact versions of external dependencies that
enter the build vary depending on the OS or CPU architecture of the system
Bazel is running on.

Fortunately, we have some builders meant to help you in this situation.

What do I do?
=============
#. Upload your change to Gerrit.
#. Use the "CHOOSE TRYJOBS" dialog to run the "pigweed-linux-bazel-lockfile" tryjob.
#. If the job fails, the result will include the diff that you need to apply to
   your CL (via ``git patch``) to update the Bazel lockfile for Linux. Apply
   the patch and upload a new patchset to Gerrit. If the job passes, the
   lockfile is already up to date on this host platform and no patching is
   necessary!
#. Run the "pigweed-mac-arm-bazel-lockfile" tryjob, and apply the patch it
   generates.
#. Run the "pigweed-mac-x86-bazel-lockfile" tryjob, and apply the patch it
   generates.
