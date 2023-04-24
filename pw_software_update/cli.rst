.. _module-pw_software_update-cli:

---------------------------------
pw_software_update: CLI reference
---------------------------------

Overview
---------

Use the ``pw_software_update`` CLI to quickly learn and prototype a software
update system on your development PC before productionizing one. In the future
you will be able to use the CLI to update a reference
target.

.. code-block:: bash

  ~$ cd pigweed
  ~/pigweed$ source ./activate.sh
  ~/pigweed$ pw update [-h] <command>

.. csv-table::
  :header: "Command", "Description"
  :widths: 30, 70
  :align: left

  ``generate-key``, "generates a local signing key"
  ``create-root-metadata``, "creates a TUF root metadata file"
  ``sign-root-metadata``, "signs a TUF root metadata"
  ``inspect-root-metadata``, "prints a TUF root metadata"
  ``create-empty-bundle``, "creates an empty update bundle"
  ``add-root-metadata-to-bundle``, "adds a root metadata to an existing bundle"
  ``add-file-to-bundle``, "adds a target file to an existing bundle"
  ``sign-bundle``, "signs an update bundle"
  ``inspect-bundle``, "prints an update bundle"
  ``verify-bundle``, "verifies an update bundle"

generate-key
------------

Generates an ECDSA SHA-256 public + private keypair.

.. code-block:: bash

  $ pw update generate-key [-h] pathname

.. csv-table::
   :header: "Argument", "Description"
   :widths: 30, 70
   :align: left

   ``pathname``, "output pathname for the new key pair"

create-root-metadata
--------------------

Creates a root metadata.

.. code-block:: bash

  $ pw update create-root-metadata [-h]
      [--version VERSION] \
      --append-root-key ROOT_KEY \
      --append-targets-key TARGETS_KEY \
      -o/--out OUT

.. csv-table::
  :header: "Option", "Description"
  :widths: 30, 70
  :align: left

  ``--append-root-key``, "path to root key (public)"
  ``--append-targets-key``, "path to targets key (public)"
  ``--out``, "output path of newly created root metadata"
  ``--version``, "anti-rollback version number of the root metadata (defaults to 1)"

sign-root-metadata
------------------

Signs a given root metadata.

.. code-block:: bash

  $ pw update sign-root-metadata [-h] \
      --root-metadata ROOT_METADATA \
      --root-key ROOT_KEY

.. csv-table::
  :header: "Option", "Description"
  :widths: 30, 70
  :align: left

  ``--root-metadata``, "Path of root metadata to be signed"
  ``--root-key``, "Path to root signing key (private)"

inspect-root-metadata
---------------------

Prints the contents of a given root metadata.

.. code-block:: bash

  $ pw update inspect-root-metadata [-h] pathname

.. csv-table::
  :header: "Argument", "Description"
  :widths: 30, 70
  :align: left

  ``pathname``, "Path to root metadata"

create-empty-bundle
-------------------

Creates an empty update bundle.

.. code-block:: bash

  $ pw update create-empty-bundle [-h] \
      [--target-metadata-version VERSION] \
      pathname

.. csv-table::
  :header: "Argument", "Description"
  :widths: 30, 70
  :align: left

  ``pathname``, "Path to newly created empty bundle"

.. csv-table::
  :header: "Option", "Description"
  :widths: 30, 70
  :align: left

  ``--target-metadata-version``, "Version number for targets metadata, defaults to 1"

add-root-metadata-to-bundle
---------------------------

Adds a root metadata to a bundle.

.. code-block:: bash

  $ pw update add-root-metadata-to-bundle [-h] \
      --append-root-metadata ROOT_METADATA \
      --bundle BUNDLE

.. csv-table::
  :header: "Option", "Description"
  :widths: 30, 70
  :align: left

  ``--append-root-metadata``, "Path to root metadata"
  ``--bundle``, "Pathname of the bundle"


add-file-to-bundle
------------------

Adds a target file to an existing bundle.

.. code-block:: bash

  $ pw update add-file-to-bundle [-h] \
      [--new-name NEW_NAME] \
      --bundle BUNDLE \
      --file FILE_PATH

.. csv-table::
  :header: "Option", "Description"
  :widths: 30, 70
  :align: left

  ``--file``, "Path to a target file"
  ``--bundle``, "Pathname of the bundle"
  ``--new-name``, "Optional new name for target"

sign-bundle
-----------

Signs an existing bundle with a dev key.

.. code-block:: bash

  $ pw update sign-bundle [-h] --bundle BUNDLE --key KEY

.. csv-table::
  :header: "Option", "Description"
  :widths: 30, 70
  :align: left

  ``--key``, "The targets signing key (private)"
  ``--bundle``, "Pathname of the bundle"

inspect-bundle
--------------

Prints the contents of a given bundle.

.. code-block:: bash

  $ pw update inspect-bundle [-h] pathname

.. csv-table::
  :header: "Argument", "Description"
  :widths: 30, 70
  :align: left

  ``pathname``, "Pathname of the bundle"

verify-bundle
-------------

Performs verification of an existing bundle.

.. code-block:: bash

  $ pw update verify-bundle [-h] \
      --bundle BUNDLE
      --trusted-root-metadata ROOT_METADATA

.. csv-table::
  :header: "Option", "Description"
  :widths: 30, 70

  ``--trusted-root-metadata``, "Trusted root metadata(anchor)"
  ``--bundle``, "Pathname of the bundle to be verified"
