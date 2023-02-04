import os
import sys
import typing
import subprocess
from setuptools import setup

# see if there is a system pybind11 install available
try:
    import pybind11 as _
except ImportError:
    # no system pybind11 install available, fall back on the bundled copy
    sys.path.insert(0, "./subprojects/pybind11")

from pybind11.setup_helpers import Pybind11Extension, build_ext


jxc_version = subprocess.check_output([sys.executable, './tools/version.py']).decode('utf-8').strip()
build_root_dir = os.path.normpath(os.path.dirname(os.path.abspath(__file__)))


def parse_version(ver: str) -> typing.Tuple[int, int, typing.Union[int, str]]:
    parts = list(ver.split('.'))
    if len(parts) == 1 and parts[0].isnumeric():
        return (int(parts[0]), 0, 0)
    elif len(parts) in (2, 3) and parts[0].isnumeric() and parts[1].isnumeric():
        patch_ver = 0
        if len(parts) > 2:
            patch_ver = int(parts[2]) if parts[2].isnumeric() else parts[2]
        return (int(parts[0]), int(parts[1]), patch_ver)
    raise ValueError(f'Value {ver!r} is not a version number')


def executable_exists(path):
    return os.path.exists(path) and os.access(path, os.X_OK)


def get_re2c_version(re2c_path: str) -> str:
    assert executable_exists(re2c_path)
    try:
        re2c_version = subprocess.check_output([re2c_path, '--version']).decode('utf-8').strip()
    except subprocess.CalledProcessError:
        return ''
    if re2c_version.startswith('re2c'):
        re2c_version = re2c_version[4:].strip()
    if re2c_version.endswith('(debug)'):
        re2c_version = re2c_version[:-7].strip()
    return re2c_version


def find_re2c() -> typing.Optional[str]:
    re2c_path = None
    if os.name == 'nt':
        re2c_path = os.path.join(build_root_dir, 'tools', 're2c.exe')
    else:
        # first, see if there's a re2c binary in the tools directory
        re2c_path = os.path.join(build_root_dir, 'tools', 're2c')

    # we have a bundled re2c executable
    if executable_exists(re2c_path):
        return re2c_path

    # if that doesn't exist, ask the shell if we have one available
    try:
        if os.name == 'nt':
            search_cmd = ['where', 're2c.exe']
        else:
            search_cmd = ['which', 're2c']
        re2c_path = subprocess.check_output(search_cmd).decode('utf-8').strip()
        if executable_exists(re2c_path):
            return re2c_path
    except subprocess.CalledProcessError:
        pass

    return None


def run_re2c(re2c_path: str, input_file: str):
    assert os.path.exists(input_file)
    input_file = os.path.relpath(input_file, start=build_root_dir)
    assert os.path.exists(input_file)

    args = [re2c_path, '-W', '--verbose', '--utf8', '--input-encoding', 'utf8']
    if os.name == 'nt':
        args += ['--location-format', 'msvc']
    else:
        args += ['--location-format', 'gnu']
    
    re2c_args = args + ['-o', f'{input_file}.cpp', input_file]
    print(' '.join(re2c_args))
    subprocess.check_call(re2c_args)


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
        generated_lexer_path = os.path.join(build_root_dir, 'jxc', 'src', 'jxc_lexer_gen.re.cpp')

        re2c_path = find_re2c()
        re2c_ver = get_re2c_version(re2c_path) if re2c_path else ''
        if re2c_path and parse_version(re2c_ver)[0] >= 3:
            run_re2c(re2c_path, os.path.join(build_root_dir, 'jxc', 'src', 'jxc_lexer_gen.re'))
        elif re2c_path:
            print(f'Warning: re2c version {re2c_ver} found, but JXC requires re2c >= 3.0. Using pre-generated lexer source file.')
        elif os.path.exists(generated_lexer_path):
            print('Warning: re2c not found, but generated lexer source already exists. Using pre-generated lexer source file.')
        else:
            raise FileNotFoundError(f'{generated_lexer_path!r} not found, and re2c could not be found to generate it.')

        assert os.path.exists(generated_lexer_path)
        if extra_args := self.per_platform_compile_args.get(ext.name, None):
            if args := extra_args.get(self.compiler.compiler_type, None):
                if not ext.extra_compile_args:
                    ext.extra_compile_args = []
                ext.extra_compile_args += args
        return super().build_extension(ext)


# verify that jxc_version is a valid version number
try:
    parse_version(jxc_version)
except ValueError as e:
    raise Exception(f'Invalid JXC version {jxc_version!r}: {e}')


setup(
    name="jxc",
    version=jxc_version,
    #long_description="",
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
