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

----------------------
Building the extension
----------------------
Start by installing the dependencies: ``npm install``

Then the most effective way to work is to run ``npm run watch:bundle`` in a
terminal window. This will build and bundle the extension as you work, and you
can launch the ``Run Extension`` run configuration from Visual Studio Code to
launch a new instance with the current build of the extension.

-----------------------
Packaging the extension
-----------------------
It's one command: ``npm run package``
