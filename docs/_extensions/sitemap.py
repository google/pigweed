# Copyright 2024 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Generates a sitemap for pigweed.dev."""


from sphinx.application import Sphinx


def build_url(docname: str) -> str:
    url = f'https://pigweed.dev/{docname}.html'
    # The pigweed.dev production server redirects pages that end with
    # `…/docs.html` to `…/`. E.g. `https://pigweed.dev/pw_string/docs.html`
    # redirects to `https://pigweed.dev/pw_string/`. The latter is the version
    # that should be recorded in the sitemap for optimal SEO. b/386257958
    #
    # Be careful not to clobber other files that end in `docs.html` such
    # as `https://pigweed.dev/targets/rp2040/target_docs.html`.
    #
    # The server also redirects pages that end in `…/index.html`.
    redirects = [
        '/docs.html',
        '/index.html',
    ]
    for pattern in redirects:
        if url.endswith(pattern):
            end = url.rfind(pattern)
            url = url[0:end]
            url += '/'
    return url


def generate_sitemap(app: Sphinx, exception: Exception | None) -> None:
    urls = []
    for docname in app.project.docnames:
        urls.append(build_url(docname))
    urls.sort()
    sitemap = '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n'
    for url in urls:
        sitemap += f'  <url><loc>{url}</loc></url>\n'
    sitemap += '</urlset>\n'
    with open(f'{app.outdir}/sitemap.xml', 'w') as f:
        f.write(sitemap)


def setup(app: Sphinx) -> dict[str, bool]:
    app.connect('build-finished', generate_sitemap)
    return {
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }
