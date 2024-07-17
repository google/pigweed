.. _module-pw_ide:

------
pw_ide
------
.. pigweed-module::
   :name: pw_ide

.. toctree::
   :maxdepth: 1
   :hidden:

   guide/index
   design/index

Rich, modern IDE and code editor support for embedded systems Projects.

Most modern software development takes advantage of language servers and
advanced editor features to power experiences like:

* Fast, accurate code navigation, including finding definitions,
  implementations, and references

* Code autocompletion based on a deep understanding of the code structure, not
  just dictionary lookups

* Instant compiler errors and warnings as you write your code

Most embedded systems development still lacks these features.
**When you use Pigweed, you get all of them!**

.. grid:: 2

   .. grid-item-card:: :octicon:`codescan` Visual Studio Code
      :link: module-pw_ide-guide-vscode
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Learn how to use Visual Studio Code for blazingly modern embedded software
      development for Bazel-based Pigweed projects

   .. grid-item-card:: :octicon:`terminal` pw_ide CLI
      :link: module-pw_ide-guide-cli
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Learn how to use the ``pw_ide`` command-line interface with
      bootstrap-based Pigweed projects using GN or CMake

.. grid:: 2

   .. grid-item-card:: :octicon:`code` C/C++ Code Intelligence
      :link: module-pw_ide-design-cpp
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Learn about Pigweed's approach to IDE code intelligence for C/C++

   .. grid-item-card:: :octicon:`project` Projects
      :link: module-pw_ide-design-projects
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Understand Pigweed project structure and how Pigweed's IDE features
      interact with it
