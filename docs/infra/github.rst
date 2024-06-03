.. _docs-github:

===========
GitHub
===========

Overview
========

Pigweed itself uses `LUCI <luci>`_ for
:abbr:`CI/CD (Continuous Integration/Continuous Deployment)`. LUCI is
well-supported within Google but complicated to deploy, so Pigweed has support
for `GitHub Actions <github-actions>`_ as well.

.. _github-actions: https://docs.github.com/en/actions

Bazel
=====
Configuring a Bazel builder that runs on pull requests is straightforward.
There are four steps: `checkout`_, `get Bazel`_, build, and test. There
are good community-managed actions for the first two steps, and the last two
steps are trivial (and can even be combined).

.. _checkout: https://github.com/marketplace/actions/checkout
.. _get Bazel: https://github.com/marketplace/actions/setup-bazel-environment

The Bazel version retrieved is configured through a ``.bazelversion`` file in
the root of the checkout.

.. code-block:: yaml

   name: basic
   run-name: presubmit run triggered by ${{ github.actor }}

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
         - name: Get Bazel
           uses: bazel-contrib/setup-bazel@0.8.1
           with:
             # Avoid downloading Bazel every time.
             bazelisk-cache: true
             # Store build cache per workflow.
             disk-cache: ${{ github.workflow }}
             # Share repository cache between workflows.
             repository-cache: true
         - name: Bazel Build
           run: bazel build ...
         - name: Bazel Test
           run: bazel test ...

``pw presubmit``
================
Configuring a builder that uses `pw_env_setup <module-pw_env_setup>`_ and
`pw_presubmit <module-pw_presubmit>`_ is also relatively simple. When run
locally, ``bootstrap.sh`` has several checks to ensure it's "sourced" instead of
directly executed. A `trivial wrapper script` is included in ``pw_env_setup``
that gets around this.

.. _trivial wrapper script: https://cs.opensource.google/pigweed/pigweed/+/main:pw_env_setup/run.sh

When ``pw_env_setup`` is run within a GitHub Action, it recognizes this from the
environment and writes the environment variables in a way
`understood by GitHub`_, and GitHub makes those variables available to
subsequent steps.

.. _understood by GitHub: https://docs.github.com/en/actions/using-workflows/workflow-commands-for-github-actions#setting-an-environment-variable

For Pigweed itself, the only ``pw_presubmit``-based action runs lintformat. To
test all of Pigweed requires more resources than the free GitHub runners
provide, but small projects using just part of Pigweed may be more successful.

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
           run: pw_env_setup/run.sh bootstrap.sh
         - name: python_format
           run: pw presubmit --program lintformat --keep-going

Note that Pigweed does not accept pull requests on GitHub, so we do not define
GitHub actions that trigger from pull requests. Instead, we trigger on pushes to
GitHub.
