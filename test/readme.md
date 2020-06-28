# Test

Test suite for the command interpreter and the commands.

## Introduction

There is a test suite that tests the command interpreter by sending commands and verifying the responses. In addition to the Arduino board (which you presumably have), you need Python on your PC, because the test is written in Python's [unittest](https://docs.python.org/3/library/unittest.html).
## Prerequisites

The test suite assumes you have 

- an Arduino board (typically ATmega2560)
- flashed with [isa6502prog](../examples/isa6502prog]
- hooked the Arduino to your PC
- 'COM11` as the virtual port for your Arduino  
  (if yours has a different name, change it in [cmd_test.py](cmd_test.py) line 12)
- [Python 3](https://www.python.org/downloads/) installed on your system

## Run the test

To run the test take these steps

- Ensure the [prerequisites](#prerequisites) are satisfied.
- Open a command window (`cmd.exe`) in the test directory, i.e. in `isa6502\test`.
- Run `setup.bat` to _create_ a virtual environment for Python.
  > Note, virtual environments are a built-in feature of Python, see [docs.python.org](https://docs.python.org/3/library/venv.html) for details. They ensures that any changes to the Python environment, like installing packages, is confined to the local (virtual) environment, and does not effect the global Python system environment. A virtual environment is a full copy, including a copy of the Python interpreter and freshly downloaded packages. Note, that `setup` creates the `env` directory that holds the virtual environment. This directory caches the Python interpreter and packages. You can simply delete the `env` directory at any time, and recreate it by running `setup` again. Note also that it does not harm to rerun `setup` when `env` exists; it will check if latest-greatest is present in `env`, and if not, download it.
- Do _not_ close the command window; we need the _activated_ virtual environment.
  > The `setup` also _activates_ the virtual environment. You can see that from the prompt, it is prefixed with `(venv)`. If you want to restart testing at a later stage, you can open a new command window and run `setup` to (got to latest-greates and) activate the virtual environment, or you can activate manually by running `env\Scripts\activate.bat`.
- Execute the test by running `run.bat`.
  > By default this runs all tests like `Test_help`, `Test_echo`, etc. Edit the batch file to remove some test to speed up testing (it is surprisingly slow). 

## Test result

The output should be something like

```text
...................
----------------------------------------------------------------------
Ran 19 tests in 22.702s

OK
```

Every dot on the first is a successful test. Below the dashed line (`-----`) the test summary is shown. If a test fails, an `F` appears instead of a `.`, and a detailed report will be inserted above the summary. For example, below I deliberately broke a test for the `help` command (by testing for a capital `T` in 'too many arguments`).

```text
........F..........
======================================================================
FAIL: test_errargs (__main__.Test_help)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "cmd_test.py", line 151, in test_errargs
    self.assertIn("ERROR: Too many arguments\r\n",r)
AssertionError: 'ERROR: Too many arguments\r\n' not found in 'ERROR: too many arguments\r\n'

----------------------------------------------------------------------
Ran 19 tests in 22.701s

FAILED (failures=1)
```


(end of doc)
