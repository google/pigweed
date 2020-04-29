.. _chapter-pw-presubmit:

.. default-domain:: cpp

.. highlight:: sh

------------
pw_presubmit
------------
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

.. image:: ../docs/images/pw_presubmit_demo.gif
   :alt: ``pw format`` demo
   :align: left

Compatibility
=============
Python 3

Presubmit tools
===============

Defining presubmit checks
-------------------------
A presubmit check is defined as a function or other callable. The function must
accept one argument: a ``PresubmitContext``, which provides the paths on which
to run. Presubmit checks communicate failure by raising any exception.

For example, either of these functions may be used as presubmit checks:

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

Presubmit checks may use the ``filter_paths`` decorator to automatically filter
the paths list for file types they care about.

See ``pigweed_presubmit.py`` for an example of how to define presubmit checks.

Key members
^^^^^^^^^^^
.. autofunction:: pw_presubmit.cli.add_arguments

.. autofunction:: pw_presubmit.cli.run

.. autofunction:: pw_presubmit.run_presubmit

.. autodecorator:: pw_presubmit.filter_paths

.. autofunction:: pw_presubmit.call

.. autoexception:: pw_presubmit.PresubmitFailure

.. autoexception:: pw_presubmit.Programs

Included presubmit checks
-------------------------
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

Code formatting tools
=====================
The ``pw_presubmit.format_code`` module formats supported source files using
external code format tools. The file ``format_code.py`` can be invoked directly
from the command line or from ``pw`` as ``pw format``.
