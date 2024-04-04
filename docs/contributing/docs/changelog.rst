.. _docs-contrib-docs-changelog:

=================
Changelog updates
=================
This page describes how to write a bi-weekly :ref:`changelog <docs-changelog>`
update.

The :ref:`docs-contrib-docs-changelog-appendix` contains some general
information about how Pigweed approaches changelogs.

--------
Overview
--------
#. You should start working on the update on the Thursday before Pigweed Live.
   Follow the :ref:`docs-contrib-docs-changelog-instructions`.

#. You should have a rough draft pushed up to Gerrit and ready for review by
   noon on Friday.

#. The update must be published before Pigweed Live.

.. _docs-contrib-docs-changelog-instructions:

------------
Instructions
------------
#. Use the :ref:`changelog tool <docs-contrib-docs-changelog-tool>` to kickstart
   your rough draft. This tool grabs all the commits between the start and end
   dates that you specify, organizes them, and then outputs reStructuredText
   (reST).

#. Copy-paste the reST into ``//docs/changelog.rst``. The new text should go
   right below the line that says ``.. _docs-changelog-latest:``.

#. Go to the last bi-weekly update (the one that was at the top before you added
   your new text) and delete the line that contains
   ``.. changelog_highlights_start`` and also the line that contains
   ``.. changelog_highlights_end``. ``//docs/index.rst`` uses these comments
   to automatically pull the latest changelog highlights onto the homepage.

#. Review each section of the new text:

   * Add a short 1-paragraph summary describing notable changes. Examples of
     notable changes include API changes or a unified collection of commits
     representing a larger body of work.

   * If the commits were trivial or obvious, you don't need to add a summary.

When in doubt about anything, look at ``//docs/changelog.rst`` and follow the
previous precedents.

.. _docs-contrib-docs-changelog-tool:

--------------
Changelog tool
--------------
.. raw:: html

   <label for="start">Start:</label>
   <input type="text" id="start">
   <label for="end">End:</label>
   <input type="text" id="end">
   <button id="generate" disabled>Generate</button>
   <noscript>
     It seems like you have JavaScript disabled. This tool requires JavaScript.
   </noscript>
   <p>
     Status: <span id="status">Waiting for the start and end dates (YYYY-MM-DD format)</span>
   </p>
   <section id="output">Output will be rendered here...</section>
   <!-- Use a relative path here so that the changelog tool also works when
        you preview the page locally on a `file:///...` path. -->
   <script src="../../../_static/js/changelog.js"></script>

.. _docs-contrib-docs-changelog-appendix:

--------
Appendix
--------

Why "changelog" and not "release notes"?
========================================
Because Pigweed doesn't have releases.

Why organize by module and category?
====================================
Why not organize by features, fixes, and breaking changes?

* Because some Pigweed customers only use a few modules. Organizing by module
  helps them filter out all the changes that aren't relevant to them faster.
* If we keep the changelog section heading text fairly structured, we may
  be able to present the changelog in other interesting ways. For example,
  it should be possible to collect every ``pw_base64`` section in the changelog
  and then provide a changelog for only ``pw_base64`` over in the ``pw_base64``
  docs.
* The changelog tool is easily able to organize by module and category due to
  how we annotate our commits. We will not be able to publish changelog updates
  every 2 weeks if there is too much manual work involved.
