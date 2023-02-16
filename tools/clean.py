#!/usr/bin/env python3
import os
import glob
import shutil
import argparse

# file globs to remove when --clean is used
CLEAN_FILES = [
    "Makefile",
    "*.make",
    "obj/",
    "build/",
    "dist/",
    "_pyjxc.*.so",
    "jxc.egg-info/",
    "*.pyc",
    "bindings/python/jxc/__pycache__/",
    "tools/jxdocgen/__pycache__/",
]

CLEAN_FILES_MSVC = [
    ".vs",
    "*.sln",
    "*.vcxproj",
    "*.vcxproj.user",
    "*.vcxproj.filters",
]


def all_paths(path_globs):
    for path_glob in path_globs:
        for path in glob.glob(path_glob, recursive=True):
            yield (path_glob, path)


def clean(globs, preview=False):
    """
    Deletes all matching globs. If preview is True, prints the paths instead of deleting them.
    """
    if not any([ os.path.exists(path) for _g, path in all_paths(globs) ]):
        print(f"No paths found to clean. Searched {globs!r}")
        return

    for matched_glob, delpath in all_paths(globs):
        pathtype = None
        if os.path.isdir(delpath):
            pathtype = "dir"
        elif os.path.isfile(delpath):
            pathtype = "file"

        if preview:
            print(f"[Preview] Delete {pathtype} {delpath!r} (matched glob {matched_glob!r})")
            continue

        if pathtype == 'dir':
            print(f"shutil.rmtree({os.path.abspath(delpath)!r})")
            shutil.rmtree(delpath)
        elif pathtype == 'file':
            print(f"os.unlink({os.path.abspath(delpath)!r})")
            os.unlink(delpath)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-path", '-b', type=str, default='.', help="Base search path (generally the repository root directory)")
    parser.add_argument("--msvc", '-m', action='store_true', help="Include MSVC project files")
    parser.add_argument("--preview", '-p', action='store_true', help="Shows actions instead of performing them")
    args = parser.parse_args()

    # make all globs relative to the base path
    clean_glob_list = [ os.path.join(args.base_path, g) for g in CLEAN_FILES ]

    if args.msvc:
        clean_glob_list += [ os.path.join(args.base_path, g) for g in CLEAN_FILES_MSVC ]

    clean(clean_glob_list, preview=args.preview)
