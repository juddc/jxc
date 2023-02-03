#!/usr/bin/env python3
import os
import sys
import glob
import zipfile
import argparse
import subprocess


INCLUDE_GLOBS = [
    # documentation
    'README.md',
    'LICENSE.MIT',
    'docs/*.md',
    'docs/jxc_syntax.jxc',

    # editor syntax highlighting packages
    'contrib/*/*',
    'contrib/*.md',
]


def append_filename(file_path: str, value: str) -> str:
    """
    Adds a string to the end of a filename, inserting it just _before_ the file's extension.
    Ex. append_filename('/a/b/c/release.zip', '_1.0') == '/a/b/c/release_1.0.zip'
    """
    file_dir, file_name = os.path.split(file_path)
    if file_name.startswith('.'):
        return os.path.join(file_dir, f'.{file_name[1:]}{value}')
    elif file_name.endswith('.'):
        return os.path.join(file_dir, f'{file_name[:-1]}{value}.')
    elif '.' not in file_name:
        return os.path.join(file_dir, f'{file_name}{value}')

    parts = file_name.split('.')
    assert len(parts) >= 2
    ext = parts[-1]
    parts = parts[:-1]
    parts[-1] += str(value)
    return os.path.join(file_dir, '.'.join(parts + [ext]))


def make_release(jxc_version: str, repo_root: str, output_path: str, amal_core_path: str, amal_cpp_path: str):
    output_path = append_filename(output_path, f'_{jxc_version}')
    zip_base_dir = f'jxc_{jxc_version}'

    with zipfile.ZipFile(output_path, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for inc_glob in INCLUDE_GLOBS:
            print(repr(inc_glob))
            for path in glob.glob(os.path.join(repo_root, inc_glob)):
                file_rel_path = os.path.join(zip_base_dir, os.path.relpath(path, repo_root))
                zf.write(path, file_rel_path)
                print('\t', repr(path), ' --> ', repr(file_rel_path))

        print('amalgamated_core')
        for filename in os.listdir(amal_core_path):
            file_path = os.path.join(amal_core_path, filename)
            file_zip_path = os.path.join(zip_base_dir, 'amalgamated_core_only', 'jxc', filename)
            print('\t', repr(file_path), ' --> ', repr(file_zip_path))
            zf.write(file_path, file_zip_path)

        print('amalgamated_cpp')
        for filename in os.listdir(amal_cpp_path):
            file_path = os.path.join(amal_cpp_path, filename)
            file_zip_path = os.path.join(zip_base_dir, 'amalgamated_core_and_cpp_lib', 'jxc', filename)
            print('\t', repr(file_path), ' --> ', repr(file_zip_path))
            zf.write(file_path, file_zip_path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=str, help='Repo root directory')
    parser.add_argument('--amalgamated-core', type=str, help='Path to the amalgamated core build')
    parser.add_argument('--amalgamated-cpp', type=str, help='Path to the amalgamated cpp build')
    parser.add_argument('--output', type=str, help='Output zip file path')
    args = parser.parse_args()

    repo_root: str = os.path.abspath(args.source)
    if not os.path.exists(repo_root):
        raise FileNotFoundError(repo_root)

    amal_core: str = os.path.abspath(args.amalgamated_core)
    if not os.path.exists(amal_core):
        raise FileNotFoundError(amal_core)

    amal_cpp: str = os.path.abspath(args.amalgamated_cpp)
    if not os.path.exists(amal_cpp):
        raise FileNotFoundError(amal_cpp)

    output_path: str = os.path.abspath(args.output)
    assert not output_path.endswith('/')
    output_path_parent = os.path.dirname(output_path)
    if not os.path.exists(output_path_parent):
        output_path_parent_parent = os.path.dirname(output_path_parent)
        if os.path.exists(output_path_parent_parent) and os.path.isdir(output_path_parent_parent):
            os.makedirs(output_path_parent, exist_ok=True)
        else:
            raise ValueError(f"Output path parent directory {output_path_parent_parent} does not exist")

    # read the release version from the repo
    jxc_version = subprocess.check_output([ sys.executable, os.path.join(repo_root, 'tools', 'version.py') ]).strip().decode('utf-8')
    assert len(jxc_version) > 0 and len(jxc_version.split('.')) == 3

    make_release(jxc_version, repo_root, output_path, amal_core, amal_cpp)
