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
"""Generates additional documentation for SEEDs."""

import argparse
import json
from pathlib import Path


def _parse_args() -> argparse.Namespace:
    """Parses command-line arguments."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--output',
        type=Path,
        required=True,
        help='Output file to write',
    )
    parser.add_argument(
        'seed_metadata',
        nargs='+',
        help='JSON file containing information about a SEED',
    )

    return parser.parse_args()


_STATUS_BADGES = {
    'draft': 'bdg-primary-line',
    'open for comments': 'bdg-primary',
    'last call': 'bdg-warning',
    'accepted': 'bdg-success',
    'rejected': 'bdg-danger',
    'deprecated': 'bdg-secondary',
    'superseded': 'bdg-info',
    'on hold': 'bdg-secondary-line',
    'meta': 'bdg-success-line',
}


def _status_badge(status: str) -> str:
    badge = _STATUS_BADGES.get(status.lower().strip(), 'bdg-primary')
    return f':{badge}:`{status}`'


def _main():
    args = _parse_args()

    # Table containing entries for each SEED.
    seed_table = [
        '\n.. list-table::',
        '   :widths: auto',
        '   :header-rows: 1\n',
        '   * - Number',
        '     - Title',
        '     - Status',
        '     - Author',
    ]

    # RST toctree including each SEED's source file.
    seed_toctree = []

    for metadata_file in args.seed_metadata:
        with open(metadata_file, 'r') as file:
            meta = json.load(file)

            if 'changelist' in meta:
                # The SEED has not yet been merged and points to an active CL.
                change_url = (
                    'https://pigweed-review.googlesource.com'
                    f'/c/pigweed/pigweed/+/{meta["changelist"]}'
                )
                title = f'`{meta["title"]} <{change_url}>`__'
                seed_toctree.append(
                    f'{meta["number"]}: {meta["title"]}<{change_url}>',
                )
            else:
                # The SEED document is in the source tree.
                title = f':ref:`seed-{meta["number"]}`'
                seed_toctree.append(Path(meta["rst_file"]).stem)

            seed_table.extend(
                [
                    f'   * - {meta["number"]}',
                    f'     - {title}',
                    f'     - {_status_badge(meta["status"])}',
                    f'     - {meta["author"]}',
                ]
            )

    table = '\n'.join(seed_table)

    # Hide the toctree it is only used for the sidebar.
    toctree_lines = '\n  '.join(seed_toctree)

    # fmt: off
    toctree = (
        '.. toctree::\n'
        '  :hidden:\n'
        '\n  '
        f'{toctree_lines}'
    )
    # fmt: on

    args.output.write_text(f'{table}\n\n{toctree}')


if __name__ == '__main__':
    _main()
