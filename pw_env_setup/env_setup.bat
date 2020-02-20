@echo off
:: Copyright 2020 The Pigweed Authors
::
:: Licensed under the Apache License, Version 2.0 (the "License"); you may not
:: use this file except in compliance with the License. You may obtain a copy of
:: the License at
::
::     https://www.apache.org/licenses/LICENSE-2.0
::
:: Unless required by applicable law or agreed to in writing, software
:: distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
:: WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
:: License for the specific language governing permissions and limitations under
:: the License.

:: Pigweed Windows environment setup.

:: Calls a Powershell script that determines the correct PW_ROOT directory and
:: exports it as an environment variable.
:: LUCI DELETE BEGIN
:: The ~dp0 magic doesn't work when this batch script is launched from another
:: batch script. So when testing on Windows we write a copy of this batch
:: script that assumes PW_ROOT is already set. Then the batch script we run
:: directly sets PW_ROOT, calls the copy of this batch script and runs
:: 'pw presubmit'.
for /F "usebackq tokens=1" %%i in (`powershell %%~dp0..\..\pw_env_setup\env_setup.ps1`) do set PW_ROOT=%%i
:: LUCI DELETE END

set shell_file="%PW_ROOT%\pw_env_setup\.env_setup.bat"

if not exist %shell_file% (
  call python %PW_ROOT%\pw_env_setup\py\pw_env_setup\env_setup.py --pw-root %PW_ROOT% --shell-file %shell_file%
)

call %shell_file%
