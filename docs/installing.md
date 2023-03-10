# Using JXC in C++ Projects

There are several ways to include JXC as a dependency in your project.

## Amalgamated Build
The simplest way to include JXC into your project is using the amalagamated build, which combines all of JXC's header files into a single header, and all of JXC's C++ source files into one cpp file. You can download the latest version of an amalgamated build from Github's releases page, or you can compile your own using the script in the `tools` directory. Compiling your own is useful if you want more control over dependencies. If you're already using `fast_float` and/or `unordered_dense` (the only two dependencies that JXC uses), you can omit these from the amalgamated build. You can also choose to include or exclude [libjxc_cpp](jxc_cpp_library.md).

Once you have obtained or generated an amalgamated build, you can add `jxc.h` and `jxc.cpp` to your project the same way any other source files would be added.

Note that JXC does not include an amalgamated build that is header-only, because doing so would slow down build times.

## Meson
If you're already using [Meson](https://mesonbuild.com/), you can include the JXC repository as a subproject. Simply clone the repository into your `/subprojects` directory and use `subproject('jxc')` in your `meson.build` script. The Meson `libjxc` dependency is named `libjxc_dep`, and the Meson `libjxc_cpp` dependency is named `libjxc_cpp_dep`.

### Meson Example
Directory structure:
```
/subprojects/jxc
/meson.build
/main.cpp
```

`meson.build`
```meson
project('your_application', 'cpp', default_options: ['cpp_std=c++20'])
jxc_proj = subproject('jxc', required: true)
executable('your_application',
  'your_application_main.cpp',
  dependencies: [
    jxc_proj.get_variable('libjxc_dep'),
    #jxc_proj.get_variable('libjxc_cpp_dep'),  # Optional C++ helper library
  ],
)
```

`your_application_main.cpp`
```cpp
#include <iostream>
#include "jxc/jxc.h"
int main(int argc, char** argv)
{
    jxc::StringOutputBuffer buf;
    jxc::Serializer doc(&buf);
    doc.array_begin().value_int(123).array_end();
    doc.flush();
    std::cout << buf.to_string() << '\n';
    return 0;
}
```

Usage:
```shell
$ mkdir subprojects
$ cd subprojects
$ git clone https://github.com/juddc/jxc
$ cd ..
$ meson setup build
$ ninja -C build
```

# Using JXC in Python Projects

JXC has first-class Python bindings that work out of the box. See the [JXC Python Library](jxc_python_library.md) documentation page for how to get started.

The fastest way to start using JXC is by installing it with **pip**: simply run `pip install jxc` to install the Python JXC package.

If you want to work on the JXC library itself, you can also install the Python bindings by running `pip install .` from the repository root directory.

For either installation method, if you activate a virtualenv first, it will be installed into your virtual environment.
