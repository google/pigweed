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

:: If PW_CHECKOUT_ROOT is set, use it. Users should not set this variable.
:: It's used because when one batch script invokes another the Powershell magic
:: below doesn't work. To reinforce that users should not be using
:: PW_CHECKOUT_ROOT, it is cleared here after it is used, and other pw tools
:: will complain if they see that variable set.
:: TODO(mohrr) find out a way to do this without PW_CHECKOUT_ROOT.
if "%PW_CHECKOUT_ROOT%"=="" (
  :: Calls a Powershell script that determines the correct PW_ROOT directory and
  :: exports it as an environment variable.
  for /F "usebackq tokens=1" %%i in (`powershell %%~dp0..\..\pw_env_setup\env_setup.ps1`) do set PW_ROOT=%%i
) ELSE (
  set PW_ROOT=%PW_CHECKOUT_ROOT%
  set PW_CHECKOUT_ROOT=
)

:: Allow forcing a specifc Python version through the environment variable
:: PW_BOOTSTRAP_PYTHON. Otherwise, use the system Python if one exists.
if not "%PW_BOOTSTRAP_PYTHON%" == "" (
  set python="%PW_BOOTSTRAP_PYTHON%"
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

call %python% %PW_ROOT%\pw_env_setup\py\pw_env_setup\windows_env_start.py

set shell_file="%PW_ROOT%\pw_env_setup\.env_setup.bat"

if not exist %shell_file% (
  call %python% %PW_ROOT%\pw_env_setup\py\pw_env_setup\env_setup.py^
    --pw-root %PW_ROOT%^
    --shell-file %shell_file%
)

call %shell_file%

:finish
