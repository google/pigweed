.. _contrib-docs-guides:

=======================
Docs contributor guides
=======================
This page explains how to do common tasks related to:

* Authoring ``pigweed.dev`` content
* Customizing and maintaining the ``pigweed.dev`` website

.. important::

   This guide is incomplete. If you need to make content updates **right now**,
   follow the guidance in :ref:`docs-get-started-upstream`. The rest of this
   quickstart focuses on the new, work-in-progress Bazel-based docs system.
   You should ignore this quickstart unless a Pigweed teammate has specifically
   pointed you here.

.. _contrib-docs-guides-setup:

-----------------------------------
Set up your development environment
-----------------------------------
#. Complete :ref:`docs-first-time-setup`.

#. :ref:`docs-install-bazel`.

.. _contrib-docs-guides-build:

--------------
Build the docs
--------------
.. code-block:: console

   bazelisk build //docs/...

.. _contrib-docs-guides-build-tree:

Build the underlying sources directory
======================================
Use this command to verify that files are being copied over to
the expected location:

.. code-block:: console

   bazelisk build //docs:docs/_sources

.. _contrib-docs-guides-build-sphinx:

Directly run sphinx-build
=========================
.. inclusive-language: disable
.. _sphinx-build: https://www.sphinx-doc.org/en/master/man/sphinx-build.html
.. inclusive-language: enable

Use this command to mimic directly running `sphinx-build`_
from a non-Bazel context. For example, you could set a breakpoint
in your Sphinx extension, then run this command, then step through
the code with ``pdb``.

.. code-block:: console

   bazelisk run //docs:docs.run

.. _contrib-docs-guides-preview:

------------------------
Locally preview the docs
------------------------
.. code-block:: console

   bazelisk run //docs:docs.serve

A message like this should get printed to ``stdout``:

.. code-block:: console

   Serving...
     Address: http://0.0.0.0:8000
     Serving directory: /home/kayce/pigweed/pigweed/bazel-out/k8-fastbuild/bin/docs/docs/_build/html
         url: file:///home/kayce/pigweed/pigweed/bazel-out/k8-fastbuild/bin/docs/docs/_build/html
     Server CWD: /home/kayce/.cache/bazel/_bazel_kayce/9659373b1552c281136de1c8eeb3080d/execroot/_main/bazel-out/k8-fastbuild/bin/docs/docs.serve.runfiles/_main

You can access the rendered docs at the URL that's printed next to
**Address**.

.. _contrib-docs-guides-site:

---------------
Website updates
---------------
.. _Sphinx: https://www.sphinx-doc.org

This section discusses how to make frontend and backend website changes
to ``pigweed.dev``, Pigweed's main documentation website, and how to
customize `Sphinx`_, the website generator that powers ``pigweed.dev``.

.. _contrib-docs-guides-site-images:

Image hosting
=============
Images should not be checked into the Pigweed repository because
it significantly slows down the repository cloning process.
Images should instead be hosted on Pigweed's image CDN,
``https://storage.googleapis.com/pigweed-media``.

If you're adding an image to a ``pigweed.dev`` doc, here's the
recommended workflow:

#. When drafting a change, it's OK to temporarily check
   in the image so that there is a record of it in Gerrit.

#. When your change is almost ready to merge, a Pigweed teammate
   will upload your image to Pigweed's image CDN, and then
   leave a comment on your change asking you to delete the
   checked-in image and replace the reference to it with the
   URL to the CDN-hosted image.

.. _contrib-docs-guides-site-redirects:

Create redirects
================
.. _sphinx-reredirects: https://pypi.org/project/sphinx-reredirects/

``pigweed.dev`` supports client-side HTML redirects. The redirects are powered
by `sphinx-reredirects`_.

To create a redirect:

#. Open ``//docs/conf.py``.

.. _Usage: https://documatt.com/sphinx-reredirects/usage.html

#. Add your redirect to the ``redirects`` dict. See the
   `Usage`_ section from the ``sphinx-reredirects`` docs.

   * The redirect should be relative to the source path (i.e. the path that
     needs to get redirected).

   * The target path (i.e. the destination path that the source path should
     redirect to) should include the full HTML filename to ensure that the
     redirects also work during local development. In other words, prefer
     ``./example/docs.html`` over ``./example/``.

For each URL that should be redirected, ``sphinx-reredirects`` auto-generates
HTML like this:

.. code-block:: html

   <html>
     <head>
       <meta http-equiv="refresh" content="0; url=pw_sys_io_rp2040/">
     </head>
   </html>

.. _meta refresh and its HTTP equivalent: https://developers.google.com/search/docs/crawling-indexing/301-redirects#metarefresh

.. note::

   Server-side redirects are the most robust solution, but client-side
   redirects are good enough for our needs:

   * Client-side redirects are supported in all browsers and should
     therefore work for all real ``pigweed.dev`` readers.

   * Client-side redirects were much easier and faster to implement.

   * Client-side redirects can be stored alongside the rest of the
     ``pigweed.dev`` source code.

   * Google Search interprets the kind of client side redirects that we use
     as permanent redirects, which is the behavior we want. See
     `meta refresh and its HTTP equivalent`_. The type of client-side redirect
     we used is called a "instant ``meta refresh`` redirect" in that guide.

.. _contrib-docs-guides-site-urls:

Auto-generated source code and issues URLS
==========================================
In the site nav there's a ``Source code`` and ``Issues`` URL for each module.
These links are auto-generated. The auto-generation logic lives in
``//pw_docgen/py/pw_docgen/sphinx/module_metadata.py``.

.. _contrib-docs-guides-site-copy:

Copy-to-clipboard feature on code blocks
========================================
.. _sphinx-copybutton: https://sphinx-copybutton.readthedocs.io/en/latest/
.. _Remove copybuttons using a CSS selector: https://sphinx-copybutton.readthedocs.io/en/latest/use.html#remove-copybuttons-using-a-css-selector

The copy-to-clipboard feature on code blocks is powered by `sphinx-copybutton`_.

``sphinx-copybutton`` recognizes ``$`` as an input prompt and automatically
removes it.

There is a workflow for manually removing the copy-to-clipboard button for a
particular code block but it has not been implemented yet. See
`Remove copybuttons using a CSS selector`_.

.. _contrib-docs-guides-site-fonts:

Fonts and typography
====================
``pigweed.dev`` is taking an iterative approach to its fonts and typography.
See :bug:`353530954` for context, examples of how to update fonts, and to
leave feedback.

.. _Typography: https://m3.material.io/styles/typography/fonts

Rationale for current choices:

* Headings: ``Lato``. Per UX team's recommendation.
* Copy: ``Noto Sans``. ``Noto`` is one of two fonts recommended by Material
  Design 3. It seems to complement ``Lato`` well. See `Typography`_.
* Code: ``Roboto Mono``. Also per UX team's recommendation. ``Roboto Mono``
  is mature and well-established in this space.

.. _contrib-docs-guides-site-search:

Inline search
=============
In the header of every page there's a search box. When you focus that search
box (or press :kbd:`Ctrl+K`) a search modal appears. After you type some
text in the search modal, you immediately see results below your search query.
The inline search results UX is Pigweed-specific custom logic. That code
lives in ``//docs/_static/js/pigweed.js``. If :bug:`363034219` is successfully
completed then we will remove this custom code from the Pigweed repo.

.. _contrib-docs-guides-site-sitemap:

Sitemap generation
==================
``https://pigweed.dev/sitemap.xml`` is generated by the custom Sphinx Extension
located at ``//docs/_extensions/sitemap.py``. A custom extension is necessary
because the ``pigweed.dev`` production server redirects pages that end in
``…/docs.html`` to ``…/`` (e.g. ``pigweed.dev/pw_string/docs.html`` redirects to
``pigweed.dev/pw_string/``) and no third-party extension supports the kind of
URL rewrite customization that we need. See :bug:`386257958`.
