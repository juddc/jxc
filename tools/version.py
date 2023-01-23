#!/usr/bin/env python3
import os
import sys


def find_defines(filename):
    """
    Generator that returns all #define lines in a file.
    NB. This does not return valid output if you care about function-style macros, preprocessor conditionals, or multi-line macros.
    """
    preproc_token = '#'
    def_token = 'define'
    with open(filename) as fp:
        for line in fp:
            line = line.strip()
            if not line.startswith(preproc_token):
                continue
            line = line[len(preproc_token):].strip()
            if not line.startswith(def_token):
                continue
            line = line[len(def_token):].strip()
            name_end_idx = line.find(' ')
            yield (line[:name_end_idx], line[name_end_idx:].strip()) if (name_end_idx >= 0) else (line, '')


if __name__ == "__main__":
    version = {
        'JXC_VERSION_MAJOR': None,
        'JXC_VERSION_MINOR': None,
        'JXC_VERSION_PATCH': None,
    }

    repo_root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
    jxc_core_path = os.path.join(repo_root, 'jxc', 'include', 'jxc', 'jxc_core.h')
    if not os.path.exists(jxc_core_path):
        print(f'Error: jxc_core.h not found. Tried {jxc_core_path!r}', file=sys.stderr)
        sys.exit(1)

    for def_name, def_value in find_defines(jxc_core_path):
        if def_name in version:
            version[def_name] = def_value
        if all(v is not None for v in version.values()):
            break

    print(".".join(str(v) for v in (
        version['JXC_VERSION_MAJOR'],
        version['JXC_VERSION_MINOR'],
        version['JXC_VERSION_PATCH'],
    )))
