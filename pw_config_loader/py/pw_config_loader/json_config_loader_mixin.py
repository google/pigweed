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
"""JSON config file loader mixin."""

from typing import Any

import json

from pw_config_loader.yaml_config_loader_mixin import YamlConfigLoaderMixin


class JsonConfigLoaderMixin(YamlConfigLoaderMixin):
    """JSON Config file loader mixin.

    Use this mixin to load json file settings and save them into
    ``self._config``. For example:

    ::

       from pw_cli.json_config_loader_mixin import JsonConfigLoaderMixin

       class PwBloatPrefs(JsonConfigLoaderMixin):
           def __init__(self) -> None:
               self.config_init(
                   config_section_title='pw_bloat',
                   project_file=Path('$PW_PROJECT_ROOT/.pw_bloat.json'),
                   project_user_file=Path(
                       '$PW_PROJECT_ROOT/.pw_bloat.user.json'),
                   user_file=Path('~/.pw_bloat.json'),
                   default_config={},
                   environment_var='PW_BLOAT_CONFIG_FILE',
               )

    """

    def _load_config_from_string(  # pylint: disable=no-self-use
        self, file_contents: str
    ) -> list[dict[Any, Any]]:
        return [json.loads(file_contents)]
