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

.. _docs-contrib-docs-website-fonts:

--------------------
Fonts and typography
--------------------
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

.. _docs-contrib-docs-website-search:

-------------
Inline search
-------------
In the header of every page there's a search box. When you focus that search
box (or press :kbd:`Ctrl+K`) a search modal appears. After you type some
text in the search modal, you immediately see results below your search query.
The inline search results UX is Pigweed-specific custom logic. That code
lives in ``//docs/_static/js/pigweed.js``. If :bug:`363034219` is successfully
completed then we will remove this custom code from the Pigweed repo.
