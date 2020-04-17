:<<"::WINDOWS_ONLY"
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
::WINDOWS_ONLY
:; echo "ERROR: Attempting to run Windows .bat from a Unix/POSIX shell!"
:; echo "Instead, run the following command."
:; echo ""
:; echo "    source ./bootstrap.sh"
:; echo ""
:<<"::WINDOWS_ONLY"

:: Pigweed Windows environment setup.

:: If PW_CHECKOUT_ROOT is set, use it. Users should not set this variable.
:: It's used because when one batch script invokes another the Powershell magic
:: below doesn't work. To reinforce that users should not be using
:: PW_CHECKOUT_ROOT, it is cleared here after it is used, and other pw tools
:: will complain if they see that variable set.
:: TODO(mohrr) find out a way to do this without PW_CHECKOUT_ROOT.
if "%PW_CHECKOUT_ROOT%"=="" (
  :: ~dp0 is the batchism for the directory in which a .bat file resides.
  set "PW_ROOT=%~dp0"
) else (
  set "PW_ROOT=%PW_CHECKOUT_ROOT%"
  set PW_CHECKOUT_ROOT=
)

:: Allow forcing a specific Python version through the environment variable
:: PW_BOOTSTRAP_PYTHON. Otherwise, use the system Python if one exists.
if not "%PW_BOOTSTRAP_PYTHON%" == "" (
  set "python=%PW_BOOTSTRAP_PYTHON%"
) else (
  where python >NUL 2>&1
  if %ERRORLEVEL% EQU 0 (
    set python=python
  ) else (
    echo.
    echo Error: no system Python present
    echo.
    echo   Pigweed's bootstrap process requires a local system Python.
    echo   Please install Python on your system, add it to your PATH
    echo   and re-try running bootstrap.
    goto finish
  )
)

set _PW_OLD_CIPD_PACKAGE_FILES=%PW_CIPD_PACKAGE_FILES%
set _PW_OLD_VIRTUALENV_REQUIREMENTS=%PW_VIRTUALENV_REQUIREMENTS%
set _PW_OLD_VIRTUALENV_SETUP_PY_ROOTS=%PW_VIRTUALENV_SETUP_PY_ROOTS%
set _PW_OLD_CARGO_PACKAGE_FILES=%PW_CARGO_PACKAGE_FILES%

set PW_CIPD_PACKAGE_FILES=%PW_ROOT%\pw_env_setup\py\pw_env_setup\cipd_setup\*.json;%PW_CIPD_PACKAGE_FILES%
set PW_VIRTUALENV_REQUIREMENTS=%PW_ROOT%\pw_env_setup\py\pw_env_setup\virtualenv_setup\requirements.txt;%PW_VIRTUALENV_REQUIREMENTS%
set PW_VIRTUALENV_SETUP_PY_ROOTS=%PW_ROOT%;%PW_VIRTUALENV_SETUP_PY_ROOTS%
set PW_CARGO_PACKAGE_FILES=%PW_ROOT%\pw_env_setup\py\pw_env_setup\cargo_setup\packages.txt;%PW_CARGO_PACKAGE_FILES%

set "_pw_start_script=%PW_ROOT%\pw_env_setup\py\pw_env_setup\windows_env_start.py"
set "shell_file=%PW_ROOT%\pw_env_setup\.env_setup.bat"

:: If PW_SKIP_BOOTSTRAP is set, only run the activation stage instead of the
:: complete env_setup.
if "%PW_SKIP_BOOTSTRAP%" == "" (
  :: Without the trailing slash in %PW_ROOT%/, batch combines that token with
  :: the --shell-file argument.
  call "%python%" "%PW_ROOT%\pw_env_setup\py\pw_env_setup\env_setup.py" ^
      --pw-root "%PW_ROOT%/" ^
      --shell-file "%shell_file%"
) else (
  if exist "%shell_file%" (
    call "%python%" "%_pw_start_script%"
  ) else (
    call "%python%" "%_pw_start_script%" --no-shell-file
    goto finish
  )
)

set PW_CIPD_PACKAGE_FILES=%_PW_OLD_CIPD_PACKAGE_FILES%
set PW_VIRTUALENV_REQUIREMENTS=%_PW_OLD_VIRTUALENV_REQUIREMENTS%
set PW_VIRTUALENV_SETUP_PY_ROOTS=%_PW_OLD_VIRTUALENV_SETUP_PY_ROOTS%
set PW_CARGO_PACKAGE_FILES=%_PW_OLD_CARGO_PACKAGE_FILES%

call "%shell_file%"

:finish
::WINDOWS_ONLY
