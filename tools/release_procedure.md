# Release Procedure

## Building

### Version Number
* Set the version number in `/jxc/include/jxc/jxc_core.h`
* Set the version number in `/pyproject.toml`

### Compile Native Binaries
* Build debug binaries.
    * `premake5 gmake2 && make config=debug_linux64 -j16`
* Build release binaries.
    * `premake5 gmake2 && make config=release_linux64 -j16`
        - NB. This command auto-updates the Python bindings interface file, which needs to be done _before_ building the Python wheels. _Technically_, this is only required if the `_pyjxc` API or docstrings have any changes, but it's best practice to just make sure it always gets run for every release.

* Build the Python wheel.
    * Note that you're going to have to enter and then immediately exit a Python virtualenv. Using `tmux` or `screen` is an easy way to do this quickly without needing to open multiple terminals.
    * If you don't have one already, create a Python virtualenv:
        - `virtualenv venv`
        - `source ./venv/bin/activate`
    * Make sure the `build` package is installed and up to date:
        - `pip install --upgrade build`
    * Build the Python wheel distribution:
        - `python3 -m build`
    * Exit the Python virtualenv so that it doesn't affect later Python module testing.

### Amalgamated Builds
* `./tools/amalgamate.py --all-dependencies --output ./build/amalgamated_core`
* `./tools/amalgamate.py --all-dependencies --jxc-cpp --output ./build/amalgamated_cpp`

## Run Tests
Run all tests and examples using the debug binaries (which have debug assertions enabled, so they can catch more issues).
* `./build/debug/example_*`
* `./build/debug/test_jxc`

Run all tests, examples, and the benchmark using the release binaries.
* `./build/release/example_*`
* `./build/release/test_jxc`
* `./build/release/benchmark_jxc --input ./tests/data/**/*.jxc -n 64`
    - Make sure benchmark numbers have not gone up significantly.
    - On my system (AMD Ryzen 9 3900X), I get about 17ms for parser-only and about 52ms for the value parser.

Run Python binding tests using the debug binaries.
* `python ./bindings/python/tests/run_pyjxc_tests.py`
    - Note that this script auto-imports the premake-built debug binaries. Don't use a virtualenv for this.

## Verification
Verify that the project works correctly for all common use cases. For this, create and enter a testing directory outside the repository, and make sure no virtualenv is activated.

### Amalgamated Core (*excludes* `libjxc_cpp`)
* `mkdir amal_core`
* `cd amal_core`
* Create `meson.build`
    ```meson
    project('amal_core', 'cpp', default_options: ['cpp_std=c++20'])
    executable('amal_core', sources: ['main.cpp', 'jxc/jxc.cpp'])
    ```
* Create `main.cpp`
    ```cpp
    #include <iostream>
    #include "jxc/jxc.h"
    int main(int argc, char** argv)
    {
        jxc::StringOutputBuffer buf;
        jxc::Serializer doc(&buf);
        doc.array_begin().value_int(123).array_end().flush();
        std::cout << buf.to_string() << '\n';
        return 0;
    }
    ```
* `mkdir jxc`
* Copy `/build/amalgamated_core/jxc.h` from the repo to `./jxc/jxc.h`
* Copy `/build/amalgamated_core/jxc.cpp` from the repo to `./jxc/jxc.cpp`
* `meson setup build`
* `ninja -C build`
* `./build/amal_core`
* `cd ..`

### Amalgamated C++ (includes `libjxc_cpp`)
* `mkdir amal_cpp`
* `cd amal_cpp`
* Create `meson.build`
    ```meson
    project('amal_cpp', 'cpp', default_options: ['cpp_std=c++20'])
    executable('amal_cpp', sources: ['main.cpp', 'jxc/jxc.cpp'])
    ```
* Create `main.cpp`
    ```cpp
    #include <iostream>
    #include "jxc/jxc.h"
    int main(int argc, char** argv)
    {
        jxc::Value val = jxc::default_object;
        val["a"] = jxc::annotated_array("vec3", { 0.2, -0.5, 4.4 });
        val["b"] = nullptr;
        val["c"] = "abc";
        std::cout << val.to_string() << '\n';
        return 0;
    }
    ```
* `mkdir jxc`
* Copy `/path/to/jxc_repo/build/amalgamated_cpp/jxc.h` from the repo to `./jxc/jxc.h`
* Copy `/path/to/jxc_repo/build/amalgamated_cpp/jxc.cpp` from the repo to `./jxc/jxc.cpp`
* `meson setup build`
* `ninja -C build`
* `./build/amal_cpp`
* `cd ..`

### Meson Subproject
* `mkdir meson_subproj`
* `cd meson_subproj`
* `mkdir subprojects`
* `ln -s /path/to/jxc_repo subprojects/jxc`
* Create `meson.build`
    ```meson
    project('meson_subproj', 'cpp', default_options: ['cpp_std=c++20'])
    executable('meson_subproj', 'main.cpp',
      dependencies: [subproject('jxc', required: true).get_variable('libjxc_dep')]
    )
    ```
* Create `main.cpp`
    ```cpp
    #include <iostream>
    #include "jxc/jxc.h"
    int main(int argc, char** argv)
    {
        jxc::StringOutputBuffer buf;
        jxc::Serializer doc(&buf);
        doc.array_begin().value_int(123).array_end().flush();
        std::cout << buf.to_string() << '\n';
        return 0;
    }
    ```
* `meson setup build`
* `ninja -C build`
* `./build/meson_subproj`
* `cd ..`

### Python Wheel
* `mkdir py_wheel`
* `virtualenv venv`
* `source venv/bin/activate`
* `pip install /path/to/jxc_repo/dist/jxc-*.whl` (make sure this is the `whl` file you created earlier)
* Verify that the `jxc` and `_pyjxc` modules are usable and loaded from the virtualenv
    * `python3 -c 'import jxc; print(jxc.__file__)'`
    * `python3 -c 'import _pyjxc; print(_pyjxc.__file__)'`
* Verify that basic parsing works correctly
    * `python3 -c 'import jxc; print(jxc.loads("[1,2,3]"))'`

# Deployment

## Repository
Make sure the repository is up to date with all changes needed for the release.
* `git push`
* Create a git tag for the release, eg. `0.8.3`
* Push the tag as well

## Upload to PyPi
* `cd /path/to/jxc_repo`
* `source venv/bin/activate`
    - Use the same virtual environment used to build the Python wheels above
* Use `twine` to upload packages to PyPi. Note that for the repository argument, you can replace `pypi` with `testpypi` to upload to the testing repository instead of the primary one.
    - `pip install --upgrade twine`
    - For twine to work, you'll need to put your API keys in `~/.pypirc`, like so:
        ```
        [pypi]
        username = __token__
        password = pypi-MAIN_REPO_API_TOKEN_HERE
        [testpypi]
        username = __token__
        password = pypi-TEST_REPO_API_TOKEN_HERE
        ```
* `twine upload --repository pypi dist/jxc-VERSION.tar.gz`
    - This just uploads the source distribution, which is all you can do on Linux. On Windows and MacOS, also upload the `.whl` binary releases.

## Upload to GitHub
* `cd /path/to/jxc_repo`
* `python3 ./tools/./tools/make_release_archive.py --source . --amalgamated-core ./build/amalgamated_core --amalgamated-cpp ./build/amalgamated_cpp --output "./build/jxc_release.zip`
    - Use the **tested** amalgamated builds from above.
    - The `make_release_archive` script automatically appends the JXC version from `jxc_core.h` to the end of the output filename.
* Upload the generated release zip archive to GitHub.
    - Reference the created git tag and name the release `v` plus the version number, eg. `"v0.8.3"`.

## Update the Documentation Website
* SSH into the documentation VPS
* `apt update && apt upgrade`
* Reboot if there was a kernel upgrade
* Run the `update_docs.py` script in the home directory, which will automatically do a `git pull`, regenerate the documentation, back up the old docs to allow for a rollback, and swap in the new documentation data.

