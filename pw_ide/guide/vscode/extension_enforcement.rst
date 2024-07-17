.. _module-pw_ide-guide-vscode-extension-enforcement:

=====================
Extension enforcement
=====================
.. pigweed-module-subpage::
   :name: pw_ide

One of Pigweed's most powerful benefits is that it provides all developers on
a project team with a consistent development environment. Some teams want to
extend this consistency to the IDE by declaring that a certain set of extensions
should be enabled (or disabled) when working on their project.

Visual Studio Code provides two mechanisms for indicating that users *should*
have certain extensions:

#. Extensions can have other extensions as dependencies, such that installing
   the extension also installs its dependencies.

#. Projects can included *recommendations* in ``.vscode/extensions.json``.

The problem with the first option is that it is only available to extensions.
Projects can't declare extension dependencies.

The problem with the second option is that they are just *recommendations*, and
there is no built-in mechanism to enforce them or even install them by default.

Because there's not a natural way to achieve the goal of editor consistency
natively in Visual Studio Code, the Pigweed extension provides an additional
mechanism that teams can use instead.

-----
Usage
-----
Just enable :ref:`the setting<module-pw_ide-guide-vscode-settings-enforce-extension-recommendations>`.

Now, extensions listed under ``recommendations`` in the project's
``.vscode/extensions.json`` will need to be installed and enabled for the
project, and the user will be prompted to do so until all of the extensions are
installed and enabled, or until the user manually disables the notification.

.. tip::

   Although installed extensions are available globally, they don't need to be
   enabled in other projects or instances of Visual Studio Code.

Likewise, extensions listed under ``unwantedRecommendations`` will need to be
uninstalled or disabled, and the user will be prompted to do so until all of the
extensions are installed and enabled, or until the user manually disables the
notification.

.. tip::

   Unwanted recommendations don't need to be uninstalled if you use them in
   other projects. Just disable them for this project.
