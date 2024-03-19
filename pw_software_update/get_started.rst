.. _module-pw_software_update-get-started:

-------------------------------
pw_software_update: Get started
-------------------------------
.. pigweed-module-subpage::
   :name: pw_software_update

Hello Bundles!
--------------

A bundle represents a single software release instance -- including
all target files of the software build and metadata needed to match,
verify and install the build on a target device.

The following illustrates how a bundle is created, signed, inspected,
and verified using ``pw update ...`` commands.

1. First let's make a working directory under your Pigweed dir. The
   ``pw update`` command is not yet visible outside the Pigweed
   directory.

.. code-block:: bash

   $ cd ~/pigweed
   $ source activate.sh

   $ mkdir hello_bundles
   $ cd hello_bundles


2. Create signing keys for "root" and "targets" roles.

.. note::
   Here keys are generated locally for demonstration purposes only. In
   production, you must use a proper key management service (such as
   `Google Cloud KMS <https://cloud.google.com/security-key-management>`_)
   to generate, control access to, and log usage of software signing keys.

.. code-block:: bash

   $ mkdir keys
   $ pw update generate-key keys/root_key
   $ pw update generate-key keys/targets_key
   $ tree
   .
   └── keys
       ├── root_key
       ├── root_key.pub
       ├── targets_key
       └── targets_key.pub

3. Now that we have the keys, let's find them an owner by creating the root
   metadata.

.. code-block:: bash

   # Assign a single key to each "root" and "targets" roles.
   $ pw update create-root-metadata --append-root-key keys/root_key.pub \
      --append-targets-key keys/targets_key.pub -o root_metadata.pb

   # Sign the root metadata with the root key to make it official.
   $ pw update sign-root-metadata --root-metadata root_metadata.pb \
      --root-key keys/root_key

   # Review the generated root metadata (output omitted for brevity).
   $ pw update inspect-root-metadata root_metadata.pb

4. Now we are ready to create a bundle.

.. code-block:: bash

   # Start with an empty bundle.
   $ pw update create-empty-bundle my_bundle.pb

   # Add root metadata.
   $ pw update add-root-metadata-to-bundle \
      --append-root-metadata root_metadata.pb --bundle my_bundle.pb

   # Add some files.
   $ mkdir target_files
   $ echo "application bytes" > target_files/application.bin
   $ echo "rtos bytes" > target_files/rtos.bin
   $ pw update add-file-to-bundle --bundle my_bundle.pb --file target_files/application.bin
   $ pw update add-file-to-bundle --bundle my_bundle.pb --file target_files/rtos.bin
   $ tree
      .
      ├── keys
      │   ├── root_key
      │   ├── root_key.pub
      │   ├── targets_key
      │   └── targets_key.pub
      ├── my_bundle.pb
      ├── root_metadata.pb
      └── target_files
          ├── application.bin
          └── rtos.bin

   # Sign our bundle with the "targets" key.
   $ pw update sign-bundle --bundle my_bundle.pb --key keys/targets_key

   # Review and admire our work (output omitted).
   $> pw update inspect-bundle my_bundle.pb

5. Finally we can verify the integrity of our bundle.

.. note::
   Here we are using ``python3 -m pw_software_update.verify`` because the
   ``pw verify-bundle`` command is WIP.

.. code-block:: bash

   $ python3 -m pw_software_update.verify --incoming my_bundle.pb
      Verifying: my_bundle.pb
      (self-verification)
      Checking content of the trusted root metadata
             Checking role type
             Checking keys database
             Checking root signature requirement
             Checking targets signature requirement
             Checking for key sharing
      Verifying incoming root metadata
             Checking signatures against current root
             Total=1, threshold=1
             Verified: 1
             Checking content
             Checking role type
             Checking keys database
             Checking root signature requirement
             Checking targets signature requirement
             Checking for key sharing
             Checking signatures against current root
             Total=1, threshold=1
             Verified: 1
             Checking for version rollback
             Targets key rotation: False
      Upgrading trust to the incoming root metadata
      Verifying targets metadata
             Checking signatures: total=1, threshold=1
             Verified signatures: 1
             Checking content
             Checking role type
      Checking targets metadata for version rollback
      Verifying target file: "application"
      Verifying target file: "rtos"
      Verification passed.

🎉🎉
Congratulations on creating your first ``pw_software_update`` bundle!
🎉🎉

To learn more, see :ref:`module-pw_software_update-design`.
