// Copyright 2024 The Pigweed Authors
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

document.addEventListener('DOMContentLoaded', () => {
  'use strict';

  const searchModule = function () {
    let index = [];
    const highlight = document.querySelector('#ls-highlight').value === 'true';
    // First we add all page titles
    const SidebarSearchIndex = window.SidebarSearchIndex;
    SidebarSearchIndex.titles.forEach((title, docIndex) => {
      let doc = SidebarSearchIndex.docnames[docIndex];
      if (doc.endsWith('/doc')) doc = doc.slice(0, -3);
      index.push({
        title: title,
        doc: doc,
        hash: '',
      });
    });

    const searchField = document.querySelector('#ls_search-field');
    const searchResults = document.querySelector('#ls_search-results');

    searchField.addEventListener('keyup', onKeyUp);
    searchField.addEventListener('keypress', onKeyPress);
    searchField.addEventListener('focusout', () => {
      setTimeout(() => {
        searchResults.style.display = 'none';
      }, 100);
    });
    searchField.addEventListener('focusin', () => {
      if (searchResults.querySelectorAll('li').length > 0) {
        searchResults.style.display = 'block';
      }
    });

    function onKeyUp(event) {
      const keycode = event.keyCode || event.which;
      const query = searchField.value;
      let results = null;

      if (keycode === 13) {
        return;
      }
      if (keycode === 40 || keycode === 38) {
        handleKeyboardNavigation(keycode);
        return;
      }
      if (query === '') {
        searchResults.innerHTML = '';
        searchResults.style.display = 'none';
        return;
      }

      results = window.fuzzysort.go(query, index, { key: 'title' });
      searchResults.innerHTML = '';
      searchResults.style.display = 'grid';

      if (results.length === 0) {
        searchResults.innerHTML = '<li><a href="#">No results found</a></li>';
      } else {
        results.slice(0, 15).forEach((result) => {
          searchResults.appendChild(createResultListElement(result));
        });
      }

      // Set the width of the dropdown
      searchResults.style.width =
        Math.max(
          searchField.offsetWidth,
          searchResults.querySelector('li').offsetWidth,
        ) + 'px';
    }

    function onKeyPress(event) {
      if (event.keyCode === 13) {
        event.preventDefault();
        const active = searchResults.querySelector('li a.hover');
        if (active) {
          active.click();
        } else {
          window.location.href = `${window.DOCUMENTATION_OPTIONS.URL_ROOT}search.html?q=${searchField.value}&check_keywords=yes&area=default`;
        }
      }
    }

    function handleKeyboardNavigation(keycode) {
      const items = Array.from(searchResults.querySelectorAll('li a'));
      const activeIndex = items.findIndex((item) =>
        item.classList.contains('hover'),
      );

      if (keycode === 40 && activeIndex < items.length - 1) {
        // next
        if (activeIndex > -1) items[activeIndex].classList.remove('hover');
        items[activeIndex + 1].classList.add('hover');
      } else if (keycode === 38 && activeIndex > 0) {
        // prev
        items[activeIndex].classList.remove('hover');
        items[activeIndex - 1].classList.add('hover');
      }
    }

    function buildHref(s) {
      const highlightString = highlight
        ? `?highlight=${encodeURIComponent(s.title || s.heading)}`
        : '';
      return `${window.DOCUMENTATION_OPTIONS.URL_ROOT}${s.doc}${window.DOCUMENTATION_OPTIONS.FILE_SUFFIX}${highlightString}#${s.hash}`;
    }

    function createResultListElement(s) {
      const li = document.createElement('li');
      const a = document.createElement('a');
      a.href = buildHref(s.obj);
      // create content
      const span = document.createElement('span');
      span.innerHTML = window.fuzzysort.highlight(s);
      const small = document.createElement('small');
      small.textContent = `${s.obj.doc}${window.DOCUMENTATION_OPTIONS.FILE_SUFFIX}`;
      a.appendChild(span);
      a.appendChild(small);

      a.addEventListener('mouseenter', () => {
        Array.from(searchResults.querySelectorAll('li a')).forEach((el) =>
          el.classList.remove('hover'),
        );
        a.classList.add('hover');
      });
      a.addEventListener('mouseleave', () => {
        a.classList.remove('hover');
      });
      li.appendChild(a);
      return li;
    }
  };

  searchModule();
});
