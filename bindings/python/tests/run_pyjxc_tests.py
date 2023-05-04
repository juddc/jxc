#!/usr/bin/env python3
import _pyjxc
print(f'[INFO] _pyjxc loaded from {_pyjxc.__file__!r}')

import jxc
print(f'[INFO] jxc loaded from {jxc.__file__!r}')

import unittest
from pyjxc_tests.test_values import *
from pyjxc_tests.test_classes import *


if __name__ == "__main__":
    unittest.main()
