.. _chapter-pw-env_setup:

------------
pw_env_setup
------------
A classic problem in the embedded space is reducing the time from git clone
to having a binary executing on a device. The issue is that an entire suite
of tools is needed for non-trivial production embedded projects. For example:

 - A C++ compiler for your target device, and also for your host
 - A build system or three; for example, GN, Ninja, CMake, Bazel
 - A code formatting program like clang-format
 - A debugger like OpenOCD to flash and debug your embedded device
 - A known Python version with known modules installed for scripting
 - A Go compiler for the Go-based command line tools

...and so on

In the server space, container solutions like Docker or Podman solve this;
however, in our experience container solutions are a mixed bag for embedded
systems development where one frequently needs access to native system
resources like USB devices, or must operate on Windows.

``pw_env_setup`` is our compromise solution for this problem that works on Mac,
Windows, and Linux. It leverages the Chrome packaging system `CIPD`_ to
bootstrap a Python installation, which in turn inflates a virtual
environment. The tooling is installed into your workspace, and makes no
changes to your system. This tooling is designed to be reused by any
project.

Users interact with  ``pw_env_setup`` with two commands: ``. bootstrap.sh`` and
``. activate.sh``. The bootstrap command always pulls down the current versions
of CIPD packages and sets up the Python virtual environment. The activate
command reinitializes a previously configured environment, and if none is found,
runs bootstrap.

.. _CIPD: https://github.com/luci/luci-go/tree/master/cipd

.. note::
  On Windows the scripts used to set up the environment are ``bootstrap.bat``
  and ``activate.bat``. For simplicity they will be referred to with the ``.sh``
  endings unless the distinction is relevant.

By default packages will be installed in a ``.environment`` folder within the
checkout root, and CIPD will cache files in ``$HOME/.cipd-cache-dir``. These
paths can be overridden by setting ``PW_ENVIRONMENT_ROOT`` and
``CIPD_CACHE_DIR``, respectively.

.. warning::
  At this time ``pw_env_setup`` works for us, but isn’t well tested. We don’t
  suggest relying on it just yet. However, we are interested in experience
  reports; if you give it a try, please `send us a note`_ about your
  experience.

.. _send us a note: pigweed@googlegroups.com

==================================
Using pw_env_setup in your project
==================================

Projects using Pigweed can leverage ``pw_env_setup`` to install their own
dependencies. The following environment variables are now used to pass options
into pw_env_setup.

    * ``PW_CIPD_PACKAGE_FILES``
    * ``PW_VIRTUALENV_REQUIREMENTS``
    * ``PW_VIRTUALENV_SETUP_PY_ROOTS``
    * ``PW_CARGO_PACKAGE_FILES``

Each of these variables can contain multiple entries separated by ``:``
(or ``;`` on Windows) like the ``PATH`` environment variable. However, they
will also be interpreted as globs, so
``PW_VIRTUALENV_REQUIREMENTS="/foo/bar/*/requirements.txt"`` is perfectly
valid. They should be full paths.

Projects depending on Pigweed should set these variables and then invoke
Pigweed's ``bootstrap.sh`` (or ``bootstrap.bat``), which will add to each of
these variables before invoking ``pw_env_setup``. Users wanting additional
setup can set these variables in their shell init files. Pigweed will add to
these variables and will not remove any existing values. At the end of
Pigweed's bootstrap process, it will reset these variables to their initial
values.

An example of what your project's `bootstrap.sh` could look like is below. This
assumes `bootstrap.sh` is at the top level of your repository.

.. code-block:: bash

  # Do not include a "#!" line, this must be sourced and not executed.

  # This assumes the user is sourcing this file from it's parent directory. See
  # below for a more flexible way to handle this.
  PROJ_SETUP_SCRIPT_PATH="$(pwd)/bootstrap.sh"

  export PROJ_ROOT="$(_python_abspath "$(dirname "$PROJ_SETUP_SCRIPT_PATH")")"

  # You may wish to check if the user is attempting to execute this script
  # instead of sourcing it. See below for an example of how to handle that
  # situation.

  # Add the project-specific CIPD manifest.
  PW_CIPD_PACKAGE_FILES="$PROJ_ROOT/tools/cipd.json:$PW_CIPD_PACKAGE_FILES"
  export PW_CIPD_PACKAGE_FILES

  # Add the tools folder of this repository to the search path for setup.py
  # files.
  PW_VIRTUALENV_SETUP_PY_ROOTS="$PROJ_ROOT/tools:$PW_VIRTUALENV_SETUP_PY_ROOTS"
  export PW_VIRTUALENV_SETUP_PY_ROOTS

  # Process the project-specific requirements.txt file.
  PW_VIRTUALENV_REQUIREMENTS="$PROJ_ROOT/tools/requirements.txt:$PW_VIRTUALENV_REQUIREMENTS"
  export PW_VIRTUALENV_REQUIREMENTS

  # Source Pigweed's bootstrap script.
  # Using '.' instead of 'source' for dash compatibility. Since users don't use
  # dash directly, using 'source' in documentation so users don't get confused
  # and try to `./bootstrap.sh`.
  . "$PROJ_ROOT/third_party/pigweed/$(basename "$PROJ_SETUP_SCRIPT_PATH")"

User-Friendliness
*****************

You may wish to allow sourcing `bootstrap.sh` from a different directory. In
that case you'll need the following at the top of `bootstrap.sh`.

.. code-block:: bash

  _python_abspath () {
    python -c "import os.path; print(os.path.abspath('$@'))"
  }

  # Use this code from Pigweed's bootstrap to find the path to this script when
  # sourced. This should work with common shells. PW_CHECKOUT_ROOT is only used in
  # presubmit tests with strange setups, and can be omitted if you're not using
  # Pigweed's automated testing infrastructure.
  if test -n "$PW_CHECKOUT_ROOT"; then
    PROJ_SETUP_SCRIPT_PATH="$(_python_abspath "$PW_CHECKOUT_ROOT/bootstrap.sh")"
    unset PW_CHECKOUT_ROOT
  # Shell: bash.
  elif test -n "$BASH"; then
    PROJ_SETUP_SCRIPT_PATH="$(_python_abspath "$BASH_SOURCE")"
  # Shell: zsh.
  elif test -n "$ZSH_NAME"; then
    PROJ_SETUP_SCRIPT_PATH="$(_python_abspath "${(%):-%N}")"
  # Shell: dash.
  elif test ${0##*/} = dash; then
    PROJ_SETUP_SCRIPT_PATH="$(_python_abspath \
      "$(lsof -p $$ -Fn0 | tail -1 | sed 's#^[^/]*##;')")"
  # If everything else fails, try $0. It could work.
  else
    PROJ_SETUP_SCRIPT_PATH="$(_python_abspath "$0")"
  fi

You may also wish to check if the user is attempting to execute `bootstrap.sh`
instead of sourcing it. Executing `bootstrap.sh` would download everything
required for the environment, but cannot modify the environment of the parent
process. To check for this add the following.

.. code-block:: bash

  # Check if this file is being executed or sourced.
  _pw_sourced=0
  # If not running in Pigweed's automated testing infrastructure the
  # SWARMING_BOT_ID check is unnecessary.
  if [ -n "$SWARMING_BOT_ID" ]; then
    # If set we're running on swarming and don't need this check.
    _pw_sourced=1
  elif [ -n "$ZSH_EVAL_CONTEXT" ]; then
    case $ZSH_EVAL_CONTEXT in *:file) _pw_sourced=1;; esac
  elif [ -n "$KSH_VERSION" ]; then
    [ "$(cd $(dirname -- $0) && pwd -P)/$(basename -- $0)" != \
      "$(cd $(dirname -- ${.sh.file}) && pwd -P)/$(basename -- ${.sh.file})" ] \
      && _pw_sourced=1
  elif [ -n "$BASH_VERSION" ]; then
    (return 0 2>/dev/null) && _pw_sourced=1
  else  # All other shells: examine $0 for known shell binary filenames
    # Detects `sh` and `dash`; add additional shell filenames as needed.
    case ${0##*/} in sh|dash) _pw_sourced=1;; esac
  fi

  if [ "$_pw_sourced" -eq 0 ]; then
    _S_NAME=$(basename "$PROJ_SETUP_SCRIPT_PATH" .sh)
    echo "Error: Attempting to $_S_NAME in a subshell"
    echo "  Since $_S_NAME.sh modifies your shell's environment variables, it"
    echo "  must be sourced rather than executed. In particular, "
    echo "  'bash $_S_NAME.sh' will not work since the modified environment "
    echo "  will get destroyed at the end of the script. Instead, source the "
    echo "  script's contents in your shell:"
    echo ""
    echo "    \$ source $_S_NAME.sh"
    exit 1
  fi

Implementation
**************

The environment is set up by installing CIPD and Python packages in
``PW_ENVIRONMENT_ROOT`` or ``<checkout>/.environment``, and saving modifications
to environment variables in setup scripts in those directories. To support
multiple operating systems this is done in an operating system-agnostic manner
and then written into operating system-specific files to be sourced now and in
the future when running ``activate.sh`` instead of ``bootstrap.sh``. In the
future these could be extended to C shell and PowerShell. A logical mapping of
high-level commands to system-specific initialization files is shown below.

.. image:: doc_resources/pw_env_setup_output.png
   :alt: Mapping of high-level commands to system-specific commands.
   :align: left
