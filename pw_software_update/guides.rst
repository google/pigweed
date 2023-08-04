.. _module-pw_software_update-guides:

-------------------------
pw_software_update: Guide
-------------------------
.. pigweed-module-subpage::
   :name: pw_software_update
   :tagline: Secure software delivery

How you update software on an embedded system is specific to the project.
However, there are common patterns. This section provides suggestions for
each scenario, which you can then adjust to fit your specific needs.

High-level steps
----------------

Setting up an end-to-end software delivery system can seem overwhelming, but
generally involves the following high-level steps.

#. Get familiar with ``pw_software_update``.
#. Enable local updates for development.
#. Enable remote updates for internal testing.
#. Prepare for launching.
#. Ensure smooth landing.

1. Get familiar with ``pw_software_update``.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``pw_software_update`` is not yet a fully managed service like Google Play
Store. To use it effectively, you need to have at least a basic understanding
of how it works. The
:ref:`Getting started <module-pw_software_update-get-started>` and
:ref:`Design <module-pw_software_update-design>` sections can help you with
this.

2. Enable local updates for development.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This step allows developers to update a device that is connected to their
development computer. This achieves a low-latency feedback loop for developers,
so they can see the results of their changes quickly.

.. csv-table::
  :header: "Component", "Task", "Description"
  :widths: 20, 20, 60
  :align: left

  *Build System*, Produce update bundles, "Use ``pw_software_update``'s CLI and
  Python APIs to generate and check in dev keys, assemble build artifacts into
  a bundle, and locally sign the bundle."

  *Dev Tools*, Send update bundles, "Use ``pw_rpc`` to connect to the
  :cpp:type:`BundledUpdate` service to start and progress through an update
  session. Use ``pw_transfer`` to transfer the bundle's bytes."

  *Device software*, "Implement :cpp:type:`BundledUpdateBackend`", "Respond to
  framework callings. Supply root metadata."

3. Enable remote updates for internal testing.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This step builds upon the previous step and allows developers as well as
internal testers to receive software updates over an internal network from an
internal release repository. This makes it easy for them to stay up-to-date with
the latest software and fixes.

.. csv-table::
  :header: "Component", "Task", "Description"
  :widths: 20, 20, 60
  :align: left

  *Build System*, Upload unsigned bundles, "Assemble and generate dev-key signed
  bundles for local consumption as before. Upload the unsigned bundle to an
  internal build artifacts repository."

  *Dev Tools*, Support remote update, "In addition to local update as before,
  optionally support updating a device with a build pulled from the build
  server."

  *Signing service*, Prepare for test-key signing, "Set up *root* and *targets*
  test keys and their corresponding ACLs. Monitor incoming signing requests and
  and automatically sign incoming builds with the test key."

  *Release system*, Produce internal releases, "Trigger builds. Run tests.
  Request test-key remote signing. Publish internal releases."

4. Prepare for launching.
~~~~~~~~~~~~~~~~~~~~~~~~~

The goal of this step is not to add new features for users, but to improve
security at key points in the development process in preparation for launch.

.. csv-table::
  :header: "Component", "Task", "Description"
  :widths: 20, 20, 60
  :align: left

  *Build System*, Validate and endorse builds, "In addition to previous
  responsibilities, validate builds to make sure the builds are configured
  and built properly per their build type (e.g. no debug features in user
  builds), and then endorse the validated builds by signing the builds with
  the build server's private key and uploading both the builds and signatures."

  *Signing service*, Prepare for prod-key signing, "Set up *root* and *targets*
  production keys and their corresponding ACLs. Monitor incoming signing
  requests and only sign qualified, user builds with the production key. Verify
  builder identity and validate build content just before signing."

  *Release system*, Produce well-secured releases, "Run builds through
  daily, internal tests, and production release candidates. Only production-sign
  a build after all other qualifications have passed."

5. Ensure smooth rollout.
~~~~~~~~~~~~~~~~~~~~~~~~~

This step ensures updates are delivered to users reliably and with speed in
cases of recoverable security bugs, over the supported lifetime of a product.

.. csv-table::
  :header: "Component", "Task", "Description"
  :widths: 20, 20, 60
  :align: left

  *Release system*, Produce well-secured updates, "Carefully control new
  features. Keep all dependencies up to date. Always ready for emergency
  updates."

..
  TODO(b/273583461): Document these topics.
  * How to integrate with verified boot
  * How to do A/B updates
  * How to manage delta updates
  * How to revoke a bad release
  * How to do stepping-stone releases
