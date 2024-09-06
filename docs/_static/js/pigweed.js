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
// The search is restarted anew after each keypress.
//
// TODO: b/363034219 - Try to upstream this code into pydata-sphinx-theme.
window.pw.initSearch = () => {
  // The search page has its own UI for running searches and displaying
  // results. This logic isn't needed on /search.html.
  if (window.location.pathname.endsWith('/search.html')) {
    return;
  }
  // Search class is provided by Sphinx's built-in search tools.
  // The template //docs/layout/page.html ensures that Sphinx's
  // search scripts are always loaded before pigweed.js.
  // eslint-disable-next-line no-undef
  if (!Search) {
    return;
  }
  window.pw.resetSearchResults();
  let timeoutId = null;
  let lastQuery = '';
  const searchInput = document.querySelector('#search-input');
  // Kick off the search after the user types something.
  searchInput.addEventListener('keyup', () => {
    const query = searchInput.value;
    // Don't search when there's nothing in the query text box.
    if (query === '') {
      return;
    }
    // Don't search if there is no detectable change between
    // the last query and the current query. This prevents the
    // search from re-running if the user presses Tab to start
    // navigating the search results.
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
// TODO: b/363034219 - Try to upstream this code into pydata-sphinx-theme.
window.pw.resetSearchResults = () => {
  let results = document.querySelector('#search-results');
  if (results) {
    results.remove();
  }
  results = document.createElement('section');
  results.classList.add('pw-search-results');
  results.id = 'search-results';
  let container = document.querySelector('.search-button__search-container');
  container.appendChild(results);
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
