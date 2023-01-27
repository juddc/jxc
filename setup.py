import os
import sys
import subprocess
from setuptools import setup

# see if there is a system pybind11 install available
try:
    import pybind11 as _
except ImportError:
    # no system pybind11 install available, fall back on the bundled copy
    sys.path.insert(0, "./subprojects/pybind11")

from pybind11.setup_helpers import Pybind11Extension, build_ext

__version__ = subprocess.check_output(['python', './tools/version.py']).decode('utf-8').strip()


repo_root_dir = os.path.normpath(os.path.dirname(os.path.abspath(__file__)))


def find_re2c() -> str | None:
    re2c_path = None
    if os.name == 'nt':
        re2c_path = os.path.join(repo_root_dir, 'tools', 're2c.exe')
    else:
        # first, see if there's a re2c binary in the tools directory
        re2c_path = os.path.join(repo_root_dir, 'tools', 're2c')
        if os.path.exists(re2c_path) and os.access(re2c_path, os.X_OK):
            return re2c_path
        # if that doesn't exist, ask the shell if we have one available
        try:
            re2c_path = subprocess.check_output(['which', 're2c']).decode('utf-8').strip()
        except subprocess.CalledProcessError:
            return None
    return re2c_path if os.path.exists(re2c_path) else None


def run_re2c(re2c_path: str, input_file: str):
    assert os.path.exists(input_file)
    input_file = os.path.relpath(input_file, start=repo_root_dir)
    assert os.path.exists(input_file)

    args = [re2c_path, '-W', '--verbose', '--utf8', '--input-encoding', 'utf8']
    if os.name == 'nt':
        args += ['--location-format', 'msvc']
    else:
        args += ['--location-format', 'gnu']
    subprocess.check_call(args + ['-o', f'{input_file}.cpp', input_file])


ext_modules = [
    Pybind11Extension(
        "_pyjxc",
        cxx_std=17,
        sources=[
            # JXC core library
            "jxc/src/jxc_core.cpp",
            "jxc/src/jxc_lexer.cpp",
            "jxc/src/jxc_lexer_gen.re.cpp",
            "jxc/src/jxc_parser.cpp",
            "jxc/src/jxc_serializer.cpp",
            "jxc/src/jxc_string.cpp",
            "jxc/src/jxc_util.cpp",

            # JXC Python bindings
            "bindings/python/src/jxc_pyparser.cpp",
            "bindings/python/src/jxc_pyserializer.cpp",
            "bindings/python/src/jxc_python.cpp",
        ],
        include_dirs=[
            "jxc/include",
            "bindings/python/src",
            "subprojects/fast_float/include",
            "subprojects/unordered_dense/include",
        ],
        define_macros = [
            ('JXC_DEBUG', 0),
            ('JXC_ENABLE_RELEASE_ASSERTS', 1),
            ('JXC_ENABLE_DEBUG_ASSERTS', 1),  # NB. These are only active when JXC_DEBUG == 1
            ('JXC_ENABLE_DEBUG_ALLOCATOR', 0),
            ('JXC_ENABLE_JUMP_BLOCK_PROFILER', 0),
        ],
    ),
]


class build_ext_jxc(build_ext):
    per_platform_compile_args = {
        '_pyjxc': {
            'unix': ['-Wno-reorder'],
        }
    }

    def build_extension(self, ext):
        re2c_path = find_re2c()
        if re2c_path is not None:
            run_re2c(re2c_path, os.path.join(repo_root_dir, 'jxc', 'src', 'jxc_lexer_gen.re'))
        assert os.path.exists(os.path.join(repo_root_dir, 'jxc', 'src', 'jxc_lexer_gen.re.cpp'))
        if extra_args := self.per_platform_compile_args.get(ext.name, None):
            if args := extra_args.get(self.compiler.compiler_type, None):
                if not ext.extra_compile_args:
                    ext.extra_compile_args = []
                ext.extra_compile_args += args
        return super().build_extension(ext)


setup(
    name="jxc",
    version=__version__,
    author="Judd Cohen",
    author_email="jcohen@juddnet.com",
    url="https://github.com/juddc/jxc",
    description="JXC file format parsing and serialization library",
    long_description="",
    cmdclass={'build_ext': build_ext_jxc},
    ext_modules=ext_modules,
    packages=['_pyjxc', 'jxc'],
    package_dir={
        '_pyjxc': './bindings/python/_pyjxc',
        'jxc': './bindings/python/jxc',
    },
    package_data={
        '_pyjxc': ['py.typed', '__init__.pyi'],
        'jxc': ['py.typed'],
    },
    zip_safe=False,
    python_requires=">=3.7",
)
