import os
import sys

_BUILD_MODE = 'debug'

_REPO_ROOT = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..', '..', '..')

try:
    import jxc
except ImportError:
    sys.path.insert(0, os.path.join(_REPO_ROOT, 'build', _BUILD_MODE))
    sys.path.insert(1, os.path.join(_REPO_ROOT, 'bindings', 'python'))

import unittest
from pyjxc_tests.test_values import *


if __name__ == "__main__":
    unittest.main()
