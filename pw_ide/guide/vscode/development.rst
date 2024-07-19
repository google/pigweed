.. _module-pw_ide-guide-vscode-development:

===========
Development
===========
.. pigweed-module-subpage::
   :name: pw_ide

The Visual Studio Code extension is developed in the main Pigweed repository.
If you want to contribute to the extension, the first step is to become a
:ref:`Pigweed contributor<docs-contributing>`.

---------------------
Working with the code
---------------------
When working on the Visual Studio Code extension, it's more effective to open
the extension's root directory instead of opening the repository root. That
directory is: ``<repo root>/pw_ide/ts/pigweed-vscode``

If you're using Visual Studio Code to write code for the Visual Studio Code
extension, install any extensions that are recommended to you when you open
the directory.

----------------------
Building the extension
----------------------
Start by installing the dependencies: ``npm install``

Then the most effective way to work is to run the ``Run Extension`` launch
configuration. This will start a build watcher that will continuously build and
bundle the extension, and will launch a new Visual Studio Code instance with the
current build of the extension.

.. note::

   You need to have the `TypeScript + Webpack Problem Matchers <https://marketplace.visualstudio.com/items?itemName=amodio.tsl-problem-matcher>`_
   extension installed for the above to work.

-----------------------
Packaging the extension
-----------------------
It's one command: ``npm run package``
