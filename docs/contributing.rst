.. _docs-contributing:

============
Contributing
============
We'd love to accept your patches and contributions to Pigweed. There are just a
few small guidelines you need to follow. Before making or sending major
changes, please reach out on the `mailing list
<https://groups.google.com/forum/#!forum/pigweed>`_ first to ensure the changes
make sense for upstream. We generally go through a design phase before making
large changes.

Before participating in our community, please take a moment to review our
:ref:`docs-code-of-conduct`. We expect everyone who interacts with the project
to respect these guidelines.

Pigweed contribution overview
-----------------------------

#. One-time contributor setup:

  - Sign the `Contributor License Agreement <https://cla.developers.google.com/>`_.
  - Verify that your Git user email (git config user.email) is either Google
    Account email or an Alternate email for the Google account used to sign
    the CLA (Manage Google account → Personal Info → email)
  - Sign in to `Gerrit <https://pigweed-review.googlesource.com/>`_ to create
    an account using the same Google account you used above.
  - Obtain a login cookie from Gerrit's `new-password <https://pigweed-review.googlesource.com/new-password>`_ page
  - Install the Gerrit commit hook to automatically add a ``Change-Id: ...``
    line to your commit
  - Install the Pigweed presubmit check hook with ``pw presubmit --install``

#. Ensure all files include the correct copyright and license headers
#. Include any necessary changes to the documentation
#. Run :ref:`module-pw_presubmit` to detect style or compilation issues before
   uploading
#. Upload the change with ``git push origin HEAD:refs/for/main``
#. Address any reviewer feedback by amending the commit (``git commit --amend``)
#. Submit change to CI builders to merge. If you are not part of Pigweed's
   core team, you can ask the reviewer to add the `+2 CQ` vote, which will
   trigger a rebase and submit once the builders pass

.. note::

  If you have any trouble with this flow, reach out in our `chat room
  <https://discord.gg/M9NSeTA>`_ or on the `mailing list
  <https://groups.google.com/forum/#!forum/pigweed>`_ for help.

Contributor License Agreement
-----------------------------
Contributions to this project must be accompanied by a Contributor License
Agreement. You (or your employer) retain the copyright to your contribution;
this simply gives us permission to use and redistribute your contributions as
part of the project. Head over to <https://cla.developers.google.com/> to see
your current agreements on file or to sign a new one.

You generally only need to submit a CLA once, so if you've already submitted one
(even if it was for a different project), you probably don't need to do it
again.

Gerrit Commit Hook
------------------
Gerrit requires all changes to have a ``Change-Id`` tag at the bottom of each
commit message. You should set this up to be done automatically using the
instructions below.

**Linux/macOS**

.. code:: bash

  $ f=`git rev-parse --git-dir`/hooks/commit-msg ; mkdir -p $(dirname $f) ; curl -Lo $f https://gerrit-review.googlesource.com/tools/hooks/commit-msg ; chmod +x $f

**Windows**

Download `the Gerrit commit hook
<https://gerrit-review.googlesource.com/tools/hooks/commit-msg>`_ and then copy
it to the ``.git\hooks`` directory in the Pigweed repository.

.. code::

  copy %HOMEPATH%\Downloads\commit-msg %HOMEPATH%\pigweed\.git\hooks\commit-msg

Commit message
--------------
Consider the following when writing a commit message:

#. **Documentation and comments are better** - Consider whether the commit
   message contents would be better expressed in the documentation or code
   comments. Docs and code comments are durable and readable later; commit
   messages are rarely read after the change lands.
#. **Include why the change is made, not just what the change is** - It is
   important to include a "why" component in most commits. Sometimes, why is
   evident - for example, reducing memory usage, or optimizing. But it is often
   not. Err on the side of over-explaining why, not under-explaining why.

Pigweed commit messages should conform to the following style:

**Yes:**

.. code:: none

   pw_some_module: Short capitalized description

   Details about the change here. Include a summary of the what, and a clear
   description of why the change is needed for future maintainers.

   Consider what parts of the commit message are better suited for
   documentation.

**Yes**: Small number of modules affected; use {} syntax.

.. code:: none

   pw_{foo, bar, baz}: Change something in a few places

   When changes cross a few modules, include them with the syntax shown above.


**Yes**: targets are effectively modules, even though they're nested, so they get a
``/`` character.

.. code:: none

   targets/xyz123: Tweak support for XYZ's PQR

**Yes**: Uses imperative style for subject and text.

.. code:: none

   pw_something: Add foo and bar functions

   This commit correctly uses imperative present-tense style.

**No**: Uses non-imperative style for subject and text.

.. code:: none

   pw_something: Adds more things

   Use present tense imperative style for subjects and commit. The above
   subject has a plural "Adds" which is incorrect; should be "Add".

**Yes**: Use bulleted lists when multiple changes are in a single CL. Prefer
smaller CLs, but larger CLs are a practical reality.

.. code:: none

   pw_complicated_module: Pre-work for refactor

   Prepare for a bigger refactor by reworking some arguments before the larger
   change. This change must land in downstream projects before the refactor to
   enable a smooth transition to the new API.

   - Add arguments to MyImportantClass::MyFunction
   - Update MyImportantClass to handle precondition Y
   - Add stub functions to be used during the transition

**No**: Run on paragraph instead of bulleted list

.. code:: none

   pw_foo: Many things in a giant BWOT

   This CL does A, B, and C. The commit message is a Big Wall Of Text (BWOT),
   which we try to discourage in Pigweed. Also changes X and Y, because Z and
   Q. Furthermore, in some cases, adds a new Foo (with Bar, because we want
   to). Also refactors qux and quz.

**No**: Doesn't capitalize the subject

.. code:: none

   pw_foo: do a thing

   Above subject is incorrect, since it is a sentence style subject.

**Yes**: Doesn't capitalize the subject when subject's first word is a
lowercase identifier.

.. code:: none

   pw_foo: std::unique_lock cleanup

   This commit message demonstrates the subject when the subject has an
   identifier for the first word. In that case, follow the identifier casing
   instead of capitalizing.

   However, imperative style subjects often have the identifier elsewhere in
   the subject; for example:

     pw_foo: Improve use of std::unique_lock

**No**: Uses a non-standard ``[]`` to indicate moduule:

.. code:: none

   [pw_foo]: Do a thing

**No**: Has a period at the end of the subject

.. code:: none

   pw_bar: Do somehthing great.

**No**: Puts extra stuff after the module which isn't a module.

.. code:: none

   pw_bar/byte_builder: Add more stuff to builder

Footer
^^^^^^
We support a number of `git footers`_ in the commit message, such as ``Bug:
123`` in the message below:

.. code:: none

   pw_something: Add foo and bar functions

   Bug: 123

You are encouraged to use the following footers when appropriate:

* ``Bug``: Associates this commit with a bug (issue in our `bug tracker`_). The
  bug will be automatically updated when the change is submitted. When a change
  is relevant to more than one bug, include multiple ``Bug`` lines, like so:

  .. code:: none

      pw_something: Add foo and bar functions

      Bug: 123
      Bug: 456

* ``Fixed``: Like ``Bug``, but automatically closes the bug when submitted.

In addition, we support all of the `Chromium CQ footers`_, but those are
relatively rarely useful.

.. _bug tracker: https://bugs.chromium.org/p/pigweed/issues/list
.. _Chromium CQ footers: https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/infra/cq.md#options
.. _git footers: https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/git-footers.html


Documentation
-------------
All Pigweed changes must either

#. Include updates to documentation, or
#. Include ``No-Docs-Update-Reason: <reason>`` in a Gerrit comment on the CL.
   For example:

   * ``No-Docs-Update-Reason: formatting tweaks``
   * ``No-Docs-Update-Reason: internal cleanups``
   * ``No-Docs-Update-Reason: bugfix``

It's acceptable to only document new changes in an otherwise underdocumented
module, but it's not acceptable to not document new changes because the module
doesn't have any other documentation.

Code Reviews
------------
All Pigweed development happens on Gerrit, following the `typical Gerrit
development workflow <http://ceres-solver.org/contributing.html>`_. Consult the
`Gerrit User Guide
<https://gerrit-documentation.storage.googleapis.com/Documentation/2.12.3/intro-user.html>`_
for more information on using Gerrit.

In the future we may support GitHub pull requests, but until that time we will
close GitHub pull requests and ask that the changes be uploaded to Gerrit
instead.

Community Guidelines
--------------------
This project follows `Google's Open Source Community Guidelines
<https://opensource.google/conduct/>`_ and the :ref:`docs-code-of-conduct`.

Source Code Headers
-------------------
Every Pigweed file containing source code must include copyright and license
information. This includes any JS/CSS files that you might be serving out to
browsers.

Apache header for C and C++ files:

.. code:: none

  // Copyright 2021 The Pigweed Authors
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

Apache header for Python and GN files:

.. code:: none

  # Copyright 2020 The Pigweed Authors
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

Presubmit Checks and Continuous Integration
-------------------------------------------
All Pigweed change lists (CLs) must adhere to Pigweed's style guide and pass a
suite of automated builds, tests, and style checks to be merged upstream. Much
of this checking is done using Pigweed's ``pw_presubmit`` module by automated
builders. These builders run before each Pigweed CL is submitted and in our
continuous integration infrastructure (see `Pigweed's build console
<https://ci.chromium.org/p/pigweed/g/pigweed/console>`_).

Running Presubmit Checks
------------------------
To run automated presubmit checks on a pending CL, click the ``CQ DRY RUN``
button in the Gerrit UI. The results appear in the Tryjobs section, below the
source listing. Jobs that passed are green; jobs that failed are red.

If all checks pass, you will see a ``Dry run: This CL passed the CQ dry run.``
comment on your change. If any checks fail, you will see a ``Dry run: Failed
builds:`` message. All failures must be addressed before submitting.

In addition to the publicly visible presubmit checks, Pigweed runs internal
presubmit checks that are only visible within Google. If any these checks fail,
external developers will see a ``Dry run: Failed builds:`` comment on the CL,
even if all visible checks passed. Reach out to the Pigweed team for help
addressing these issues.

Project Presubmit Checks
------------------------
In addition to Pigweed's presubmit checks, some projects that use Pigweed run
their presubmit checks in Pigweed's infrastructure. This supports a development
flow where projects automatically update their Pigweed submodule if their tests
pass. If a project cannot build against Pigweed's tip-of-tree, it will stay on
a fixed Pigweed revision until the issues are fixed. See the `sample project
<https://pigweed.googlesource.com/pigweed/sample_project/>`_ for an example of
this.

Pigweed does its best to keep builds passing for dependent projects. In some
circumstances, the Pigweed maintainers may choose to merge changes that break
dependent projects. This will only be done if

* a feature or fix is needed urgently in Pigweed or for a different project,
  and
* the project broken by the change does not imminently need Pigweed updates.

The downstream project will continue to build against their last working
revision of Pigweed until the incompatibilities are fixed.

In these situations, Pigweed's commit queue submission process will fail for all
changes. If a change passes all presubmit checks except for known failures, the
Pigweed team may permit manual submission of the CL. Contact the Pigweed team
for submission approval.

Running local presubmits
------------------------
To speed up the review process, consider adding :ref:`module-pw_presubmit` as a
git push hook using the following command:

Linux/macOS
^^^^^^^^^^^
.. code:: bash

  $ pw presubmit --install

This will be effectively the same as running the following command before every
``git push``:

.. code:: bash

  $ pw presubmit


.. image:: ../pw_presubmit/docs/pw_presubmit_demo.gif
  :width: 800
  :alt: pw presubmit demo

If you ever need to bypass the presubmit hook (due to it being broken, for
example) you may push using this command:

.. code:: bash

  $ git push origin HEAD:refs/for/main --no-verify

Presubmit and branch management
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
When creating new feature branches, make sure to specify the upstream branch to
track, e.g.

.. code:: bash

  $ git checkout -b myfeature origin/main

When tracking an upstream branch, ``pw presubmit`` will only run checks on the
modified files, rather than the entire repository.

.. _Sphinx: https://www.sphinx-doc.org/

.. inclusive-language: disable

.. _reStructuredText Primer: https://www.sphinx-doc.org/en/master/usage/restructuredtext/basics.html

.. inclusive-language: enable

