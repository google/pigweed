.. _module-pw_docgen:

=========
pw_docgen
=========
The docgen module provides tools to generate documentation for Pigweed-based
projects, and for Pigweed itself.

Pigweed-based projects typically use a subset of Pigweed's modules and add their
own product-specific modules on top of that, which may have product-specific
documentation. Docgen provides a convenient way to combine all of the relevant
documentation for a project into one place, allowing downstream consumers of
release bundles (e.g. factory teams, QA teams, beta testers, etc.) to have a
unified source of documentation early on.

The documentation generation is integrated directly into the build system. Any
build target can depend on documentation, which allows it to be included as part
of a factory release build, for example. Additionally, documentation itself can
depend on other build targets, such as report cards for binary size/profiling.
Any time the code is changed, documentation will be regenerated with the updated
reports.

----------------------
Documentation overview
----------------------
Each Pigweed module provides documentation describing its functionality, use
cases, and programming API.

Included in a module's documentation are report cards which show an overview of
the module's size cost and performance benchmarks. These allow prospective users
to evaluate the impact of including the module in their projects.

-----------------
Build integration
-----------------
Pigweed documentation files are written in `reStructuredText`_ format and
rendered to HTML using `Sphinx`_ through Pigweed's GN build system.

.. _reStructuredText: http://docutils.sourceforge.net/rst.html
.. inclusive-language: ignore
.. _Sphinx: http://www.sphinx-doc.org/en/master

There are additonal Sphinx plugins used for rendering diagrams within
reStructuredText files including:

* `mermaid <https://mermaid-js.github.io/>`_ via the `sphinxcontrib-mermaid
  <https://pypi.org/project/sphinxcontrib-mermaid/>`_ package.

Documentation source and asset files are placed alongside code within a module
and registered as a ``pw_doc_group`` target within a ``BUILD.gn`` file. These
groups become available for import within a special documentation generation
target, which accumulates all of them and renders the resulting HTML. This
system can either be used directly within Pigweed, or integrated into a
downstream project.

GN Templates
============

pw_doc_group
------------
The main template for defining documentation files is ``pw_doc_group``. It is
used to logically group a collection of documentation source files and assets.
Each Pigweed module is expected to provide at least one ``pw_doc_group`` target
defining the module's documentation. A ``pw_doc_group`` can depend on other
groups, causing them to be built with it.

**Arguments**

* ``sources``: RST documentation source files.
* ``inputs``: Additional resources required for the docs (images, data files,
  etc.)
* ``group_deps``: Other ``pw_doc_group`` targets required by this one.
* ``report_deps``: Report card generating targets (e.g. ``pw_size_diff``) on
  which the docs depend.
* ``other_deps``: Any other GN targets that should be run before this
  ``pw_doc_group`` runs that is not included in one of the above ``dep``
  categories.

**Example**

.. code-block::

   pw_doc_group("my_doc_group") {
     sources = [ "docs.rst" ]
     inputs = [ "face-with-tears-of-joy-emoji.svg" ]
     group_deps = [ ":sub_doc_group" ]
     report_deps = [ ":my_size_report" ]
   }

pw_doc_gen
----------
The ``pw_doc_gen`` template creates a target which renders complete HTML
documentation for a project. It depends on registered ``pw_doc_group`` targets
and creates an action which collects and renders them.

To generate the complete docs, the template also requires a ``conf.py`` file
configuring Sphinx's output, and a top level ``index.rst`` for the main page of
the documentation. These are added at the root level of the built documentation
to tie everything together.

**Arguments**

* ``conf``: Path to the ``conf.py`` to use for Sphinx.
* ``index``: Path to the top-level ``index.rst`` file.
* ``output_directory``: Directory in which to render HTML output.
* ``deps``: List of all ``pw_doc_group`` targets required for the documentation.
* ``python_metadata_deps``: Python-related dependencies that are only used as
  deps for generating Python package metadata list, not the overall
  documentation generation. This should rarely be used by non-Pigweed code.

**Example**

.. code-block::

   pw_doc_gen("my_docs") {
     conf = "//my_docs/conf.py"
     index = "//my_docs/index.rst"
     output_directory = target_gen_dir
     deps = [
       "//my_module:my_doc_group",
     ]
   }

Generating documentation
------------------------
All source files listed under a ``pw_doc_gen`` target and its ``pw_doc_group``
dependencies get copied out into a directory structure mirroring the original
layout of the modules in which the sources appear. This is demonstrated below
using a subset of Pigweed's core documentation.

Consider the following target in ``$dir_pigweed/docs/BUILD.gn``:

.. code-block::

   pw_doc_gen("docs") {
     conf = "conf.py"
     index = "index.rst"
     output_directory = target_gen_dir
     deps = [
       "$dir_pw_bloat:docs",
       "$dir_pw_docgen:docs",
       "$dir_pw_preprocessor:docs",
     ]
   }

A documentation tree is created under the output directory. Each of the sources
and inputs in the target's dependency graph is copied under this tree in the
same directory structure as they appear under the root GN build directory
(``$dir_pigweed`` in this case). The ``conf.py`` and ``index.rst`` provided
directly to the ``pw_doc_gen`` template are copied in at the root of the tree.

.. code-block::

   out/gen/docs/pw_docgen_tree/
   ├── conf.py
   ├── index.rst
   ├── pw_bloat
   │   ├── bloat.rst
   │   └── examples
   │       └── simple_bloat.rst
   ├── pw_docgen
   │   └── docgen.rst
   └── pw_preprocessor
       └── docs.rst

This is the documentation tree which gets passed to Sphinx to build HTML output.
Imports within documentation files must be relative to this structure. In
practice, relative imports from within modules' documentation groups are
identical to the project's directory structure. The only special case is the
top-level ``index.rst`` file's imports; they must start from the project's build
root.

Viewing Documentation
---------------------
``pw_docgen`` includes a web server that serves locally-generated documentation
at ``pw_docgen.docserver``. It supports hot-reloading, so the rendered docs in
your browser will refresh as you make changes to the source files.

In most cases, you will not need to run the docs server directly. Instead, it
will be run via :ref:`module-pw_watch`.

-----------------
Sphinx Extensions
-----------------
This module houses Pigweed-specific extensions for the Sphinx documentation
generator. Extensions are included and configured in ``docs/conf.py``.

module_metadata
===============
See :ref:`docs-contrib-docs-modules-metadata`.

Canonical URL configuration
---------------------------
``module_metadata`` fixes the canonical URLs for ``*/docs.html`` pages. By
default Sphinx assumes that a page's canonical URL is its full URL. E.g. the
default canonical URL for ``//pw_string/docs.rst`` is
``https://pigweed.dev/pw_string/docs.html``.  The ``pigweed.dev``
server treats ``https://pigweed.dev/pw_string/`` as the canonical URL however.
This problem is not limited to module homepages; it occurs on any page that
ends in ``/docs.html`` such as
``https://pigweed.dev/third_party/emboss/docs.html``. ``module_metadata`` fixes
this problem by ensuring that the ``<link rel="canonical" href="..."/>`` tag
generated in the HTML is aligned with the server's configuration.

After building the docs, the canonical URLs for all HTML pages can be verified
by running the following command in a terminal from the root directory of the
upstream Pigweed repo:

.. code-block:: console

   grep '<link rel="canonical' out/docs/gen/docs/html/* -R

Context: `b/323077749 <https://issues.pigweed.dev/323077749>`_

google_analytics
----------------
When this extension is included and a ``google_analytics_id`` is set in the
Sphinx configuration, a Google Analytics tracking tag will be added to each
page of the documentation when it is rendered to HTML.

By default, the Sphinx configuration's ``google_analytics_id`` is set
automatically based on the value of the GN argument
``pw_docs_google_analytics_id``, allowing you to control whether tracking is
enabled or not in your build configuration. Typically, you would only enable
this for documentation builds intended for deployment on the web.

Debugging Pigweed's Sphinx extensions
-------------------------------------
To step through your Pigweed extension code with
`pdb <https://docs.python.org/3/library/pdb.html>`_:

#. Set a breakpoint in your extension code:

   .. code-block::

      breakpoint()

#. Build ``python.install`` to install the code change into the bootstrap venv
   (``environment/pigweed-venv/lib/python3.8/site-packages/pw_docgen``):

   .. code-block::

      ninja -C out python.install

#. Manually invoke Sphinx to build the docs and trigger your breakpoint:

   .. code-block::

      cd out
      sphinx-build -W -b html -d docs/gen/docs/help docs/gen/docs/pw_docgen_tree docs/gen/docs/html -v -v -v

   You should see build output from Sphinx. The build should pause at your
   breakpoint and you should then see pdb's prompt (``(Pdb)``).
