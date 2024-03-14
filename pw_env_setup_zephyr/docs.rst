.. _module-pw_env_setup_zephyr:

.. rst-class:: with-subtitle

===================
pw_env_setup_zephyr
===================
.. pigweed-module::
   :name: pw_env_setup_zephyr

   * **Integrate your Pigweed environment with Zephyr**

---------------------
Zephyr kick the tires
---------------------
The ultimate goal of this module is to provide tools and utilities for
integrating your Zephyr + Pigweed project. Some code in this python module will
be used in the ``pw`` CLI, while other code will be leveraged by other
utilities. This module is experimental so watch out for changes.

Check out the Zephyr getting started guide at :ref:`docs-os-zephyr`.

-----------
CLI options
-----------
This module offers a CLI integrated with the ``pw`` command as ``pw west`` which
provides an easy way to work with both Zephyr + Pigweed:

.. list-table::
   :header-rows: 1

   * - Flag
     - Description
   * - ``-v``
     - Print verbose messages (debug)

.. list-table::
   :header-rows: 1

   * - Subcommand
     - Description
   * - ``manifest``
     - Print a simple West manifest which includes this instance of Pigweed and
       optionally the bundled version of Zephyr if ``pw package install zephyr``
       was run.
