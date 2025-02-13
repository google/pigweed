.. _docs-style-python:

==================
Python style guide
==================
.. _Python: https://www.python.org/

Pigweed uses the standard `Python`_ style: `PEP8
<https://www.python.org/dev/peps/pep-0008/>`__. All Pigweed Python code should
pass ``pw format``, which invokes ``black`` with a couple options.

---------------
Python versions
---------------
Pigweed officially supports :ref:`a few Python versions
<docs-concepts-python-version>`. Upstream Pigweed code must support those Python
versions. The only exception is :ref:`module-pw_env_setup`, which must also
support Python 2 and 3.6.

.. _docs-style-python-extend-generated-import-paths:

-------------------------------------------------------
Extend the import path of packages with generated files
-------------------------------------------------------
Python packages that include generated files should extend their import path
using the following snippet at the beginning of their ``__init__.py``:

.. code-block:: python

   # This Python package contains generated Python modules that overlap with
   # this `__init__.py` file's import namespace, so this package's import path
   # must be extended for the generated modules to be discoverable.
   #
   # Note: This needs to be done in every nested `__init__.py` that will contain
   # overlapping generated files.
   __path__ = __import__('pkgutil').extend_path(__path__, __name__)
