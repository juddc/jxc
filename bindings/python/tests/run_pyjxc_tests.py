import os
import sys

_BUILD_MODE = 'debug'
#_BUILD_MODE = 'release'

_REPO_ROOT = os.path.realpath(os.path.join(os.path.abspath(os.path.dirname(__file__)), '..', '..', '..'))

try:
    import _pyjxc
except ImportError:
    sys.path.insert(0, os.path.join(_REPO_ROOT, 'build', _BUILD_MODE))
    sys.path.insert(1, os.path.join(_REPO_ROOT, 'bindings', 'python'))

import _pyjxc
assert isinstance(_pyjxc.__version__, str)

import jxc

print(f'[INFO] _pyjxc loaded from {_pyjxc.__file__!r}')
print(f'[INFO] jxc loaded from {jxc.__file__!r}')

import unittest
from pyjxc_tests.test_values import *


if __name__ == "__main__":
    unittest.main()
