.. _showcase-sense-tutorial-setup:

========
1. Setup
========
First things first: install prerequisite software and set up the
Sense project.

.. _showcase-sense-tutorial-setup-help:

------------
Getting help
------------
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
   **You only need to complete the "express setup" instructions**.

.. _showcase-sense-tutorial-setup-sense:

------------------------
Set up the Sense project
------------------------
Usually, we recommend trying out the tutorial with Visual Studio Code (VS Code)
but we're currently refactoring our VS Code extension and fixing some bugs
(e.g. :bug:`419310666`). In the short-term, we recommend trying the CLI path of
the tutorial, not the VS Code path.

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

         .. admonition:: Important

            The next few instructions show you how to deal with popups that
            you **may or may not** see. If you use VS Code a lot, you may
            already have the recommended tools and extensions installed, so
            you won't see the popups. That's OK; you can just skip the
            instructions for popups you didn't see.

         .. admonition:: Troubleshooting

            If you see a popup that says ``spawn bazel ENOENT``, try ignoring it
            and proceeding with the rest of the tutorial. When the Bazel extension
            for VS Code starts up, it tries to run queries right away, even though
            the Bazel environment isn't completely ready yet. The Pigweed extension
            for VS Code ensures that the Bazel environment sets up properly.

            If the ``spawn bazel ENOENT`` popup seems like a legitimate error,
            make sure that you have opened the correct folder i.e. directory.
            If you're still seeing the issue after that, please
            :ref:`ask the Pigweed team for help <showcase-sense-tutorial-setup-help>`.

      #. If you see the **Do you trust the authors of the files in this folder?** popup
         click **Yes, I trust the authors**.

         If you want to try out the project's building and flashing workflows, you must
         click **Yes, I trust the authors** or else the project doesn't have sufficient
         permissions to perform these actions. You can still browse the tutorial
         if you click **No, I don't trust the authors** but you won't be able to do
         much more than that.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/trust.png

      #. If you see the **Do you want to install the recommended 'Pigweed' extension
         from pigweed for this repository?** popup click **Install**.

         The Pigweed extension is basically the project's heart. Lots of
         things depend on this extension being installed.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/install_pigweed_extension.png

      #. Open the **Extensions** pane (:kbd:`Control+Shift+X` on Linux and Windows,
         :kbd:`Command+Shift+X` on macOS), open the **Pigweed** extension, and make
         sure that you're running version 1.9.1 or later. If not, click the **More actions** (**▼**)
         button next to **Uninstall**, then select **Install Specific Version**, and then select
         the latest version from the dropdown menu.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/install_specific_version_v1.png

         You should see a Pigweed icon in the **Activity Bar**. If you don't, try closing and re-opening
         VS Code.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/extension_icon_v1.png

      #. If you see the popup that starts with **Pigweed recommends using Bazelisk to manage your
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

   .. tab-item:: CLI
      :sync: cli

      #. :ref:`Install Bazelisk <docs-install-bazel>`.

         .. note::

            See :ref:`docs-install-bazel-bazelisk` for an explanation of the
            difference between Bazel and Bazelisk.

      #. Run the following command to verify your Bazelisk installation:

         .. code-block:: console

            bazelisk version

         You should see output similar to this:

         .. code-block:: text

            Bazelisk version: v1.25.0
            Starting local Bazel server and connecting to it...
            Build target: @@//src/main/java/com/google/devtools/build/lib/bazel:BazelServer
            Build time: Thu Jan 01 00:00:00 1970 (0)
            Build timestamp: Thu Jan 01 00:00:00 1970 (0)
            Build timestamp as int: 0

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
.. _MODULE.bazel: https://cs.opensource.google/pigweed/showcase/sense/+/main:MODULE.bazel
.. _Bazel modules: https://bazel.build/external/module

Later on, if you decide to build a product on top of Pigweed, you can
expect new teammates to onboard onto your codebase using workflows
like this.

.. _--recursive: https://explainshell.com/explain?cmd=git+clone+--recursive

When cloning Sense, did you notice that there was no need for the
`--recursive`_ flag even though Sense has a few third-party dependencies?
Most Bazel projects don't need Git submodules. Check out Sense's `MODULE.bazel`_
file and read about `Bazel modules`_ to learn more about how dependencies
are managed in Bazel projects.

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
   ``bazel.buildifierExecutable`` entry, try copying the path to the
   executable and running ``<buildifier-path> --version`` to confirm
   that the executable is valid. If this works, then the Pigweed extension
   has already set up Buildifier correctly and no further work is needed.

   .. figure:: https://storage.googleapis.com/pigweed-media/sense/buildifier_version_v1.png

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
