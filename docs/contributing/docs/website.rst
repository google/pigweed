.. _docs-contrib-docs-website:

===========
pigweed.dev
===========
Guides and resources for people who want to make changes to Pigweed's
documentation website, ``pigweed.dev``.

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
