.. _docs-contributing:

============
Contributing
============
We'd love to accept your patches and contributions to Pigweed. There are just a
few small guidelines you need to follow.

Before participating in our community, please take a moment to review our
:ref:`docs-code-of-conduct`. We expect everyone who interacts with the project
to respect these guidelines.

Get started
-----------
See :ref:`docs-get-started-upstream`.

Pigweed contribution overview
-----------------------------
.. note::

  If you have any trouble with this flow, reach out in our `chat room
  <https://discord.gg/M9NSeTA>`_ or on the `mailing list
  <https://groups.google.com/forum/#!forum/pigweed>`_ for help.

One-time contributor setup
^^^^^^^^^^^^^^^^^^^^^^^^^^
#. Sign the
   `Contributor License Agreement <https://cla.developers.google.com/>`_.
#. Verify that your Git user email (git config user.email) is either Google
   Account email or an Alternate email for the Google account used to sign
   the CLA (Manage Google account → Personal Info → email).
#. Obtain a login cookie from Gerrit's
   `new-password <https://pigweed.googlesource.com/new-password>`_ page.
#. Install the :ref:`gerrit-commit-hook` to automatically add a
   ``Change-Id: ...`` line to your commit.
#. Install the Pigweed presubmit check hook with ``pw presubmit --install``.
   Remember to :ref:`activate-pigweed-environment` first!

Presubmission process
^^^^^^^^^^^^^^^^^^^^^
Before making or sending major changes or SEEDs, please reach out in our
`chat room <https://discord.gg/M9NSeTA>`_ or on the `mailing list
<https://groups.google.com/forum/#!forum/pigweed>`_ first to ensure the changes
make sense for upstream. We generally go through a design phase before making
large changes. See :ref:`SEED-0001` for a description of this process; but
please discuss with us before writing a full SEED. Let us know of any
priorities, timelines, requirements, and limitations ahead of time.

For minor changes that don't fit the SEED process, follow the
:ref:`docs-code_reviews-small-changes` guidance and the `Change submission
process`_.

.. warning::
   Skipping communicating with us before doing large amounts of work risks
   accepting your contribution. Communication is key!

Change submission process
^^^^^^^^^^^^^^^^^^^^^^^^^

.. note::
   A change is a single git commit, and is also known as Change List or CL for
   short.

.. tip::
   Follow :ref:`docs-code_reviews-small-changes` for a smooth submission process

#. Go through the `Presubmission process`_ and review this document's guidance.
#. Ensure all files include the correct copyright and license headers.
#. Include any necessary changes to the documentation.
#. Run :ref:`module-pw_presubmit` to detect style or compilation issues before
   uploading.
#. Upload the change with ``git push origin HEAD:refs/for/main``.
#. Add ``gwsq-pigweed@pigweed.google.com.iam.gserviceaccount.com`` as a
   reviewer. This will automatically choose an appropriate person to review the
   change.
#. Address any reviewer feedback by amending the commit
   (``git commit --amend``).
#. Submit change to CI builders to merge. If you are not part of Pigweed's
   core team, you can ask the reviewer to add the `+2 CQ` vote, which will
   trigger a rebase and submit once the builders pass.

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

.. _docs-contributing-contribution-standards:

Contribution Standards
----------------------
Contributions, i.e. CLs, are expected to be complete solutions, accompanied by
documentation and unit tests when merited, e.g. new code, bug fixes, or code
that changes some behavior. This also means that code changes must be tested.
Tests will avoid or minimize any negative impact to Pigweed users, while
documentation will help others learn about the new changes.

We understand that you may have different priorities or not know the best way to
complete your contribution. In that case, reach out via our `chat room
<https://discord.gg/M9NSeTA>`_ or on the `mailing list
<https://groups.google.com/forum/#!forum/pigweed>`_ for help or `File a bug
<https://issues.pigweed.dev/issues?q=status:open>`_
requesting fixes describing how this may be blocking you. Otherwise you risk
working on a CL that does not match our vision and could be rejected. To keep
our focus, we cannot adopt incomplete CLs.

.. _gerrit-commit-hook:

Gerrit Commit Hook
------------------
Gerrit requires all changes to have a ``Change-Id`` tag at the bottom of each
commit message. You should set this up to be done automatically using the
instructions below.

The commands below assume that your current working directory is the root
of your Pigweed repository.

**Linux/macOS**

.. code-block:: bash

   f=`git rev-parse --git-dir`/hooks/commit-msg ; mkdir -p $(dirname $f) ; curl -Lo $f https://gerrit-review.googlesource.com/tools/hooks/commit-msg ; chmod +x $f

**Windows**

.. code-block:: batch

   git rev-parse --git-dir > gitrepopath.txt & set /p "g="< gitrepopath.txt & del gitrepopath.txt & call set "f=%g%/hooks" & call mkdir "%f%" & call curl -Lo "%f%/commit-msg" https://gerrit-review.googlesource.com/tools/hooks/commit-msg

Commit Message
--------------
See the :ref:`commit message section of the style
guide<docs-pw-style-commit-message>` for how commit messages should look.

Documentation
-------------
Most changes to Pigweed should have an associated documentation change.

Building
^^^^^^^^
To build the documentation, follow the :ref:`getting
started<docs-get-started-upstream>` guide so you can build Pigweed. Then:

#. Change to your checkout directory and ``. activate.sh`` if necessary
#. Run ``pw watch out`` to build the code, run tests, and build docs
#. Wait for the build to finish (see a ``PASS``)
#. Navigate to  ``<CHECKOUT>/out/docs/gen/docs/html/index.html``
#. Edit the relevant ``.rst`` file. Save when ready
#. Refresh your browser after the build completes

Alternately, you can use the local webserver in watch; this works better for
some pages that have external resources: ``pw watch --serve-docs`` then
navigate to `http://localhost:8000 <http://localhost:8000>`_ in your browser.

Submission checklist
^^^^^^^^^^^^^^^^^^^^
All Pigweed changes must either:

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
See :ref:`docs-code_reviews` for information about the code review process.

Experimental Repository and Where to Land Code
----------------------------------------------
Pigweed's has an `Experimental Repository
<https://pigweed.googlesource.com/pigweed/experimental>`_ which differs from
our main repository in a couple key ways:

* Code is not expected to become production grade.
* Code review standards are relaxed to allow experimentation.
* In general the value of the code in the repository is the knowledge gained
  from the experiment, not the code itself.

Good uses of the repo include:

* Experimenting with using an API (ex. C++20 coroutines) with no plans to
  turn it into production code.
* One-off test programs to gather data.

We would like to avoid large pieces of code being developed in the experimental
repository then imported into the Pigweed repository. If large amounts of code
end up needing to migrate from experimental to main, then it must be landed
incrementally as a series of reviewable patches, typically no
`larger than 500 lines each
<https://google.github.io/eng-practices/review/developer/small-cls.html>`_.
This creates a large code review burden that often results in poorer reviews.
Therefore, if the eventual location of the code will be the main Pigweed
repository, it is **strongly encouraged** that the code be developed in the
**main repository under an experimental flag**.

.. note::
   The current organization of the experimental repository does not reflect
   this policy. This will be re-organized once we have concrete recommendations
   on its organization.

Community Guidelines
--------------------
This project follows `Google's Open Source Community Guidelines
<https://opensource.google/conduct/>`_ and the :ref:`docs-code-of-conduct`.

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
.. code-block:: bash

  $ pw presubmit --install

This will be effectively the same as running the following command before every
``git push``:

.. code-block:: bash

  $ pw presubmit


.. image:: ../../pw_presubmit/docs/pw_presubmit_demo.gif
  :width: 800
  :alt: pw presubmit demo

If you ever need to bypass the presubmit hook (due to it being broken, for
example) you may push using this command:

.. code-block:: bash

  $ git push origin HEAD:refs/for/main --no-verify

Presubmit and branch management
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
When creating new feature branches, make sure to specify the upstream branch to
track, e.g.

.. code-block:: bash

  $ git checkout -b myfeature origin/main

When tracking an upstream branch, ``pw presubmit`` will only run checks on the
modified files, rather than the entire repository.

Presubmit flags
^^^^^^^^^^^^^^^
``pw presubmit`` can accept a number of flags

``-b commit, --base commit``
  Git revision against which to diff for changed files. Default is the tracking
  branch of the current branch. Set commit to "HEAD" to check files added or
  modified but not yet commited. Cannot be used with --full.

``--full``
  Run presubmit on all files, not just changed files. Cannot be used with
  --base.

``-e regular_expression, --exclude regular_expression``
  Exclude paths matching any of these regular expressions, which are interpreted
  relative to each Git repository's root.

``-k, --keep-going``
  Continue instead of aborting when errors occur.

``--output-directory OUTPUT_DIRECTORY``
  Output directory (default: <repo root>/out/presubmit)

``--package-root PACKAGE_ROOT``
  Package root directory (default: <output directory>/packages)

``--clear, --clean``
  Delete the presubmit output directory and exit.

``-p, --program PROGRAM``
  Which presubmit program to run

``--step STEP``
  Provide explicit steps instead of running a predefined program.

``--install``
  Install the presubmit as a Git pre-push hook and exit.

.. _Sphinx: https://www.sphinx-doc.org/

.. inclusive-language: disable

.. _reStructuredText Primer: https://www.sphinx-doc.org/en/master/usage/restructuredtext/basics.html

.. inclusive-language: enable

.. _docs-contributing-presubmit-virtualenv-hashes:

Updating Python dependencies in the virtualenv_setup directory
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
If you update any of the requirements or constraints files in
``//pw_env_setup/py/pw_env_setup/virtualenv_setup``, you must run this command
to ensure that all of the hashes are updated:

.. code-block:: console

   pw presubmit --step update_upstream_python_constraints --full

For Python packages that have native extensions, the command needs to be run 3
times: once on Linux, once on macOS, and once on Windows. Please run it on the
OSes that are available to you; a core Pigweed teammate will run it on the rest.
See the warning about caching Python packages for multiple platforms in
:ref:`docs-python-build-downloading-packages`.

.. toctree::
   :maxdepth: 1
   :hidden:

   ../embedded_cpp_guide
   ../style_guide
   ../code_reviews
   ../code_of_conduct
   module_docs
   changelog
