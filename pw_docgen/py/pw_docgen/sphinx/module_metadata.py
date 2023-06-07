# Copyright 2023 The Pigweed Authors
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
"""Sphinx directives for Pigweed module metadata"""

from dataclasses import dataclass
from typing import cast, Dict, List, Optional, TypeVar, Union

# We use BeautifulSoup for certain docs rendering features. It may not be
# available in downstream projects. If so, no problem. We fall back to simpler
# docs rendering.
# pylint: disable=import-error
try:
    from bs4 import BeautifulSoup  # type: ignore
    from bs4.element import Tag as HTMLTag  # type: ignore

    bs_enabled = True
except ModuleNotFoundError:
    bs_enabled = False
# pylint: enable=import-error

import docutils
from docutils import nodes
import docutils.statemachine

# pylint: disable=consider-using-from-import
import docutils.parsers.rst.directives as directives  # type: ignore

# pylint: enable=consider-using-from-import
from sphinx import addnodes
from sphinx.application import Sphinx as SphinxApplication
from sphinx.environment import BuildEnvironment
from sphinx.util.docutils import SphinxDirective

from sphinx_design.cards import CardDirective

EnvAttrT = TypeVar('EnvAttrT')


@dataclass
class ParsedBody:
    topnav: str
    body_without_topnav: str


class EnvMetadata:
    """Easier access to the Sphinx `env` for custom metadata.

    You can store things in the Sphinx `env`, which is just a dict. But each
    time you do, you have to handle the possibility that the key you want
    hasn't been set yet, and set it to a default. The `env` is also untyped,
    so you have to cast the value you get to whatever type you expect it to be.

    Or you can use this class to define your metadata keys up front, and just
    access them like: `value = EnvMetadata(env).my_value`

    ... which will handle initializing the value if it hasn't been yet and
    provide you a typed result.
    """

    def __init__(self, env: BuildEnvironment):
        self._env = env

    def _get_env_attr(self, attr: str, default: EnvAttrT) -> EnvAttrT:
        if not hasattr(self._env, attr):
            value: EnvAttrT = default
            setattr(self._env, attr, value)
        else:
            value = getattr(self._env, attr)

        return value

    @property
    def pw_parsed_bodies(self) -> Dict[str, ParsedBody]:
        default: Dict[str, ParsedBody] = {}
        return self._get_env_attr('pw_module_nav', default)


def status_choice(arg):
    return directives.choice(arg, ('experimental', 'unstable', 'stable'))


def status_badge(module_status: str) -> str:
    """Given a module status, return the status badge for rendering."""
    role = ':bdg-primary:'
    return role + f'`{module_status.title()}`'


def cs_url(module_name: str) -> str:
    """Return the codesearch URL for the given module."""
    return f'https://cs.opensource.google/pigweed/pigweed/+/main:{module_name}/'


def concat_tags(*tag_lists: List[str]) -> List[str]:
    """Given a list of tag lists, return them concat'ed and ready for render."""

    all_tags = tag_lists[0]

    for tag_list in tag_lists[1:]:
        if len(tag_list) > 0:
            all_tags.append(':octicon:`dot-fill`')
            all_tags.extend(tag_list)

    return all_tags


def parse_nav(nav_option: str) -> Dict[str, str]:
    """Parse the `nav` option.

    The value for this option must contain key-value pairs separated by ':',
    with each pair on a separate line. The value is a Sphinx ref pointing to
    the destination of the nav button link, and the key is the name of the nav
    button (see :func:`nav_name`).
    """
    try:
        return {
            line.split(':')[0].strip(): line.split(':')[1].strip()
            for line in nav_option.split('\n')
        }
    except IndexError:
        raise ValueError(
            'Nav parsing failure. Ensure each line has a single pair in the '
            f'form `name: ref`. You provided this:\n{nav_option}'
        )


def nav_name(name: str) -> str:
    """Return the display name for a particular nav button name.

    By default, the returned display name be the provided name with the words
    capitalized. However, this can be overridden by adding an entry to the
    canonical names list.
    """
    canonical_names: Dict[str, str] = {
        'api': 'API Reference',
        'cli': 'CLI Reference',
        'gui': 'GUI Reference',
    }

    if name in canonical_names:
        return canonical_names[name]

    return ' '.join(
        [name_part.lower().capitalize() for name_part in name.split()]
    )


def create_ref_node(
    docname: str, rawtext: str, target: str, classes: Optional[List[str]] = None
) -> nodes.Node:
    """Create a reference node."""

    if classes is None:
        classes = []

    options = {
        "classes": classes,
        "reftarget": target,
        "refdoc": docname,
        "refdomain": "std",
        "reftype": "ref",
        "refexplicit": True,
        "refwarn": True,
    }

    return addnodes.pending_xref(rawtext, **options)


def create_topnav(
    docname: str,
    title: str,
    subtitle: str,
    nav: Dict[str, str],
    extra_classes: Optional[List[str]] = None,
) -> nodes.Node:
    """Create the nodes for the top title and navigation bar."""

    topnav_classes = (
        ['pw-topnav'] + extra_classes if extra_classes is not None else []
    )

    topnav_container = nodes.container(classes=topnav_classes)
    topnav_inline_container = nodes.container(classes=['pw-topnav-inline'])
    topnav_container += topnav_inline_container

    title_node = nodes.paragraph(
        classes=['pw-topnav-title'],
        text=title,
    )

    topnav_inline_container += title_node
    nav_group = nodes.bullet_list(classes=['pw-module-section-nav-group'])

    for raw_name, ref in nav.items():
        name = nav_name(raw_name)
        ref_node = create_ref_node(docname, name, ref)
        ref_node.append(nodes.inline(name, name))  # type: ignore
        p_node = nodes.paragraph()
        p_node += ref_node
        nav_link = nodes.list_item(classes=['pw-module-section-nav-link'])
        nav_link += p_node
        nav_group += nav_link

    topnav_inline_container += nav_group

    subtitle_node = nodes.paragraph(
        classes=['pw-topnav-subtitle'],
        text=subtitle,
    )

    topnav_container += subtitle_node
    return topnav_container


class PigweedModuleDirective(SphinxDirective):
    """Directive registering module metadata, rendering title & info card."""

    required_arguments = 0
    final_argument_whitespace = True
    has_content = True
    option_spec = {
        'name': directives.unchanged_required,
        'tagline': directives.unchanged_required,
        'status': status_choice,
        'is-deprecated': directives.flag,
        'languages': directives.unchanged,
        'code-size-impact': directives.unchanged,
        'facade': directives.unchanged,
        'nav': directives.unchanged_required,
    }

    def _try_get_option(self, option: str):
        """Try to get an option by name and raise on failure."""

        try:
            return self.options[option]
        except KeyError:
            raise self.error(f' :{option}: option is required')

    def _maybe_get_option(self, option: str):
        """Try to get an option by name and return None on failure."""
        return self.options.get(option, None)

    def run(self) -> List[nodes.Node]:
        module_name = self._try_get_option('name')
        tagline = self._try_get_option('tagline')

        status_tags: List[str] = [
            status_badge(self._try_get_option('status')),
        ]

        if 'is-deprecated' in self.options:
            status_tags.append(':bdg-danger:`Deprecated`')

        language_tags = []

        if 'languages' in self.options:
            languages = self.options['languages'].split(',')

            if len(languages) > 0:
                for language in languages:
                    language_tags.append(f':bdg-info:`{language.strip()}`')

        code_size_impact = []

        if code_size_text := self._maybe_get_option('code-size-impact'):
            code_size_impact.append(f'**Code Size Impact:** {code_size_text}')

        # Move the directive content into a section that we can render wherever
        # we want.
        raw_content = cast(List[str], self.content)  # type: ignore
        content = nodes.paragraph()
        self.state.nested_parse(raw_content, 0, content)

        # The card inherits its content from this node's content, which we've
        # already pulled out. So we can replace this node's content with the
        # content we need in the card.
        self.content = docutils.statemachine.StringList(
            concat_tags(status_tags, language_tags, code_size_impact)
        )

        card = CardDirective.create_card(
            inst=self,
            arguments=[],
            options={},
        )

        try:
            nav = parse_nav(self._try_get_option('nav'))
        except ValueError as err:
            raise self.error(err)

        topbar = create_topnav(
            self.env.docname,
            module_name,
            tagline,
            nav,
            ['pw-module-index'],
        )

        return [topbar, card, content]


class PigweedModuleSubpageDirective(PigweedModuleDirective):
    """Directive registering module metadata, rendering title & info card."""

    required_arguments = 0
    final_argument_whitespace = True
    has_content = True
    option_spec = {
        'name': directives.unchanged_required,
        'tagline': directives.unchanged_required,
        'nav': directives.unchanged_required,
    }

    def run(self) -> List[nodes.Node]:
        module_name = self._try_get_option('name')
        tagline = self._try_get_option('tagline')

        try:
            nav = parse_nav(self._try_get_option('nav'))
        except ValueError as err:
            raise self.error(err)

        topbar = create_topnav(
            self.env.docname,
            module_name,
            tagline,
            nav,
            ['pw-module-subpage'],
        )

        return [topbar]


def _parse_body(body: str) -> ParsedBody:
    """From the `body` HTML, return the topnav and the body without topnav.

    The fundamental idea is this: Our Sphinx directives can only render nodes
    *within* the docutils doc, but we want to elevate the top navbar *outside*
    of that doc into the web theme. Sphinx by itself provides no mechanism for
    this, since it's model looks something like this:

      ┌──────────────────┐
      │ Theme            │
      │  ┌──────────────┐│    When Sphinx builds HTML, the output is plain HTML
      │  │ Sphinx HTML  ││    with a structure defined by docutils. Themes can
      │  │              ││    build *around* that and cascade styles down *into*
      │  │              ││    that HTML, but there's no mechanism in the Sphinx
      │  └──────────────┘│    build to render docutils nodes in the theme.
      └──────────────────┘

    The escape hatch is this:
    - Render things within the Sphinx HTML output (`body`)
    - Use Sphinx theme templates to run code during the final render phase
    - Extract the HTML from the `body` and insert it in the theme via templates

    So this function extracts the things that we rendered in the `body` but
    actually want in the theme (the top navbar), returns them for rendering in
    the template, and returns the `body` with those things removed.
    """
    if not bs_enabled:
        return ParsedBody('', body)

    def _add_class_to_tag(tag: HTMLTag, classname: str) -> None:
        tag['class'] = tag.get('class', []) + [classname]  # type: ignore

    def _add_classes_to_tag(
        tag: HTMLTag, classnames: Union[str, List[str], None]
    ) -> None:
        tag['class'] = tag.get('class', []) + classnames  # type: ignore

    html = BeautifulSoup(body, features='html.parser')

    # Render the doc unchanged, unless it has the module doc topnav
    if (topnav := html.find('div', attrs={'class': 'pw-topnav'})) is None:
        return ParsedBody('', body)

    assert isinstance(topnav, HTMLTag)

    # Find the topnav title and subtitle
    topnav_title = topnav.find('p', attrs={'class': 'pw-topnav-title'})
    topnav_subtitle = topnav.find('p', attrs={'class': 'pw-topnav-subtitle'})
    assert isinstance(topnav_title, HTMLTag)
    assert isinstance(topnav_subtitle, HTMLTag)

    # Find the single `h1` element, the doc's canonical title
    doc_title = html.find('h1')
    assert isinstance(doc_title, HTMLTag)

    topnav_str = ''

    if 'pw-module-index' in topnav['class']:
        # Take the standard Sphinx/docutils title and add topnav styling
        _add_class_to_tag(doc_title, 'pw-topnav-title')
        # Replace the placeholder title in the topnav with the "official" `h1`
        topnav_title.replace_with(doc_title)
        # Promote the subtitle to `h2`
        topnav_subtitle.name = 'h2'
        # We're done mutating topnav; write it to string for rendering elsewhere
        topnav_str = str(topnav)
        # Destroy the instance that was rendered in the document
        topnav.decompose()

    elif 'pw-module-subpage' in topnav['class']:
        # Take the title from the topnav (the module name), promote it to `h1`
        topnav_title.name = 'h1'
        # Add the heading link, but pointed to the module index page
        heading_link = html.new_tag(
            'a',
            attrs={
                'class': ['headerlink'],
                'href': 'docs.html',
                'title': 'Permalink to module index',
            },
        )
        heading_link.string = '#'
        topnav_title.append(heading_link)
        # Promote the subtitle to `h2`
        topnav_subtitle.name = 'h2'
        # We're done mutating topnav; write it to string for rendering elsewhere
        topnav_str = str(topnav)
        # Destroy the instance that was rendered in the document
        topnav.decompose()

    return ParsedBody(topnav_str, str(html))


def setup_parse_body(_app, _pagename, _templatename, context, _doctree):
    def parse_body(body: str) -> ParsedBody:
        return _parse_body(body)

    context['parse_body'] = parse_body


def setup(app: SphinxApplication):
    app.add_directive('pigweed-module', PigweedModuleDirective)
    app.add_directive('pigweed-module-subpage', PigweedModuleSubpageDirective)
    app.connect('html-page-context', setup_parse_body)

    return {
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }
