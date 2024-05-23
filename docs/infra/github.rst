.. _docs-github:

===========
GitHub
===========

--------
Overview
--------

Pigweed itself uses `LUCI <luci>`_ for
:abbr:`CI/CD (Continuous Integration/Continuous Deployment)`. LUCI is
well-supported within Google but complicated to deploy, so Pigweed has support
for `GitHub Actions <github-actions>`_ as well.

.. _github-actions: https://docs.github.com/en/actions

Bazel
=====
Configuring a Bazel builder that runs on pull requests is straightforward.
There are four steps: `checkout <github-actions-checkout>`_,
`get Bazel <github-actions-bazel>`_, build, and test. There are good
community-managed actions for the first two steps, and the last two steps are
trivial (and can even be combined).

.. _github-actions-checkout: https://github.com/marketplace/actions/checkout
.. _github-actions-bazel: https://github.com/marketplace/actions/setup-bazel-environment

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
Tests that require using `pw_env_setup <module-pw_env_setup>`_ and
`pw_presubmit <module-pw_presubmit>`_ are not yet supported.
