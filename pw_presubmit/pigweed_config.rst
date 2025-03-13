.. _module-pw_presubmit-pwigweed_config:

======================================
``pigweed.json`` configuration options
======================================
.. pigweed-module-subpage::
   :name: pw_presubmit

--------------------------------------------
``pw.pw_presubmit.format.black_config_file``
--------------------------------------------
+--------------+-------------------------+
| Type         | str (path)              |
+--------------+-------------------------+

Path to the toml file containing
`Black formatting configuration <https://black.readthedocs.io/en/stable/usage_and_configuration/the_basics.html#configuration-via-a-file>`_.

.. admonition:: Note

   This only applies when running ``pw format`` from GN. Bazel-based formatting
   walks up parent directories for the current file being formatted until it
   finds a ``.black.toml`` file.

-------------------------------------
``pw.pw_presubmit.format.black_path``
-------------------------------------
+--------------+-------------------------+
| Type         | str (path)              |
+--------------+-------------------------+

When formatting with Black, uses the formatting tooling to call the binary at
the specified path rather than calling ``black`` from the shell ``PATH``.

.. admonition:: Note

   This only applies when running ``pw format`` from GN. Bazel-based formatting
   uses the ``black`` binary bundled via runfiles.

----------------------------------
``pw.pw_presubmit.format.exclude``
----------------------------------
+--------------+-------------------------+
| Type         | List[str] (regex)       |
+--------------+-------------------------+

Regex patterns of files that should be excluded from formatting checks.


.. admonition:: Tip

   Regex patterns are
   `searched <https://docs.python.org/3/library/re.html#re.Pattern.search>`_
   relative to the ``pigweed.json`` file. This means ``"^README$"`` will match a
   ``README`` file next to the ``pigweed.json``, and ``"(^|/)README$"`` will
   match every ``README`` in the same directory or any directories beneath the
   ``pigweed.json`` file.

-------------------------------------------
``pw.pw_presubmit.format.python_formatter``
-------------------------------------------
+--------------+-------------------------+
| Type         | str (choice)            |
+--------------+-------------------------+
|Valid choices | ``"black"``, ``"yapf"`` |
+--------------+-------------------------+

Which tool to use for Python code formatting:
`Black <https://black.readthedocs.io/en/stable/>`_ or
`YAPF <https://github.com/google/yapf>`_.

.. admonition:: Note

   This only applies when running ``pw format`` from GN. Bazel-based formatting
   always uses Black.
