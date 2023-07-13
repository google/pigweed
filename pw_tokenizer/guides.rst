.. _module-pw_tokenizer-guides:

======
Guides
======
.. pigweed-module-subpage::
   :name: pw_tokenizer
   :tagline: Cut your log sizes in half
   :nav:
      getting started: module-pw_tokenizer-get-started
      design: module-pw_tokenizer-design
      api: module-pw_tokenizer-api
      cli: module-pw_tokenizer-cli

.. _module-pw_tokenizer-get-started:

---------------
Getting started
---------------
Integrating ``pw_tokenizer`` requires a few steps beyond building the code. This
section describes one way ``pw_tokenizer`` might be integrated with a project.
These steps can be adapted as needed.

#. Add ``pw_tokenizer`` to your build. Build files for GN, CMake, and Bazel are
   provided. For Make or other build systems, add the files specified in the
   BUILD.gn's ``pw_tokenizer`` target to the build.
#. Use the tokenization macros in your code. See :ref:`module-pw_tokenizer-api-tokenization`.
#. Add the contents of ``pw_tokenizer_linker_sections.ld`` to your project's
   linker script. In GN and CMake, this step is done automatically.
#. Compile your code to produce an ELF file.
#. Run ``database.py create`` on the ELF file to generate a CSV token
   database. See :ref:`module-pw_tokenizer-managing-token-databases`.
#. Commit the token database to your repository. See notes in
   :ref:`module-pw_tokenizer-database-management`.
#. Integrate a ``database.py add`` command to your build to automatically update
   the committed token database. In GN, use the ``pw_tokenizer_database``
   template to do this. See :ref:`module-pw_tokenizer-update-token-database`.
#. Integrate ``detokenize.py`` or the C++ detokenization library with your tools
   to decode tokenized logs. See :ref:`module-pw_tokenizer-detokenization`.

Using with Zephyr
=================
When building ``pw_tokenizer`` with Zephyr, 3 Kconfigs can be used currently:

* ``CONFIG_PIGWEED_TOKENIZER`` will automatically link ``pw_tokenizer`` as well
  as any dependencies.
* ``CONFIG_PIGWEED_TOKENIZER_BASE64`` will automatically link
  ``pw_tokenizer.base64`` as well as any dependencies.
* ``CONFIG_PIGWEED_DETOKENIZER`` will automatically link
  ``pw_tokenizer.decoder`` as well as any dependencies.

Once enabled, the tokenizer headers can be included like any Zephyr headers:

.. code-block:: cpp

   #include <pw_tokenizer/tokenize.h>

.. note::
  Zephyr handles the additional linker sections via
  ``pw_tokenizer_zephyr.ld`` which is added to the end of the linker file
  via a call to ``zephyr_linker_sources(SECTIONS ...)``.

.. _module-pw_tokenizer-domains:

---------------------------------------------------------------------
Keep tokens from different sources separate with tokenization domains
---------------------------------------------------------------------
``pw_tokenizer`` supports having multiple tokenization domains. Domains are a
string label associated with each tokenized string. This allows projects to keep
tokens from different sources separate. Potential use cases include the
following:

* Keep large sets of tokenized strings separate to avoid collisions.
* Create a separate database for a small number of strings that use truncated
  tokens, for example only 10 or 16 bits instead of the full 32 bits.

If no domain is specified, the domain is empty (``""``). For many projects, this
default domain is sufficient, so no additional configuration is required.

.. code-block:: cpp

   // Tokenizes this string to the default ("") domain.
   PW_TOKENIZE_STRING("Hello, world!");

   // Tokenizes this string to the "my_custom_domain" domain.
   PW_TOKENIZE_STRING_DOMAIN("my_custom_domain", "Hello, world!");

The database and detokenization command line tools default to reading from the
default domain. The domain may be specified for ELF files by appending
``#DOMAIN_NAME`` to the file path. Use ``#.*`` to read from all domains. For
example, the following reads strings in ``some_domain`` from ``my_image.elf``.

.. code-block:: sh

   ./database.py create --database my_db.csv path/to/my_image.elf#some_domain

See :ref:`module-pw_tokenizer-managing-token-databases` for information about
the ``database.py`` command line tool.

.. _module-pw_tokenizer-managing-token-databases:

------------------------
Managing token databases
------------------------
Background: :ref:`module-pw_tokenizer-token-databases`

Token databases are managed with the ``database.py`` script. This script can be
used to extract tokens from compilation artifacts and manage database files.
Invoke ``database.py`` with ``-h`` for full usage information.

An example ELF file with tokenized logs is provided at
``pw_tokenizer/py/example_binary_with_tokenized_strings.elf``. You can use that
file to experiment with the ``database.py`` commands.

.. _module-pw_tokenizer-database-creation:

Create a database
=================
The ``create`` command makes a new token database from ELF files (.elf, .o, .so,
etc.), archives (.a), existing token databases (CSV or binary), or a JSON file
containing an array of strings.

.. code-block:: sh

   ./database.py create --database DATABASE_NAME ELF_OR_DATABASE_FILE...

Two database output formats are supported: CSV and binary. Provide
``--type binary`` to ``create`` to generate a binary database instead of the
default CSV. CSV databases are great for checking into a source control or for
human review. Binary databases are more compact and simpler to parse. The C++
detokenizer library only supports binary databases currently.

.. _module-pw_tokenizer-update-token-database:

Update a database
=================
As new tokenized strings are added, update the database with the ``add``
command.

.. code-block:: sh

   ./database.py add --database DATABASE_NAME ELF_OR_DATABASE_FILE...

This command adds new tokens from ELF files or other databases to the database.
Adding tokens already present in the database updates the date removed, if any,
to the latest.

A CSV token database can be checked into a source repository and updated as code
changes are made. The build system can invoke ``database.py`` to update the
database after each build.

GN integration
==============
Token databases may be updated or created as part of a GN build. The
``pw_tokenizer_database`` template provided by
``$dir_pw_tokenizer/database.gni`` automatically updates an in-source tokenized
strings database or creates a new database with artifacts from one or more GN
targets or other database files.

To create a new database, set the ``create`` variable to the desired database
type (``"csv"`` or ``"binary"``). The database will be created in the output
directory. To update an existing database, provide the path to the database with
the ``database`` variable.

.. code-block::

   import("//build_overrides/pigweed.gni")

   import("$dir_pw_tokenizer/database.gni")

   pw_tokenizer_database("my_database") {
     database = "database_in_the_source_tree.csv"
     targets = [ "//firmware/image:foo(//targets/my_board:some_toolchain)" ]
     input_databases = [ "other_database.csv" ]
   }

Instead of specifying GN targets, paths or globs to output files may be provided
with the ``paths`` option.

.. code-block::

   pw_tokenizer_database("my_database") {
     database = "database_in_the_source_tree.csv"
     deps = [ ":apps" ]
     optional_paths = [ "$root_build_dir/**/*.elf" ]
   }

.. note::

   The ``paths`` and ``optional_targets`` arguments do not add anything to
   ``deps``, so there is no guarantee that the referenced artifacts will exist
   when the database is updated. Provide ``targets`` or ``deps`` or build other
   GN targets first if this is a concern.

CMake integration
=================
Token databases may be updated or created as part of a CMake build. The
``pw_tokenizer_database`` template provided by
``$dir_pw_tokenizer/database.cmake`` automatically updates an in-source tokenized
strings database or creates a new database with artifacts from a CMake target.

To create a new database, set the ``CREATE`` variable to the desired database
type (``"csv"`` or ``"binary"``). The database will be created in the output
directory.

.. code-block::

   include("$dir_pw_tokenizer/database.cmake")

   pw_tokenizer_database("my_database") {
     CREATE binary
     TARGET my_target.ext
     DEPS ${deps_list}
   }

To update an existing database, provide the path to the database with
the ``database`` variable.

.. code-block::

   pw_tokenizer_database("my_database") {
     DATABASE database_in_the_source_tree.csv
     TARGET my_target.ext
     DEPS ${deps_list}
   }
