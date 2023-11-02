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

window.pigweed = {};

// Scroll the site nav so that the current page is visible.
// Context: https://pigweed.dev/docs/style_guide.html#site-nav-scrolling
window.pigweed.scrollSiteNav = () => {
  const siteNav = document.querySelector('.sidebar-scroll');
  // The node within the site nav that represents the page that the user is
  // currently looking at.
  const currentPage = document.querySelector('.current-page');
  // Determine which site nav node to scroll to. Scroll directly to top-level
  // (L1) and second-level (L2) nodes. For L3 nodes and deeper, scroll to the
  // node's L2 ancestor so that the user sees all the docs in the set.
  let targetNode;
  if (
    currentPage.classList.contains('toctree-l1') ||
    currentPage.classList.contains('toctree-l2')
  ) {
    targetNode = currentPage;
  } else {
    targetNode = document.querySelector('li.toctree-l2.current');
  }
  // Scroll to the node. Note that we also tried scrollIntoView() but
  // it wasn't reliable enough.
  const scrollDistance =
    targetNode.getBoundingClientRect().top -
    siteNav.getBoundingClientRect().top;
  siteNav.scrollTop = scrollDistance;
};

// If the URL has a deep link, make sure the page scrolls to that section.
// Context: https://pigweed.dev/docs/style_guide.html#site-nav-scrolling
window.pigweed.scrollMainContent = () => {
  // Only run this logic if there's a deep link in the URL.
  if (!window.location.hash) {
    return;
  }
  // Make sure that there's actually a node that matches the deep link before
  // attempting to jump to it.
  const targetNode = document.querySelector(window.location.hash);
  if (!targetNode) {
    return;
  }
  // Scroll to the node. Note that we also tried scrollIntoView() but
  // it wasn't reliable enough.
  const mainContent = document.querySelector('div.main');
  const scrollDistance =
    targetNode.getBoundingClientRect().top -
    mainContent.getBoundingClientRect().top;
  mainContent.scrollTop = scrollDistance;
};

window.addEventListener('DOMContentLoaded', () => {
  // Manually control when Mermaid diagrams render to prevent scrolling issues.
  // Context: https://pigweed.dev/docs/style_guide.html#site-nav-scrolling
  if (window.mermaid) {
    // https://mermaid.js.org/config/usage.html#using-mermaid-run
    window.mermaid.run();
  }
  window.pigweed.scrollSiteNav();
  window.pigweed.scrollMainContent();
});
