.. _module-pw_cli_analytics:

----------------
pw_cli_analytics
----------------
.. pigweed-module::
   :name: pw_cli_analytics

This module collects and transmits analytics on usage of the ``pw`` command line
interface. This will help the Pigweed team improve its command line utilities.
By default this will be enabled, but it can be disabled on a per-project,
per-user, or per-project/per-user basis. Individuals can completely opt-out by
running ``pw cli-analytics --opt-out``, and can opt-in by running
``pw cli-analytics --opt-in``.

Analytics
=========
Pigweed collects analytics on the use of the ``pw`` command. For projects using
Pigweed the default is to just collect details about the host machine
(operating system, CPU architecture, and CPU core count), but no data about the
exact command run or the project in question. This can be disabled by setting
``enabled`` to false in the files listed below. The default values for all the
project-specific analytics options are below.

For development on Pigweed itself, these options are all specifically enabled.

``debug_url``
  Google Analytics URL to use when debugging. Defaults to
  https://www.google-analytics.com/debug/mp/collect.

``prod_url``
  Google Analytics URL to use in production. Defaults to
  https://www.google-analytics.com/mp/collect.

``report_command_line``
  If ``true``, the report will include the full command line with all arguments.
  The home directory and any references to the username will be masked.

``report_project_name``
  If ``true``, the ``root_variable`` entry from ``pigweed.json`` will be
  reported. This is the name of the root variable, not its value.

``report_remote_url``
  If ``true``, the remote URL will be reported.

``report_subcommand_name``
  ``never``
    The subcommand name will not be reported.

  ``limited``
    The subcommand name will be reported if and only if it's one of the
    subcommands defined in Pigweed itself. These are ``analytics``, ``bloat``,
    ``build``, ``console``, ``doctor``, ``emu``, ``format``, ``heap-viewer``,
    ``help``, ``ide``, ``keep-sorted``, ``logdemo``, ``module``, ``package``,
    ``presubmit``, ``python-packages``, ``requires``, ``rpc``, ``test``,
    ``update``, and ``watch``.

  ``always``
    The subcommand name will always be reported.

.. code-block:: json

   {
     "pw": {
       "pw_cli_analytics": {
         "prod_url": "https://www.google-analytics.com/mp/collect",
         "debug_url": "https://www.google-analytics.com/debug/mp/collect",
         "report_command_line": false,
         "report_project_name": false,
         "report_remote_url": false,
         "report_subcommand_name": "limited"
       }
     }
   }

There are three places these configs can be set, with increasing precedence:

* The project config, at ``<project-root>/pigweed.json``,
* The user/project config, at ``<project-root>/.pw_cli_analytics.user.json``,
  and
* The user config, at ``~/.pw_cli_analytics.json``.

The user config contains a UUID for the user. It's set to a new value when
opting in (manually or by running ``pw cli-analytics --opt-in``) and cleared
when opting out (with ``pw cli-analytics --opt-out``).

In general, decisions about what is shared about the project must come from the
project config, the unique id of the user must come from the user config, and
other details can be set in any config. Also, a ``false`` value for ``enabled``
can't be overridden.

In addition, if ``PW_DISABLE_CLI_ANALYTICS`` is set, then all reporting is
disabled.
