# JXC Tools

Note that several of these scripts depend on the Python JXC library. If you want to use the bindings from the repo, this is the simplest way (from the repo root):

```
$ virtualenv venv
$ source venv/bin/activate
(venv) $ pip install .
```

Then, to verify that the Python JXC library works correctly:
```
(venv) $ python -c "import jxc; print(jxc.loads('[1, 2, 3]'))"
[1, 2, 3]
```

# amalgamate.py

Generates a header and cpp file, each containing all of JXC's headers and cpp source respectively. Example usage (running from the repo root):

```
python3 tools/amalgamate.py --input . --output ./build/amalgamate --all-dependencies --jxc-cpp --test
```

The `--jxc-cpp` flag indicates that `libjxc_cpp` should be included.

The `--all-dependencies` flag auto-includes third party dependencies. At the time of this writing, that's just `fast_float`, and `unordered_dense` if `--jxc-cpp` is used.

The `--test` flag adds a `premake5.lua` and a main cpp file to the output directory that's just useful for verifying that the amalgamated build works correctly.

# clean.py

Just useful for developing the JXC reference implementation, this script removes build artifacts from the repo. Recommended to run this from the repo root. Add the `--preview` flag to do a dry run to see what it would delete if run without flags.

# generate_docs.py

Static website generator for JXC. Supports parsing railroad diagrams that are themselves defined in JXC (see `/docs/jxc_syntax.jxc`). Note that this script has some dependencies (other than JXC) - see `/tools/jxdocgen/requirements.txt`.

Example documentation generator usage:
```bash
cd /path/to/repo
# create and activate a virtualenv for the documentation generator
$ virtualenv venv
$ source venv/bin/activate
# install the documentation generator's dependencies
(venv) $ pip install -r tools/jxdocgen/requirements.txt
# install the JXC python bindings directly from the repo
(venv) $ pip install .
# build the documentation
(venv) $ python3 tools/generate_docs.py --input ./docs --output ./build/docs
```

For documentation development, you can add the `--dev-server` flag to run a local webserver that automatically rebuilds the documentation when any source file changes.

# generate_pyjxc_stub.py

This script generates a Python interface file (`.pyi` file) for the `_pyjxc` binary module. It's a bit hackish but it gets the job done.

Example usage (running from the repo root):

```
python3 tools/generate_pyjxc_stub.py --module-path ./build/release --output ./bindings/python/_pyjxc/__init__.pyi
```

# version.py

Simple script that reads the version number of the JXC library from `jxc_core.h` and prints it to stdout. Used by some of the build scripts.

# premake5

Windows binary for premake5 (build system generator). On Linux, this is generally available from your distribution's respositories.

Note that premake is only used for developing the JXC reference implementation itself. If you are integrating JXC into a C/C++ project with another build system, you can either use the amalgamated build (recommended), or add JXC's source files and headers directly to your project. If you are integrating JXC into a Python project, you can simply use the included `setup.py` and `pyproject.toml` to build the Python extension directly.

If your Linux distribution does not include premake5 yet, you can download a standalone binary with no dependencies from the [premake website](https://premake.github.io/download), or build it from source.

# re2c (3.0)

Windows binary for re2c (regular expression compiler). On Linux, this is generally available from your distribution's respositories.

If your distribution does not include re2c 3.0 yet, you can easily [build re2c from source](https://github.com/skvadrik/re2c/releases/tag/3.0). Note that JXC has not been tested with older versions of re2c. It might work, but it's not tested or supported.
