.. _module-pw_config_loader:

----------------
pw_config_loader
----------------
.. pigweed-module::
   :name: pw_config_loader


.. pw_config_loader-nav-start

.. grid:: 1

   .. grid-item-card:: :octicon:`code-square` API reference
      :link: module-pw_config_loader-api
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Reference details about the ``pw_config_loader`` Python API

.. pw_config_loader-nav-end


This directory contains the code to extract specific sections of user-specific
configuration files for different parts of Pigweed.

There are two different supported structures for extracting sections:

.. code-block:: yaml

   section_title:
     foo: bar

.. code-block:: yaml

   config_title: section_title
   foo: bar

In addition, section titles can be nested:

.. code-block:: yaml

   section_title:
     subtitle:
       subsubtitle:
         foo: bar

.. code-block:: yaml

   config_title: section_title.subtitle.subsubtitle
   foo: bar

Similar data structures are also supported in JSON and TOML.

.. toctree::
   :hidden:
   :maxdepth: 1

   api
