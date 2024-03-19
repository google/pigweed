.. _docs-pw-style:

===========
Style Guide
===========
.. grid:: 1

   .. grid-item-card:: :octicon:`diff-added` C++ style
      :link: docs-pw-style-cpp
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Our C++ style guide: an extension of the Google C++ style with further
      restrictions and guidance for embedded

.. grid:: 2

   .. grid-item-card:: :octicon:`rocket` Commit messages
      :link: docs-pw-style-commit-message
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      How to format commit messages for Pigweed

   .. grid-item-card:: :octicon:`code-square` Sphinx
      :link: docs-pw-style-sphinx
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Our website and module documentation is built with Sphinx

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` Doxygen
      :link: docs-pw-style-doxygen
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      How to structure reference documentation for C++ code

   .. grid-item-card:: :octicon:`code-square` Protobuf
      :link: docs-pw-style-protobuf
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      How to structure reference documentation for C++ code

.. tip::
   Pigweed runs ``pw format`` as part of ``pw presubmit`` to perform some code
   formatting checks. To speed up the review process, consider adding ``pw
   presubmit`` as a git push hook using the following command:
   ``pw presubmit --install``

-----------------
Copyright headers
-----------------
Every Pigweed file containing source code must include copyright and license
information. This includes any JS/CSS files that you might be serving out to
browsers.

Apache header source code files that support ``//`` comments:

.. code-block:: none

   // Copyright 2023 The Pigweed Authors
   //
   // Licensed under the Apache License, Version 2.0 (the "License"); you may not
   // use this file except in compliance with the License. You may obtain a copy of
   // the License at
   //
   //     https://www.apache.org/licenses/LICENSE-2.0
   //
   // Unless required by applicable law or agreed to in writing, software
   // distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
   // WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
   // License for the specific language governing permissions and limitations under
   // the License.

Apache header for source code files that support ``#`` comments:

.. code-block:: none

   # Copyright 2023 The Pigweed Authors
   #
   # Licensed under the Apache License, Version 2.0 (the "License"); you may not
   # use this file except in compliance with the License. You may obtain a copy of
   # the License at
   #
   #     https://www.apache.org/licenses/LICENSE-2.0
   #
   # Unless required by applicable law or agreed to in writing, software
   # distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
   # WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
   # License for the specific language governing permissions and limitations under
   # the License.


.. _python-style:

------------
Python style
------------
Pigweed uses the standard Python style: PEP8, which is available on the web at
https://www.python.org/dev/peps/pep-0008/. All Pigweed Python code should pass
``pw format``, which invokes ``black`` with a couple options.

Python versions
===============
Pigweed officially supports :ref:`a few Python versions
<docs-concepts-python-version>`. Upstream Pigweed code must support those Python
versions. The only exception is :ref:`module-pw_env_setup`, which must also
support Python 2 and 3.6.

---------------
Build files: GN
---------------
Each Pigweed source module requires a GN build file named BUILD.gn. This
encapsulates the build targets and specifies their sources and dependencies.
GN build files use a format similar to `Bazel's BUILD files
<https://docs.bazel.build/versions/main/build-ref.html>`_
(see the `Bazel style guide
<https://docs.bazel.build/versions/main/skylark/build-style.html>`_).

C/C++ build targets include a list of fields. The primary fields are:

* ``<public>`` -- public header files
* ``<sources>`` -- source files and private header files
* ``<public_configs>`` -- public build configuration
* ``<configs>`` -- private build configuration
* ``<public_deps>`` -- public dependencies
* ``<deps>`` -- private dependencies

Assets within each field must be listed in alphabetical order.

.. code-block:: cpp

   # Here is a brief example of a GN build file.

   import("$dir_pw_unit_test/test.gni")

   config("public_include_path") {
     include_dirs = [ "public" ]
     visibility = [":*"]
   }

   pw_source_set("pw_sample_module") {
     public = [ "public/pw_sample_module/sample_module.h" ]
     sources = [
       "public/pw_sample_module/internal/secret_header.h",
       "sample_module.cc",
       "used_by_sample_module.cc",
     ]
     public_configs = [ ":public_include_path" ]
     public_deps = [ dir_pw_status ]
     deps = [ dir_pw_varint ]
   }

   pw_test_group("tests") {
     tests = [ ":sample_module_test" ]
   }

   pw_test("sample_module_test") {
     sources = [ "sample_module_test.cc" ]
     deps = [ ":sample_module" ]
   }

   pw_doc_group("docs") {
     sources = [ "docs.rst" ]
   }

------------------
Build files: Bazel
------------------
Build files for the Bazel build system must be named ``BUILD.bazel``. Bazel can
interpret files named just ``BUILD``, but Pigweed uses ``BUILD.bazel`` to avoid
ambiguity with other build systems or tooling.

Pigweed's Bazel files follow the `Bazel style guide
<https://docs.bazel.build/versions/main/skylark/build-style.html>`_.

------------------
Build files: Soong
------------------
Build files (blueprints) for the Soong build system must be named
``Android.bp``. The way Pigweed modules and backends are used is documented in
:ref:`module-pw_build_android`.

.. _owners-style:

--------------------
Code Owners (OWNERS)
--------------------
If you use Gerrit for source code hosting and have the
`code-owners <https://android-review.googlesource.com/plugins/code-owners/Documentation/backend-find-owners.html>`__
plug-in enabled Pigweed can help you enforce consistent styling on those files
and also detects some errors.

The styling follows these rules.

#. Content is grouped by type of line (Access grant, include, etc).
#. Each grouping is sorted alphabetically.
#. Groups are placed the following order with a blank line separating each
   grouping.

   * "set noparent" line
   * "include" lines
   * "file:" lines
   * user grants (some examples: "*", "foo@example.com")
   * "per-file:" lines

This plugin will, by default, act upon any file named "OWNERS".

.. toctree::
   :hidden:

   C++ <style/cpp>
   Commit message <style/commit_message>
   Doxygen documentation <style/doxygen>
   Protobuf <style/protobuf>
   Sphinx documentation <style/sphinx>
