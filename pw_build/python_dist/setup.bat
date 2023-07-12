:: Copyright 2021 The Pigweed Authors
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
@echo off

set "ROOT_DIR=%~dp0"
set "WHEEL_DIR=%ROOT_DIR%\python_wheels"

:: Generate python virtual environment using existing python.
python -m venv "%ROOT_DIR%\python-venv"
:: Use the venv python for pip installs
set "python=%ROOT_DIR%\python-venv\Scripts\python.exe"

set "CONSTRAINT_PATH=%ROOT_DIR%\constraints.txt"
set "CONSTRAINT_ARG="
if exist "%CONSTRAINT_PATH%" (
  set "CONSTRAINT_ARG=--constraint=%CONSTRAINT_PATH%"
)

set "EXTRA_REQUIREMENT_PATH=%WHEEL_DIR%\requirements.txt"
set "EXTRA_REQUIREMENT_ARG="
if exist "%EXTRA_REQUIREMENT_PATH%" (
  set "EXTRA_REQUIREMENT_ARG=--requirement=%EXTRA_REQUIREMENT_PATH%"
)

:: Run pip install in the venv.
call "%python%" -m pip install --require-hashes ^
    "--find-links=%ROOT_DIR%python_wheels" ^
    "--requirement=requirements.txt" %EXTRA_REQUIREMENT_ARG% %CONSTRAINT_ARG%
