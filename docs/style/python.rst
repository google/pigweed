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
