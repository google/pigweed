.. _module-pw_doctor:

---------
pw_doctor
---------
``pw doctor`` confirms the environment is set up correctly. With ``--strict``
it checks that things exactly match what is expected and it checks that things
look compatible without.

Projects that adjust the behavior of pw_env_setup may need to customize
these checks, but unfortunately this is not generally supported yet.

Checks carried out by pw_doctor include:

* The bootstrapped OS matches the current OS.
* ``PW_ROOT`` is defined and points to the root of the Pigweed repo.

  - If your copy of pigweed is intentionally vendored and not a git repo (or
    submodule), set ``PW_DISABLE_ROOT_GIT_REPO_CHECK=1`` during bootstrap to
    suppress the anti-vendoring portion of this check.

* The presubmit git hook is installed.
* Python is one of the :ref:`supported versions <docs-concepts-python-version>`.
* The Pigweed virtual env is active.
* CIPD is set up correctly and in use.
* The CIPD packages required by Pigweed are up to date.
* The platform support symlinks.

.. note::
  The documentation for this module is currently incomplete.

Configuration
=============
Options for ``pw doctor`` can be specified in the ``pigweed.json`` file
(see also :ref:`SEED-0101 <seed-0101>`). This is currently limited to one
option.

* ``new_bug_url``: What link is given to users be given for filing bugs. By
  default this is to the `Pigweed Bug Tracker_`.

.. _Pigweed Bug Tracker: https://issues.pigweed.dev/new

.. code-block::

   {
     "pw": {
       "pw_doctor": {
         "new_bug_url": "https://example.com/bugs/new"
       }
     }
   }

