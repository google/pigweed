# Python Distributables
Setup and usage instructions for Pigweed Python distributables.

## Prerequisites
Python distributables require Python 3.7 or later.

## Setup
Run the included setup script found inside the unzipped directory.

Linux / MacOS:
```bash
setup.sh
```

Windows:
```
setup.bat
```

The setup script will create a virtual environment called `python-venv`.

### Usage
Once setup is complete, the Python tools can be invoked as runnable modules:

Linux/MacOS:
```bash
python-venv/bin/python -m MODULE_NAME [OPTIONS]
```

Windows:
```
python-venv\Scripts\python -m MODULE_NAME [OPTIONS]
```
