.. _showcase-sense-tutorial-setup:

========
1. Setup
========
First things first: install prerequisite software and set up the
Sense project.

.. admonition:: Getting help

   If you get stuck or confused at any point during the Sense tutorial, you're
   welcome (and encouraged!) to talk to the Pigweed team in our
   `Discord <https://discord.gg/M9NSeTA>`_ or
   `issue tracker <https://pwbug.dev>`_.

.. _showcase-sense-tutorial-setup-prereqs:

-----------------------------
Install prerequisite software
-----------------------------
Prepare your computer for working with Pigweed-based projects:

#. Complete the instructions in :ref:`docs-first-time-setup-guide-express-setup`.
   **You only need to complete the "express setup" instructions**; come back to
   this page once you're done with the express setup.

.. _showcase-sense-tutorial-setup-sense:

------------------------
Set up the Sense project
------------------------
We recommend trying out this tutorial with Visual Studio Code (VS Code). This
tutorial will also provide CLI-equivalent workflows if you can't or don't want
to use VS Code.

.. _Visual Studio Code: https://code.visualstudio.com/Download
.. _Pigweed extension: https://marketplace.visualstudio.com/items?itemName=pigweed.pigweed-vscode
.. _Extensions view: https://code.visualstudio.com/docs/editor/extension-marketplace#_browse-for-extensions
.. _user settings file: https://code.visualstudio.com/docs/getstarted/settings#_settings-file-locations

.. tab-set::

   .. tab-item:: VS Code
      :sync: vsc

      #. Install `Visual Studio Code`_ (VS Code).

      #. Clone the project:

         .. code-block:: console

            git clone https://pigweed.googlesource.com/pigweed/showcase/sense

      #. Open the Sense project in VS Code. One way to do this is to
         launch VS Code, click the **Open folder** link on the landing view,
         and then select your Sense directory A.K.A. folder.

         .. warning::

            Don't use the **Add Folder to Workspace** workflow or any other
            workspace-oriented workflow. This project doesn't play nicely
            with workspaces yet.

      #. For the **Do you trust the authors of the files in this folder?** popup
         click **Yes, I trust the authors**.

         If you want to try out the project's building and flashing workflows, you must
         click **Yes, I trust the authors** or else the project doesn't have sufficient
         permissions to perform these actions. You can still browse the tutorial
         if you click **No, I don't trust the authors** but you won't be able to do
         much more than that.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/trust.png

         .. admonition:: Important

            The next few instructions show you how to deal with popups that
            you **may or may not** see. If you use VS Code a lot, you may
            already have the recommended tools and extensions installed, so
            you won't see the popups. That's OK; you can just skip the
            instructions for popups you didn't see.

      #. For the **Do you want to install the recommended 'Pigweed' extension
         from pigweed for this repository?** popup click **Install**.

         The Pigweed extension is basically the project's heart. Lots of
         things depend on this extension being installed.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/install_pigweed_extension.png

      #. For the popup that starts with **Pigweed recommends using Bazelisk to manage your
         Bazel environment** click **Default**.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/recommended_bazelisk_settings.png

      #. If you see the popup that starts with **Buildifier was not found**,
         open the ``//.vscode/settings.json`` file **within the Sense repo**
         (*not* your `user settings file`_) and verify that a
         ``bazel.buildifierExecutable`` setting has been populated. If you
         see that setting, then Buildifier is set up and you can ignore the
         popup warning. If you don't see that setting, then you can follow
         the instructions in :ref:`showcase-sense-tutorial-appendix-buildifier`
         to set up Buildifier. You can also skip setting up Buildifier; you'll
         still be able to complete the tutorial. Some Bazel files just might
         not get formatted correctly.

         .. note::

            ``//`` means the root directory of your Sense repository.
            If you cloned Sense to ``~/sense``, then ``//.vscode`` would
            be located at ``~/sense/.vscode``.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/buildifier_not_found.png

         .. note::

            This warning happens because VS Code doesn't provide fine-tuned
            control over the extension loading order. Basically, the Bazel
            extension loads and it doesn't detect Buildifier, so it displays
            that popup warning. But then the Pigweed extension does set up
            Buildifier soon after. The problem is that there's no way to
            specify that the Pigweed extension should load before the Bazel
            extension.

      #. Make sure you're running the latest version of the Pigweed extension
         by opening the `Extensions view`_, going to the page for the Pigweed
         extension, and checking that your version is ``v1.3.0`` or later.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/20240802/pigweed_extension.png

            Double-checking the Pigweed extension version

         .. caution::

            If you see a legacy version of the Pigweed extension, uninstall it.

   .. tab-item:: CLI
      :sync: cli

      #. :ref:`Install Bazelisk <docs-install-bazel>`. **Come back to this page
         once you can successfully run** ``bazelisk --version`` **from your
         command line.** It should print out the version of Bazel that you're
         using.

         .. note::

            See :ref:`docs-install-bazel-bazelisk` for an explanation of the
            difference between Bazel and ``bazelisk``.

      #. Clone the project:

         .. code-block:: console

            git clone https://pigweed.googlesource.com/pigweed/showcase/sense

      #. Set your working directory to the project root:

         .. code-block:: console

            cd sense

-------
Summary
-------
.. _Bazelisk: https://bazel.build/install/bazelisk

Later on, if you decide to build a product on top of Pigweed, you can
expect new teammates to onboard onto your codebase using workflows
similar to how you just set up Sense.

One interesting thing to note about Bazel-based projects like Sense:
no need for ``--recursive`` when cloning the repo! I.e. no need for
Git submodules. Check out ``MODULE.bazel`` in the root directory of
the repo to discover more about how dependencies are managed.

Next, head over to :ref:`showcase-sense-tutorial-explore` to build
up your top-down intution about how the Sense project is structured.

--------
Appendix
--------

.. _showcase-sense-tutorial-appendix-buildifier:

Buildifier setup
================
The Pigweed extension for VS Code should set up Bazel's Buildifier
for you. If for some reason it doesn't work, here's how to set it
up manually:

#. First check ``//.vscode/settings.json``. If you see a
   ``bazel.buildifierExecutable`` entry then the Pigweed extension
   actually already set up Buildifier correctly and no further
   work is needed on your part.

#. Download the latest `Buildifier
   release <https://github.com/bazelbuild/buildtools/releases>`_.

#. Make sure that the Buildifier binary you download is executable:

   .. code-block:: console

      chmod +x buildifier-*

#. Add a ``bazel.buildifierExecutable`` entry in
   ``//.vscode/settings.json``:

   .. code-block:: json

      {
          "...": "...",
          "bazel.buildifierExecutable": "/path/to/buildifier-*-*"
      }
