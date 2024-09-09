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

window.pw = {};

// Display inline search results under the search modal. After the user types
// text in the search box, results are shown underneath the text input box.
// The search is restarted after each keypress.
//
// TODO: b/363034219 - Try to upstream this code into pydata-sphinx-theme.
window.pw.initSearch = () => {
  // Don't interfere with the default search UX on /search.html.
  if (window.location.pathname.endsWith('/search.html')) {
    return;
  }
  // The template //docs/layout/page.html ensures that Search is always
  // loaded at this point.
  // eslint-disable-next-line no-undef
  if (!Search) {
    return;
  }
  // Destroy the previous search container and create a new one.
  window.pw.resetSearchResults();
  let timeoutId = null;
  let lastQuery = '';
  const searchInput = document.querySelector('#search-input');
  // Set up the event handler to initiate searches whenever the user
  // types stuff in the search modal textbox.
  searchInput.addEventListener('keyup', () => {
    const query = searchInput.value;
    // Don't search when there's nothing in the query textbox.
    if (query === '') {
      return;
    }
    // Don't search if there is no detectable change between
    // the last query and the current query. E.g. user presses
    // Tab to start navigating the search results.
    if (query === lastQuery) {
      return;
    }
    // Debounce so that the search only starts only when the
    // user stops typing.
    const delay_ms = 500;
    lastQuery = query;
    if (timeoutId) {
      window.clearTimeout(timeoutId);
    }
    timeoutId = window.setTimeout(() => {
      // The user has changed the search query. Delete the old results.
      window.pw.resetSearchResults();
      // eslint-disable-next-line no-undef
      Search.performSearch(query);
      timeoutId = null;
    }, delay_ms);
  });
};

// Resets the custom search results container to an empty state.
//
// Note that Sphinx assumes that searches are always made from /search.html
// so there's some safeguard logic to make sure the inline search always
// works no matter what pigweed.dev page you're on. b/365179592
//
// TODO: b/363034219 - Try to upstream this code into pydata-sphinx-theme.
window.pw.resetSearchResults = () => {
  let results = document.querySelector('#search-results');
  if (results) {
    results.remove();
  }
  results = document.createElement('section');
  results.classList.add('pw-search-results');
  results.id = 'search-results';
  // The template //docs/layout/page.html ensures that DOCUMENTATION_OPTIONS
  // is always loaded at this point.
  // eslint-disable-next-line no-undef
  if (!DOCUMENTATION_OPTIONS || !DOCUMENTATION_OPTIONS.URL_ROOT) {
    return;
  }
  // eslint-disable-next-line no-undef
  const urlRoot = DOCUMENTATION_OPTIONS.URL_ROOT;
  // As Sphinx populates the search results, this observer makes sure that
  // each URL is correct (i.e. doesn't 404). b/365179592
  const linkObserver = new MutationObserver(() => {
    const links = Array.from(
      document.querySelectorAll('#search-results .search a'),
    );
    // Check every link every time because the timing of when new results are
    // added is unpredictable and it's not an expensive operation.
    links.forEach((link) => {
      // Don't use the link.href getter because the browser computes the href
      // as a full URL. We need the relative URL that Sphinx generates.
      const href = link.getAttribute('href');
      if (!href.startsWith(urlRoot)) {
        link.href = `${urlRoot}${href}`;
      }
    });
  });
  // The node that linkObserver watches doesn't exist until the user types
  // something into the search textbox. The next observer just waits for
  // that node to exist and then registers linkObserver on it.
  let isObserved = false;
  const resultsObserver = new MutationObserver(() => {
    if (isObserved) {
      return;
    }
    const container = document.querySelector('#search-results .search');
    if (!container) {
      return;
    }
    linkObserver.observe(container, { childList: true });
    isObserved = true;
  });
  resultsObserver.observe(results, { childList: true });
  // Add the new nodes to the DOM.
  let modal = document.querySelector('.search-button__search-container');
  modal.appendChild(results);
};

window.addEventListener('DOMContentLoaded', () => {
  // Manually control when Mermaid diagrams render to prevent scrolling issues.
  // Context: https://pigweed.dev/docs/style_guide.html#site-nav-scrolling
  if (window.mermaid) {
    // https://mermaid.js.org/config/usage.html#using-mermaid-run
    window.mermaid.run();
  }
  window.pw.initSearch();
});
