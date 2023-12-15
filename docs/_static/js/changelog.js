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

// This file powers the changelog tool in //docs/contributing/changelog.rst.
// We use this tool to speed up the generation of bi-weekly changelog
// updates. It fetches the commits over a user-specified timeframe, derives
// a little metadata about each commit, organizes the commits, and renders
// the data as reStructuredText. It doesn't completely automate the changelog
// update process (a contributor still needs to manually write the summaries)
// but it does reduce a lot of the toil.

// Get the commits from the user-specified timeframe.
async function get() {
  const start = `${document.querySelector('#start').value}T00:00:00Z`;
  const end = `${document.querySelector('#end').value}T23:59:59Z`;
  document.querySelector('#status').textContent = `Getting commit data...`;
  let page = 1;
  let done = false;
  let commits = [];
  while (!done) {
    // The commits are pulled from the Pigweed mirror on GitHub because
    // GitHub provides a better API than Gerrit for this task.
    let url = new URL(`https://api.github.com/repos/google/pigweed/commits`);
    const params = { since: start, until: end, per_page: 100, page };
    Object.keys(params).forEach((key) =>
      url.searchParams.append(key, params[key]),
    );
    const headers = {
      Accept: 'application/vnd.github+json',
      'X-GitHub-Api-Version': '2022-11-28',
    };
    const response = await fetch(url.href, { method: 'GET', headers });
    if (!response.ok) {
      document.querySelector('#status').textContent =
        'An error occurred while fetching the commit data.';
      console.error(response);
      return;
    }
    const data = await response.json();
    if (data.length === 0) {
      done = true;
      continue;
    }
    commits = commits.concat(data);
    page += 1;
  }
  return commits;
}

// Weed out all the data that GitHub provides that we don't need.
// Also, parse the "subject" of the commit, which is the first line
// of the commit message.
async function normalize(commits) {
  function parseSubject(message) {
    const end = message.indexOf('\n\n');
    return message.substring(0, end);
  }

  document.querySelector('#status').textContent = 'Normalizing data...';
  let normalizedCommits = [];
  commits.forEach((commit) => {
    normalizedCommits.push({
      sha: commit.sha,
      message: commit.commit.message,
      date: commit.commit.committer.date,
      subject: parseSubject(commit.commit.message),
    });
  });
  return normalizedCommits;
}

// Derive Pigweed-specific metadata from each commit.
async function annotate(commits) {
  function categorize(preamble) {
    if (preamble.startsWith('third_party')) {
      return 'Third party';
    } else if (preamble.startsWith('pw_')) {
      return 'Modules';
    } else if (preamble.startsWith('targets')) {
      return 'Targets';
    } else if (['build', 'bazel', 'cmake'].includes(preamble)) {
      return 'Build';
    } else if (['rust', 'python'].includes(preamble)) {
      return 'Language support';
    } else if (['zephyr', 'freertos'].includes(preamble)) {
      return 'OS support';
    } else if (preamble.startsWith('SEED')) {
      return 'SEEDs';
    } else if (preamble === 'docs') {
      return 'Docs';
    } else {
      return 'Miscellaneous';
    }
  }

  function parseTitle(message) {
    const start = message.indexOf(':') + 1;
    const tmp = message.substring(start);
    const end = tmp.indexOf('\n');
    return tmp.substring(0, end).trim();
  }

  function parseBugUrl(message, bugLabel) {
    const start = message.indexOf(bugLabel);
    const tmp = message.substring(start);
    const end = tmp.indexOf('\n');
    let bug = tmp.substring(bugLabel.length, end).trim();
    if (bug.startsWith('b/')) bug = bug.replace('b/', '');
    return `https://issues.pigweed.dev/issues/${bug}`;
  }

  function parseChangeUrl(message) {
    const label = 'Reviewed-on:';
    const start = message.indexOf(label);
    const tmp = message.substring(start);
    const end = tmp.indexOf('\n');
    const change = tmp.substring(label.length, end).trim();
    return change;
  }

  for (let i = 0; i < commits.length; i++) {
    let commit = commits[i];
    const { message, sha } = commit;
    commit.url = `https://cs.opensource.google/pigweed/pigweed/+/${sha}`;
    commit.change = parseChangeUrl(message);
    commit.summary = message.substring(0, message.indexOf('\n'));
    commit.preamble = message.substring(0, message.indexOf(':'));
    commit.category = categorize(commit.preamble);
    commit.title = parseTitle(message);
    // We use syntax like "pw_{tokenizer,string}" to indicate that a commit
    // affects both pw_tokenizer and pw_string. The next logic detects this
    // situation. The same commit gets duplicated to each module's section.
    // The rationale for the duplication is that someone might only care about
    // pw_tokenizer and they should be able to see all commits that affected
    // in a single place.
    if (commit.preamble.indexOf('{') > -1) {
      commit.topics = [];
      const topics = commit.preamble
        .substring(
          commit.preamble.indexOf('{') + 1,
          commit.preamble.indexOf('}'),
        )
        .split(',');
      topics.forEach((topic) => commit.topics.push(`pw_${topic}`));
    } else {
      commit.topics = [commit.preamble];
    }
    const bugLabels = ['Bug:', 'Fixes:', 'Fixed:'];
    for (let i = 0; i < bugLabels.length; i++) {
      const bugLabel = bugLabels[i];
      if (message.indexOf(bugLabel) > -1) {
        const bugUrl = parseBugUrl(message, bugLabel);
        const bugId = bugUrl.substring(bugUrl.lastIndexOf('/') + 1);
        commit.issue = { id: bugId, url: bugUrl };
        break;
      }
    }
  }
  return commits;
}

// If there are any categories of commits that we don't want to surface
// in the changelog, this function is where we drop them.
async function filter(commits) {
  const filteredCommits = commits.filter((commit) => {
    if (commit.preamble === 'roll') return false;
    return true;
  });
  return filteredCommits;
}

// Render the commit data as reStructuredText.
async function render(commits) {
  function organizeByCategoryAndTopic(commits) {
    let categories = {};
    commits.forEach((commit) => {
      const { category } = commit;
      if (!(category in categories)) categories[category] = {};
      commit.topics.forEach((topic) => {
        topic in categories[category]
          ? categories[category][topic].push(commit)
          : (categories[category][topic] = [commit]);
      });
    });
    return categories;
  }

  async function createRestSection(commits) {
    const locale = 'en-US';
    const format = { day: '2-digit', month: 'short', year: 'numeric' };
    const start = new Date(
      document.querySelector('#start').value,
    ).toLocaleDateString(locale, format);
    const end = new Date(
      document.querySelector('#end').value,
    ).toLocaleDateString(locale, format);
    let rest = '';
    rest += '.. _docs-changelog-latest:\n\n';
    const title = `${end}`;
    rest += `${'-'.repeat(title.length)}\n`;
    rest += `${title}\n`;
    rest += `${'-'.repeat(title.length)}\n\n`;
    rest += '.. changelog_highlights_start\n\n';
    rest += `Highlights (${start} to ${end}):\n\n`;
    rest += '* Highlight #1\n';
    rest += '* Highlight #2\n';
    rest += '* Highlight #3\n\n';
    rest += '.. changelog_highlights_end\n\n';
    rest += 'Active SEEDs\n';
    rest += '============\n';
    rest += 'Help shape the future of Pigweed! Please visit :ref:`seed-0000`\n';
    rest += 'and leave feedback on the RFCs (i.e. SEEDs) marked\n';
    rest += '``Open for Comments``.\n\n';
    rest += '.. Note: There is space between the following section headings\n';
    rest += '.. and commit lists to remind you to write a summary for each\n';
    rest += '.. section. If a summary is not needed, delete the extra\n';
    rest += '.. space.\n\n';
    const categories = [
      'Modules',
      'Build',
      'Targets',
      'Language support',
      'OS support',
      'Docs',
      'SEEDs',
      'Third party',
      'Miscellaneous',
    ];
    for (let i = 0; i < categories.length; i++) {
      const category = categories[i];
      if (!(category in commits)) continue;
      rest += `${category}\n`;
      rest += `${'='.repeat(category.length)}\n\n`;
      let topics = Object.keys(commits[category]);
      topics.sort();
      topics.forEach((topic) => {
        rest += `${topic}\n`;
        rest += `${'-'.repeat(topic.length)}\n\n\n`;
        commits[category][topic].forEach((commit) => {
          const change = commit.change.replaceAll('`', '`');
          // The double underscores are signficant:
          // https://github.com/sphinx-doc/sphinx/issues/3921
          rest += `* \`${commit.title}\n  <${change}>\`__\n`;
          if (commit.issue)
            rest += `  (issue \`#${commit.issue.id} <${commit.issue.url}>\`__)\n`;
        });
        rest += '\n';
      });
    }
    const section = document.createElement('section');
    const heading = document.createElement('h2');
    section.appendChild(heading);
    const pre = document.createElement('pre');
    section.appendChild(pre);
    const code = document.createElement('code');
    pre.appendChild(code);
    code.textContent = rest;
    try {
      await navigator.clipboard.writeText(rest);
      document.querySelector('#status').textContent =
        'Done! The output was copied to your clipboard.';
    } catch (error) {
      document.querySelector('#status').textContent = 'Done!';
    }
    return section;
  }

  const organizedCommits = organizeByCategoryAndTopic(commits);
  document.querySelector('#status').textContent = 'Rendering data...';
  const container = document.createElement('div');
  const restSection = await createRestSection(organizedCommits);
  container.appendChild(restSection);
  return container;
}

// Use the placeholder in the start and end date text inputs to guide users
// towards the correct date format.
function populateDates() {
  // Suggest the start date.
  let twoWeeksAgo = new Date();
  twoWeeksAgo.setDate(twoWeeksAgo.getDate() - 14);
  const twoWeeksAgoFormatted = twoWeeksAgo.toISOString().slice(0, 10);
  document.querySelector('#start').placeholder = twoWeeksAgoFormatted;
  // Suggest the end date.
  const today = new Date();
  const todayFormatted = today.toISOString().slice(0, 10);
  document.querySelector('#end').placeholder = todayFormatted;
}

// Enable the "generate" button only when the start and end dates are valid.
function validateDates() {
  const dateFormat = /^\d{4}-\d{2}-\d{2}$/;
  const start = document.querySelector('#start').value;
  const end = document.querySelector('#end').value;
  const status = document.querySelector('#status');
  let generate = document.querySelector('#generate');
  if (!start.match(dateFormat) || !end.match(dateFormat)) {
    generate.disabled = true;
    status.textContent = 'Invalid start or end date (should be YYYY-MM-DD)';
  } else {
    generate.disabled = false;
    status.textContent = 'Ready to generate!';
  }
}

// Set up the date placeholder and validation stuff when the page loads.
window.addEventListener('load', () => {
  populateDates();
  document.querySelector('#start').addEventListener('keyup', validateDates);
  document.querySelector('#end').addEventListener('keyup', validateDates);
});

// Run through the whole get/normalize/annotate/filter/render pipeline when
// the user clicks the "generate" button.
document.querySelector('#generate').addEventListener('click', async (e) => {
  e.target.disabled = true;
  const rawCommits = await get();
  const normalizedCommits = await normalize(rawCommits);
  const annotatedCommits = await annotate(normalizedCommits);
  const filteredCommits = await filter(annotatedCommits);
  const output = await render(filteredCommits);
  document.querySelector('#output').innerHTML = '';
  document.querySelector('#output').appendChild(output);
  e.target.disabled = false;
});
