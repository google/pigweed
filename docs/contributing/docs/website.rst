.. _docs-contrib-docs-website:

===========
pigweed.dev
===========
How to make frontend and backend website changes to ``pigweed.dev``,
Pigweed's main documentationw website, and how to change the functionality
of Sphinx, the website generator that powers ``pigweed.dev``.

.. _docs-contrib-docs-website-redirects:

----------------
Create redirects
----------------
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
     ``./example/docs.html`` over ``./example/`` because the latter assumes
     the existence of a server that redirects ``./example/`` to
     ``./example/docs.html``.

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

   * Client-side redirects are supported in all browsers and should work
     therefore work for all real ``pigweed.dev`` readers.

   * Client-side redirects were much easier and faster to implement.

   * Client-side redirects can be stored alongside the rest of the
     ``pigweed.dev`` source code.

   * Google Search interprets the kind of client side redirects that we use
     as permanent redirects, which is the behavior we want. See
     `meta refresh and its HTTP equivalent`_. The type of client-side redirect
     we used is called a "instant ``meta refresh`` redirect" in that guide.

.. _docs-contrib-docs-website-breadcrumbs:

-----------
Breadcrumbs
-----------
.. _breadcrumbs: https://en.wikipedia.org/wiki/Breadcrumb_navigation

The `breadcrumbs`_ at the top of each page (except the homepage) is implemented
in ``//docs/layout/page.html``. The CSS for this UI is in
``//docs/_static/css/pigweed.css`` under the ``.breadcrumbs`` and
``.breadcrumb`` classes.

.. _docs-contrib-docs-website-urls:

------------------------------------------
Auto-generated source code and issues URLS
------------------------------------------
In the site nav there's a ``Source code`` and ``Issues`` URL for each module.
These links are auto-generated. The auto-generation logic lives in
``//pw_docgen/py/pw_docgen/sphinx/module_metadata.py``.

.. _docs-contrib-docs-website-copy:

----------------------------------------
Copy-to-clipboard feature on code blocks
----------------------------------------
.. _sphinx-copybutton: https://sphinx-copybutton.readthedocs.io/en/latest/
.. _Remove copybuttons using a CSS selector: https://sphinx-copybutton.readthedocs.io/en/latest/use.html#remove-copybuttons-using-a-css-selector

The copy-to-clipboard feature on code blocks is powered by `sphinx-copybutton`_.

``sphinx-copybutton`` recognizes ``$`` as an input prompt and automatically
removes it.

There is a workflow for manually removing the copy-to-clipboard button for a
particular code block but it has not been implemented yet. See
`Remove copybuttons using a CSS selector`_.

.. _docs-site-scroll:

------------------
Site nav scrolling
------------------
We have had recurring issues with scrolling on pigweed.dev. This section
provides context on the issue and fix(es).

The behavior we want:

* The page that you're currently on should be visible in the site nav.
* URLs with deep links (e.g. ``pigweed.dev/pw_allocator/#size-report``) should
  instantly jump to the target section (e.g. ``#size-report``).
* There should be no scrolling animations anywhere on the site. Scrolls should
  happen instantly.

.. _furo.js: https://github.com/pradyunsg/furo/blob/main/src/furo/assets/scripts/furo.js

A few potential issues at play:

* Our theme (Furo) has non-configurable scrolling logic. See `furo.js`_.
* There seems to be a race condition between Furo's scrolling behavior and our
  text-to-diagram tool, Mermaid, which uses JavaScript to render the diagrams
  on page load. However, we also saw issues on pages that didn't have any
  diagrams, so that can't be the site-wide root cause.

.. _scrollTop: https://developer.mozilla.org/en-US/docs/Web/API/Element/scrollTop
.. _scrollIntoView: https://developer.mozilla.org/en-US/docs/Web/API/Element/scrollIntoView
.. _scroll-behavior: https://developer.mozilla.org/en-US/docs/Web/CSS/scroll-behavior

Our current fix:

* In ``//docs/_static/js/pigweed.js`` we manually scroll the site nav and main
  content via `scrollTop`_. Note that we previously tried `scrollIntoView`_
  but it was not an acceptable fix because the main content would always scroll
  down about 50 pixels, even when a deep link was not present in the URL.
  We also manually control when Mermaid renders its diagrams.
* In ``//docs/_static/css/pigweed.css`` we use an aggressive CSS rule
  to ensure that `scroll-behavior`_ is set to ``auto`` (i.e. instant scrolling)
  for all elements on the site.

Background:

* `Tracking issue <https://issues.pigweed.dev/issues/303261476>`_
* `First fix <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162410>`_
* `Second fix <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/162990>`_
* `Third fix <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/168555>`_
* `Fourth fix <https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/178591>`_
