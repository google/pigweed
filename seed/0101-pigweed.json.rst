.. _seed-0101:

==================
0101: pigweed.json
==================

.. card::
   :fas:`seedling` SEED-0101: :ref:`pigweed.json<seed-0101>`

   :octicon:`comment-discussion` Status:
   :bdg-primary:`Open for Comments`
   :octicon:`chevron-right`
   :bdg-secondary-line:`Last Call`
   :octicon:`chevron-right`
   :bdg-secondary-line:`Accepted`
   :octicon:`kebab-horizontal`
   :bdg-secondary-line:`Rejected`

   :octicon:`calendar` Proposal Date: 2023-02-06

   :octicon:`code-review` CL: `pwrev/128010 <https://pigweed-review.git.corp.google.com/c/pigweed/pigweed/+/128010>`_

-------
Summary
-------
Combine several of the configuration options downstream projects use to
configure parts of Pigweed in one place, and use this place for further
configuration options.

----------
Motivation
----------
Pigweed-based projects configure Pigweed and themselves in a variety of ways.
The environment setup is controlled by a JSON file that's referenced in
``bootstrap.sh`` files and in internal infrastructure repos that looks
something like this:

.. code-block::

  {
    "root_variable": "<PROJNAME>_ROOT",
    "cipd_package_files": ["tools/default.json"],
    "virtualenv": {
      "gn_args": ["dir_pw_third_party_stm32cube=\"\""],
      "gn_root": ".",
      "gn_targets": [":python.install"]
    },
    "optional_submodules": ["vendor/shhh-secret"],
    "gni_file": "build_overrides/pigweed_environment.gni"
  }

The plugins to the ``pw`` command-line utility are configured in ``PW_PLUGINS``,
which looks like this:

.. code-block::

  # <name> <Python module> <function>
  console pw_console.__main__ main
  format pw_presubmit.format_code _pigweed_upstream_main

In addition, changes have been proposed to configure some of the behavior of
``pw format`` and the formatting steps of ``pw presubmit`` from config files,
but there's no standard place to put these configuration options.

---------------
Guide reference
---------------
This proposal affects two sets of people: developers looking to use Pigweed,
and developers looking to add configurable features to Pigweed.

Developers looking to use Pigweed will have one config file that contains all
the options they need to set. Documentation for individual Pigweed modules will
show only the configuration options relevant for that module, and multiple of
these examples can simply be concatenated to form a valid config file.

Developers looking to add configurable features to Pigweed no longer need to
define a new file format, figure out where to find it in the tree (or how to
have Pigweed-projects specify a location), or parse this format.

---------------------
Problem investigation
---------------------
There are multiple issues with the current system that need to be addressed.

* ``PW_PLUGINS`` works, but is a narrow custom format with exactly one purpose.
* The environment config file is somewhat extensible, but is still specific to
  environment setup.
* There's no accepted place for other modules to retrieve configuration options.

These should be combined into a single file. There are several formats that
could be selected, and many more arguments for and against each. Only a subset
of these arguments are reproduced here.

* JSON does not support comments
* JSON5 is not supported in the Python standard library
* XML is too verbose
* YAML is acceptable, but implicit type conversion could be a problem, and it's
  not supported in the Python standard library
* TOML is acceptable, and `was selected for a similar purpose by Python
  <https://snarky.ca/what-the-heck-is-pyproject-toml/>`_, but it's
  not supported in the Python standard library before Python v3.11
* Protobuf Text Format is acceptable and widely used within Google, but is not
  supported in the Python standard library

The location of the file is also an issue. Environment config files can be found
in a variety of locations depending on the project—all of the following paths
are used by at least one internal Pigweed-based project.

* ``build/environment.json``
* ``build/pigweed/env_setup.json``
* ``environment.json``
* ``env_setup.json``
* ``pw_env_setup.json``
* ``scripts/environment.json``
* ``tools/environment.json``
* ``tools/env_setup.json``

``PW_PLUGINS`` files can in theory be in any directory and ``pw`` will search up
for them from the current directory, but in practice they only exist at the root
of checkouts. Having this file in a fixed location with a fixed name makes it
significantly easier to find as a user, and the fixed name (if not path) makes
it easy to find programmatically too.

---------------
Detailed design
---------------
The ``pw_env_setup`` Python module will provide an API to retrieve a parsed
``pigweed.json`` file. ``pw_env_setup`` is the correct location because it can't
depend on anything else, but other modules can depend on it. Code in other
languages does not yet depend on configuration files.

A ``pigweed.json`` file might look like the following. Individual option names
and structures are not final but will evolve as those options are
implemented—this is merely an example of what an actual file could look like.
The ``pw`` namespace is reserved for Pigweed, but other projects can use other
namespaces for their own needs. Within the ``pw`` namespace all options are
first grouped by their module name, which simplifies searching for the code and
documentation related to the option in question.

.. code-block::

  {
    "pw": {
      "pw_cli": {
        "plugins": {
          "console": {
            "module": "pw_console.__main__",
            "function": "main"
          },
          "format": {
            "module": "pw_presubmit.format_code",
            "function": "_pigweed_upstream_main"
          }
        }
      },
      "pw_env_setup": {
        "root_variable": "<PROJNAME>_ROOT",
        "rosetta": "allow",
        "gni_file": "build_overrides/pigweed_environment.gni",
        "cipd": {
          "package_files": [
            "tools/default.json"
          ]
        },
        "virtualenv": {
          "gn_args": [
            "dir_pw_third_party_stm32cube=\"\""
          ],
          "gn_targets": [
            "python.install"
          ],
          "gn_root": "."
        },
        "submodules": {
          "optional": [
            "vendor/shhh-secret"
          ]
        }
      },
      "pw_presubmit": {
        "format": {
          "python": {
            "formatter": "black",
            "black_path": "pyink"
          }
        }
      }
    }
  }

Some teams will resist a new file at the root of their checkout, but this seed
won't be adding any files, it'll be combining at least one top-level file, maybe
two, into a new top-level file, so there won't be any additional files in the
checkout root.

------------
Alternatives
------------
``pw format`` and the formatting steps of ``pw presubmit`` could read from yet
another config file, further fracturing Pigweed's configuration.

A different file format could be chosen over JSON. Since JSON is parsed into
only Python lists, dicts, and primitives, switching to another format that can
be parsed into the same internal structure should be trivial.

--------------
Open questions
--------------
None?
