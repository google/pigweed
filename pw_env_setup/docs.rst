.. _module-pw_env_setup:

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

.. _CIPD: https://github.com/luci/luci-go/tree/master/cipd

Users interact with  ``pw_env_setup`` with two commands: ``. bootstrap.sh`` and
``. activate.sh``. The bootstrap command always pulls down the current versions
of CIPD packages and sets up the Python virtual environment. The activate
command reinitializes a previously configured environment, and if none is found,
runs bootstrap.

.. note::
  On Windows the scripts used to set up the environment are ``bootstrap.bat``
  and ``activate.bat``. For simplicity they will be referred to with the ``.sh``
  endings unless the distinction is relevant.

.. warning::
  At this time ``pw_env_setup`` works for us, but isn’t well tested. We don’t
  suggest relying on it just yet. However, we are interested in experience
  reports; if you give it a try, please `send us a note`_ about your
  experience.

.. _send us a note: pigweed@googlegroups.com

==================================
Using pw_env_setup in your project
==================================

Downstream Projects Using Pigweed's Packages
********************************************

Projects using Pigweed can leverage ``pw_env_setup`` to install Pigweed's
dependencies or their own dependencies. Projects that only want to use Pigweed's
dependencies without modifying them can just source Pigweed's ``bootstrap.sh``
and ``activate.sh`` scripts.

An example of what your project's `bootstrap.sh` could look like is below. This
assumes `bootstrap.sh` is at the top level of your repository.

.. code-block:: bash

  # Do not include a "#!" line, this must be sourced and not executed.

  # This assumes the user is sourcing this file from it's parent directory. See
  # below for a more flexible way to handle this.
  PROJ_SETUP_SCRIPT_PATH="$(pwd)/bootstrap.sh"

  export PW_PROJECT_ROOT="$(_python_abspath "$(dirname "$PROJ_SETUP_SCRIPT_PATH")")"

  # You may wish to check if the user is attempting to execute this script
  # instead of sourcing it. See below for an example of how to handle that
  # situation.

  # Source Pigweed's bootstrap utility script.
  # Using '.' instead of 'source' for POSIX compatibility. Since users don't use
  # dash directly, using 'source' in most documentation so users don't get
  # confused and try to `./bootstrap.sh`.
  . "$PW_PROJECT_ROOT/third_party/pigweed/pw_env_setup/util.sh"

  pw_check_root "$PW_ROOT"
  _PW_ACTUAL_ENVIRONMENT_ROOT="$(pw_get_env_root)"
  export _PW_ACTUAL_ENVIRONMENT_ROOT
  SETUP_SH="$_PW_ACTUAL_ENVIRONMENT_ROOT/activate.sh"
  pw_bootstrap --args...  # See below for details about args.
  pw_finalize bootstrap "$SETUP_SH"

User-Friendliness
-----------------

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

  _pw_eval_sourced "$_pw_sourced"

Downstream Projects Using Different Packages
********************************************

Projects depending on Pigweed but using additional or different packages should
copy the Pigweed `sample project`'s ``bootstrap.sh`` and ``config.json`` and
update the call to ``pw_bootstrap``. Search for "downstream" for other places
that may require changes, like setting the ``PW_ROOT`` and ``PW_PROJECT_ROOT``
environment variables. Explanations of parts of ``config.json`` are described
here.

.. _sample project: https://pigweed.googlesource.com/pigweed/sample_project/+/master

``cipd_package_files``
  CIPD package file. JSON file consisting of a list of dictionaries with "path"
  and "tags" keys, where "tags" is a list of strings.

``virtualenv.gn_targets``
  Target for installing Python packages. Downstream projects will need to
  create targets to install their packages or only use Pigweed Python packages.

``virtualenv.gn_root``
  The root directory of your GN build tree, relative to ``PW_PROJECT_ROOT``.
  This is the directory your project's ``.gn`` file is located in. If you're
  only installing Pigweed Python packages, use the location of the Pigweed
  submodule.

An example of a config file is below.

.. code-block:: json

  {
    "cipd_package_files": [
      "pigweed/pw_env_setup/py/pw_env_setup/cipd_setup/pigweed.json",
      "pigweed/pw_env_setup/py/pw_env_setup/cipd_setup/luci.json"
      "tools/myprojectname.json"
    ],
    "virtualenv": {
      "gn_root": ".",
      "gn_targets": [
        ":python.install",
      ]
    }
  }

In case the CIPD packages need to be referenced from other scripts, variables
like ``PW_${BASENAME}_CIPD_INSTALL_DIR`` point to the CIPD install directories,
where ``${BASENAME}`` is "PIGWEED" for
"pigweed/pw_env_setup/py/pw_env_setup/cipd_setup/pigweed.json" and "LUCI" for
"pigweed/pw_env_setup/py/pw_env_setup/cipd_setup/luci.json". This example would
set the following environment variables.

 - ``PW_LUCI_CIPD_INSTALL_DIR``
 - ``PW_MYPROJECTNAME_CIPD_INSTALL_DIR``
 - ``PW_PIGWEED_CIPD_INSTALL_DIR``

Environment Variables
*********************
The following environment variables affect env setup behavior. Most users will
never need to set these.

``CIPD_CACHE_DIR``
  Location of CIPD cache dir. Defaults to ``$HOME/.cipd-cache-dir``.

``PW_ACTIVATE_SKIP_CHECKS``
  If set, skip running ``pw doctor`` at end of bootstrap/activate. Intended to
  be used by automated tools but not interactively.

``PW_BOOTSTRAP_PYTHON``
  Python executable to be used, for example "python2" or "python3". Defaults to
  "python".

``PW_ENVIRONMENT_ROOT``
  Location to which packages are installed. Defaults to ``.environment`` folder
  within the checkout root.

``PW_ENVSETUP_DISABLE_SPINNER``
  Disable the spinner during env setup. Intended to be used when the output is
  being redirected to a log.

``PW_ENVSETUP_QUIET``
  Disables all non-error output.

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
