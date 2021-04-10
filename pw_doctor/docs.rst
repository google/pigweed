.. _module-pw_doctor:

---------
pw_doctor
---------
``pw doctor`` confirms the environment is set up correctly. With ``--strict``
it checks that things exactly match what is expected and it checks that things
look compatible without.

Currently pw_doctor expects the running python to be Python 3.8 or 3.9.

Projects that adjust the behavior of pw_env_setup may need to customize
these checks, but unfortunately this is not supported yet.

.. note::
  The documentation for this module is currently incomplete.
