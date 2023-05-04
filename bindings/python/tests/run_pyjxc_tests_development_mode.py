#!/usr/bin/env python3
import os
import sys
import subprocess

BUILD_MODE = 'debug'
#BUILD_MODE = 'release'

REPO_ROOT = os.path.realpath(os.path.join(os.path.abspath(os.path.dirname(__file__)), '..', '..', '..'))

extra_paths = None

# if we can't import _pyjxc, add paths to PYTHONPATH so we can import the premake-built one
try:
    import _pyjxc
except ImportError:
    extra_paths = [
        os.path.join(REPO_ROOT, 'build', BUILD_MODE),
        os.path.join(REPO_ROOT, 'bindings', 'python'),
    ]

if extra_paths:
    new_env = { k: v for k, v in os.environ.items() }
    new_env['PYTHONPATH'] = ':'.join(extra_paths)
else:
    new_env = None

sys.exit(subprocess.call(
    [sys.executable, os.path.join(REPO_ROOT, 'bindings', 'python', 'tests', 'run_pyjxc_tests.py')],
    env=new_env))
