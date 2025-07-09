.. _docs-github-actions:

===========================================
Set up GitHub Actions for a Pigweed project
===========================================
.. _GitHub Actions: https://docs.github.com/en/actions

This tutorial shows you how to set up `GitHub Actions`_ to build and test your
Bazel-based Pigweed project. You'll learn how to set up both presubmit and
postsubmit Actions.

.. _docs-github-actions-examples:

--------
Examples
--------
Pigweed's :ref:`Bazel quickstart repo <docs-get-started-bazel>` demonstrates
GitHub Actions integration:

* `//.github/workflows/presubmit.yaml <https://pigweed.googlesource.com/pigweed/quickstart/bazel/+/refs/heads/main/.github/workflows/presubmit.yaml>`_
* `//.github/workflows/postsubmit.yaml <https://pigweed.googlesource.com/pigweed/quickstart/bazel/+/refs/heads/main/.github/workflows/postsubmit.yaml>`_

.. _docs-github-actions-enable:

--------------
Enable Actions
--------------
.. _Managing GitHub Actions settings for a repository: https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/enabling-features-for-your-repository/managing-github-actions-settings-for-a-repository

Make sure that Actions are enabled for your repo. See
`Managing GitHub Actions settings for a repository`_.

-----------------------------------------
Create a .bazelversion file for your repo
-----------------------------------------
.. _.bazelversion: https://github.com/bazelbuild/bazelisk?tab=readme-ov-file#how-does-bazelisk-know-which-bazel-version-to-run

Make sure that `.bazelversion`_ exists in your repo. This file specifies what
version of Bazel the Actions should use.

.. _docs-github-actions-presubmit:

-------------------------
Create a presubmit Action
-------------------------
A presubmit Action runs when a pull request is sent. Creating a presubmit
Action that makes sure pull requests build and pass tests involves four steps:

#. Checking out your repo's code.

#. Installing Bazel.

#. Building your code.

#. Testing your code.

The first two steps are handled by community-managed extensions. The last
two steps require just a few lines of code.

#. From the root of your repository, create ``.github/workflows/presubmit.yaml``.
   The path to the file must be ``.github/workflows`` but you can name the file
   whatever you want.

#. Put the following YAML into the file:

   .. code-block:: yaml

      name: presubmit
      run-name: presubmit run triggered by ${{ github.actor }}

      on:
        pull_request:
          types: [opened, synchronize, reopened]

      jobs:
        bazel-build-test:
          runs-on: ubuntu-latest
          steps:
            - name: Checkout
              # Check out this repo's code.
              # https://github.com/actions/checkout
              uses: actions/checkout@v4
              with:
                fetch-depth: 0
                submodules: recursive
            - name: Get Bazel
              # Install Bazel.
              # https://github.com/bazel-contrib/setup-bazel
              uses: bazel-contrib/setup-bazel@0.8.1
              with:
                # Avoid downloading Bazel every time.
                bazelisk-cache: true
                # Store build cache per workflow.
                disk-cache: ${{ github.workflow }}
                # Share repository cache between workflows.
                repository-cache: true
            - name: Bazel Build
              # Always use bazelisk rather than bazel to
              # guarantee that the correct version of Bazel
              # (sourced from .bazelversion) is used.
              run: bazelisk build ...
            - name: Bazel Test
              run: bazelisk test ...

#. Commit the file.

The Action runs whenever a pull request is opened, updated, or
reopened.

.. _Bazelisk: https://bazel.build/install/bazelisk

.. note::

   In general, Pigweed recommends always launching Bazel through
   the ``bazelisk`` command rather than the ``bazel`` command.
   `Bazelisk`_ guarantees that you're always running the correct
   version of Bazel for your project, as defined in your project's
   ``.bazelversion`` file. It would technically be OK to use the
   ``bazel`` command in your GitHub Actions code because the
   ``bazel-contrib/setup-bazel`` extension also makes sure to launch
   the correct version of Bazel based on what's defined in ``.bazelversion``,
   but in practice Pigweed finds it safer to just use ``bazelisk`` everywhere.

.. _docs-github-actions-postsubmit:

--------------------------
Create a postsubmit Action
--------------------------
A postsubmit Action runs after a pull request has been merged.
The process for creating a postsubmit Action that builds and tests your code
when a new commit is pushed is almost identical to
:ref:`the presubmit Action setup <docs-github-actions-presubmit>`. The
only thing that changes is the ``on`` field in the YAML:

.. code-block:: yaml

   name: postsubmit
   run-name: postsubmit run

   on:
     push

   jobs:
     bazel-build-test:
       runs-on: ubuntu-latest
       steps:
         - name: Checkout
           # Check out this repo's code.
           # https://github.com/actions/checkout
           uses: actions/checkout@v4
           with:
             fetch-depth: 0
             submodules: recursive
         - name: Get Bazel
           # Install Bazel.
           # https://github.com/bazel-contrib/setup-bazel
           uses: bazel-contrib/setup-bazel@0.8.1
           with:
             # Avoid downloading Bazel every time.
             bazelisk-cache: true
             # Store build cache per workflow.
             disk-cache: ${{ github.workflow }}
             # Share repository cache between workflows.
             repository-cache: true
         - name: Bazel Build
           # Always use bazelisk rather than bazel to
           # guarantee that the correct version of Bazel
           # (sourced from .bazelversion) is used.
           run: bazelisk build ...
         - name: Bazel Test
           run: bazelisk test ...

.. _docs-github-actions-linter:

--------------------------------------------------------------
Create a linter Action that uses pw_presubmit and pw_env_setup
--------------------------------------------------------------
The following code demonstrates a presubmit linter Action that uses
:ref:`module-pw_env_setup` and :ref:`module-pw_presubmit`.

.. code-block:: yaml

   name: lintformat

   on:
     pull_request:
       types: [opened, synchronize, reopened]

   jobs:
     bazel-build-test:
       runs-on: ubuntu-latest
       steps:
         - name: Checkout
           uses: actions/checkout@v4
           with:
             fetch-depth: 0
             submodules: recursive
         - name: Bootstrap
           # When run locally, bootstrap.sh has checks to ensure that
           # it's sourced (source bootstrap.sh) rather than executed
           # directly. run.sh gets around this.
           run: pw_env_setup/run.sh bootstrap.sh
         - name: lintformat
           run: pw presubmit --program lintformat --keep-going

.. _understood by GitHub: https://docs.github.com/en/actions/using-workflows/workflow-commands-for-github-actions#setting-an-environment-variable

When ``pw_env_setup`` is run within a GitHub Action, it recognizes this from
the environment and writes the environment variables in a way that is
`understood by GitHub`_, and GitHub makes those variables available to
subsequent steps.

-------------------
Create more Actions
-------------------
.. _official GitHub Actions docs: https://docs.github.com/en/actions

You can create as many Actions as you want! Just add new files to
``//.github/workflows`` and tweak the options as needed. Check out
the `official GitHub Actions docs`_ to learn more.

.. _docs-github-actions-support:

-------
Support
-------
Please start a discussion in Pigweed's `Discord <https://discord.gg/M9NSeTA>`_.
