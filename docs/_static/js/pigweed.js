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

function scrollSiteNavToCurrentPage() {
  const siteNav = document.querySelector('.sidebar-scroll');
  // The node within the site nav that represents the page that the user is
  // currently looking at.
  const currentPage = document.querySelector('.current-page');
  // Determine which site nav node to scroll to.
  let targetNode;
  if (currentPage.classList.contains('toctree-l1') ||
      currentPage.classList.contains('toctree-l2')) {
    // Scroll directly to top-level (L1) and second-level (L2) nodes.
    targetNode = currentPage;
  } else {
    // For L3 nodes and deeper, scroll to the node's L2 ancestor so that the
    // user sees all the docs in the set.
    targetNode = document.querySelector('li.toctree-l2.current');
  }
  scrollDistance = targetNode.getBoundingClientRect().top -
      siteNav.getBoundingClientRect().top;
  siteNav.scrollTop = scrollDistance;
}

window.addEventListener('load', () => {
  // Run the scrolling function with a 1-second delay so that it doesn't
  // interfere with Sphinx's scrolling function. E.g. when you visit
  // https://pigweed.dev/pw_tokenizer/design.html#bit-tokenization we need
  // to give Sphinx a chance to scroll to the #bit-tokenization section.
  setTimeout(scrollSiteNavToCurrentPage, 1000);
});
