# Using JXC in C++ Projects

There are several ways to include JXC as a dependency in your project.

## Meson
If you're already using [Meson](https://mesonbuild.com/), you can include the JXC repository as a subproject. Simply clone the repository into your `/subprojects` directory and use `subproject('jxc')` in your `meson.build` script.


### Meson Example
Directory structure:
```
/subprojects/jxc
/meson.build
/main.cpp
```
`meson.build`:
```meson
project('your_application', 'cpp', default_options: ['cpp_std=c++20'])
jxc_proj = subproject('jxc', required: true)
executable('your_application',
  'your_application_main.cpp',
  dependencies: [
    jxc_proj.get_variable('libjxc_dep'),
    jxc_proj.get_variable('libjxc_cpp_dep'),  # Optional C++ helper library
  ],
)
```
`your_application_main.cpp`:
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
