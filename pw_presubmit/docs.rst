.. _module-pw_presubmit:

============
pw_presubmit
============
The presubmit module provides Python tools for running presubmit checks and
checking and fixing code format. It also includes the presubmit check script for
the Pigweed repository, ``pigweed_presubmit.py``.

Presubmit checks are essential tools, but they take work to set up, and
projects don’t always get around to it. The ``pw_presubmit`` module provides
tools for setting up high quality presubmit checks for any project. We use this
framework to run Pigweed’s presubmit on our workstations and in our automated
building tools.

The ``pw_presubmit`` module also includes ``pw format``, a tool that provides a
unified interface for automatically formatting code in a variety of languages.
With ``pw format``, you can format C, C++, Python, GN, and Go code according to
configurations defined by your project. ``pw format`` leverages existing tools
like ``clang-format``, and it’s simple to add support for new languages.

.. image:: docs/pw_presubmit_demo.gif
   :alt: ``pw format`` demo
   :align: left

The ``pw_presubmit`` package includes presubmit checks that can be used with any
project. These checks include:

* Check code format of several languages including C, C++, and Python
* Initialize a Python environment
* Run all Python tests
* Run pylint
* Run mypy
* Ensure source files are included in the GN and Bazel builds
* Build and run all tests with GN
* Build and run all tests with Bazel
* Ensure all header files contain ``#pragma once``

-------------
Compatibility
-------------
Python 3

-------------------------------------------
Creating a presubmit check for your project
-------------------------------------------
Creating a presubmit check for a project using ``pw_presubmit`` is simple, but
requires some customization. Projects must define their own presubmit check
Python script that uses the ``pw_presubmit`` package.

A project's presubmit script can be registered as a
:ref:`pw_cli <module-pw_cli>` plugin, so that it can be run as ``pw
presubmit``.

Setting up the command-line interface
-------------------------------------
The ``pw_presubmit.cli`` module sets up the command-line interface for a
presubmit script. This defines a standard set of arguments for invoking
presubmit checks. Its use is optional, but recommended.

pw_presubmit.cli
~~~~~~~~~~~~~~~~
.. automodule:: pw_presubmit.cli
   :members: add_arguments, run

Presubmit checks
----------------
A presubmit check is defined as a function or other callable. The function must
accept one argument: a ``PresubmitContext``, which provides the paths on which
to run. Presubmit checks communicate failure by raising an exception.

Presubmit checks may use the ``filter_paths`` decorator to automatically filter
the paths list for file types they care about.

Either of these functions could be used as presubmit checks:

.. code-block:: python

  @pw_presubmit.filter_paths(endswith='.py')
  def file_contains_ni(ctx: PresubmitContext):
      for path in ctx.paths:
          with open(path) as file:
              contents = file.read()
              if 'ni' not in contents and 'nee' not in contents:
                  raise PresumitFailure('Files must say "ni"!', path=path)

  def run_the_build(_):
      subprocess.run(['make', 'release'], check=True)

Presubmit checks functions are grouped into "programs" -- a named series of
checks. Projects may find it helpful to have programs for different purposes,
such as a quick program for local use and a full program for automated use. The
:ref:`example script <example-script>` uses ``pw_presubmit.Programs`` to define
``quick`` and ``full`` programs.

pw_presubmit
~~~~~~~~~~~~
.. automodule:: pw_presubmit
   :members: filter_paths, call, PresubmitFailure, Programs

.. _example-script:

Example
-------
A simple example presubmit check script follows. This can be copied-and-pasted
to serve as a starting point for a project's presubmit check script.

See ``pigweed_presubmit.py`` for a more complex presubmit check script example.

.. code-block:: python

  """Example presubmit check script."""

  import argparse
  import logging
  import os
  from pathlib import Path
  import re
  import sys
  from typing import List, Pattern

  try:
      import pw_cli.log
  except ImportError:
      print('ERROR: Activate the environment before running presubmits!',
            file=sys.stderr)
      sys.exit(2)

  import pw_presubmit
  from pw_presubmit import build, cli, environment, format_code, git_repo
  from pw_presubmit import python_checks, filter_paths, PresubmitContext
  from pw_presubmit.install_hook import install_hook

  # Set up variables for key project paths.
  PROJECT_ROOT = Path(os.environ['MY_PROJECT_ROOT'])
  PIGWEED_ROOT = PROJECT_ROOT / 'pigweed'

  #
  # Initialization
  #
  def init_cipd(ctx: PresubmitContext):
      environment.init_cipd(PIGWEED_ROOT, ctx.output_dir)


  def init_virtualenv(ctx: PresubmitContext):
      environment.init_virtualenv(PIGWEED_ROOT,
                                  ctx.output_dir,
                                  setup_py_roots=[PROJECT_ROOT])


  # Rerun the build if files with these extensions change.
  _BUILD_EXTENSIONS = frozenset(
      ['.rst', '.gn', '.gni', *format_code.C_FORMAT.extensions])


  #
  # Presubmit checks
  #
  def release_build(ctx: PresubmitContext):
      build.gn_gen(PROJECT_ROOT, ctx.output_dir, build_type='release')
      build.ninja(ctx.output_dir)


  def host_tests(ctx: PresubmitContext):
      build.gn_gen(PROJECT_ROOT, ctx.output_dir, run_host_tests='true')
      build.ninja(ctx.output_dir)


  # Avoid running some checks on certain paths.
  PATH_EXCLUSIONS = (
      re.compile(r'^external/'),
      re.compile(r'^vendor/'),
  )


  # Use the upstream pragma_once check, but apply a different set of path
  # filters with @filter_paths.
  @filter_paths(endswith='.h', exclude=PATH_EXCLUSIONS)
  def pragma_once(ctx: PresubmitContext):
      pw_presubmit.pragma_once(ctx)


  #
  # Presubmit check programs
  #
  QUICK = (
      # Initialize an environment for running presubmit checks.
      init_cipd,
      init_virtualenv,
      # List some presubmit checks to run
      pragma_once,
      host_tests,
      # Use the upstream formatting checks, with custom path filters applied.
      format_code.presubmit_checks(exclude=PATH_EXCLUSIONS),
  )

  FULL = (
      QUICK,  # Add all checks from the 'quick' program
      release_build,
      # Use the upstream Python checks, with custom path filters applied.
      python_checks.all_checks(exclude=PATH_EXCLUSIONS),
  )

  PROGRAMS = pw_presubmit.Programs(quick=QUICK, full=FULL)


  def run(install: bool, **presubmit_args) -> int:
      """Process the --install argument then invoke pw_presubmit."""

      # Install the presubmit Git pre-push hook, if requested.
      if install:
          install_hook(__file__, 'pre-push', ['--base', 'HEAD~'],
                       git_repo.root())
          return 0

      return cli.run(root=PROJECT_ROOT, **presubmit_args)


  def main() -> int:
      """Run the presubmit checks for this repository."""
      parser = argparse.ArgumentParser(description=__doc__)
      cli.add_arguments(parser, PROGRAMS, 'quick')

      # Define an option for installing a Git pre-push hook for this script.
      parser.add_argument(
          '--install',
          action='store_true',
          help='Install the presubmit as a Git pre-push hook and exit.')

      return run(**vars(parser.parse_args()))

  if __name__ == '__main__':
      pw_cli.log.install(logging.INFO)
      sys.exit(main())

---------------------
Code formatting tools
---------------------
The ``pw_presubmit.format_code`` module formats supported source files using
external code format tools. The file ``format_code.py`` can be invoked directly
from the command line or from ``pw`` as ``pw format``.
