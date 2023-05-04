#!/usr/bin/env python3
"""
This tool generates the zip files that are uploaded to each GitHub release.
The archive contains amalgamated build variants, documentation, and editor extensions for syntax highlighting.
"""
import os
import sys
import glob
import shutil
import typing
import fnmatch
import zipfile
import argparse
import subprocess
import dataclasses


@dataclasses.dataclass
class ArchivePackage:
    src_dir: str
    zip_base_path: str
    desc: typing.Optional[str] = None
    src_patterns: typing.Optional[typing.List[str]] = None
    compress_type: typing.Optional[int] = None
    compress_level: typing.Optional[int] = None


def make_archive_package_list(repo_root: str, amal_core_path: str, amal_cpp_path: str) -> typing.List[ArchivePackage]:
    return [
        # top-level files
        ArchivePackage(
            src_dir=repo_root,
            zip_base_path='',
            src_patterns=['README.md', 'LICENSE.MIT'],
        ),

        # documentation
        ArchivePackage(
            src_dir=os.path.join(repo_root, 'docs'),
            zip_base_path='docs',
            desc='JXC documentation',
        ),

        # syntax highlighting packages
        ArchivePackage(
            src_dir=os.path.join(repo_root, 'contrib'),
            zip_base_path='contrib',
            desc='JXC syntax highlighting extensions',
        ),

        # meson subproject
        ArchivePackage(
            src_dir=repo_root,
            zip_base_path='jxc',
            desc='JXC Meson subproject',
            src_patterns=[
                'meson.build',
                'jxc/include/*/*',
                'jxc/src/*',
                'jxc_cpp/include/*/*',
                'jxc_cpp/src/*',
                'subprojects/fast_float/include/*.h',
                'subprojects/fast_float/*',
                'subprojects/unordered_dense/include/*.h',
                'subprojects/unordered_dense/*',
                'tools/version.py',
            ],
        ),

        # amalgamated build (libjxc only)
        ArchivePackage(
            src_dir=amal_core_path,
            zip_base_path=os.path.join('amalgamated_core_only', 'jxc'),
            desc='Amalgamated JXC source release, with only the core libjxc library',
        ),

        # amalgamated build (libjxc + libjxc_cpp)
        ArchivePackage(
            src_dir=amal_cpp_path,
            zip_base_path=os.path.join('amalgamated_core_and_cpp_lib', 'jxc'),
            desc='Amalgamated JXC source release, with both the core libjxc library and the supplemental libjxc_cpp library',
        ),
    ]


def get_output(*args, **kwargs) -> str:
    """
    Runs a process and returns its output as a string.
    """
    return subprocess.check_output([str(a) for a in args], **kwargs).decode('utf-8', errors='replace')


def append_to_filename(file_path: str, value: str) -> str:
    """
    Adds a string to the end of a filename, inserting it just _before_ the file's extension.
    Ex. append_to_filename('/a/b/c/release.zip', '_1.0') == '/a/b/c/release_1.0.zip'
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


def is_empty_dir(path: str) -> bool:
    """
    Checks if a path is an existing and empty directory.
    """
    if not os.path.exists(path) or not os.path.isdir(path):
        return False
    for _ in os.scandir(path):
        return False
    return True


def is_amalgamated_build_dir(path: str) -> bool:
    """
    Checks if the given path is an existing amalgamated build directory
    """
    return (os.path.exists(path)
        and os.path.isdir(path)
        and all(os.path.exists(os.path.join(path, name)) for name in ('jxc.h', 'jxc.cpp')))


def dir_entry_matches_pattern(base: str, entry: os.DirEntry, pattern_list: typing.Sequence[str]) -> bool:
    for pat in pattern_list:
        if fnmatch.fnmatchcase(entry.path, os.path.join(base, pat)):
            return True
    return False


def scan_dir_recursive(base_path: str, patterns: typing.Optional[typing.Sequence[str]] = None,
    orig_base_path: typing.Optional[str] = None) -> typing.Iterable[os.DirEntry]:
    """
    Recursively gets all files and directories from a path, yielding os.DirEntry objects.
    If glob is passed in, only yields entries that match a glob.
    """
    if orig_base_path is None:
        orig_base_path = base_path
    for item in os.scandir(base_path):
        if patterns is not None and not item.is_dir() and not dir_entry_matches_pattern(orig_base_path, item, patterns):
            continue
        yield item
        # scan directory contents
        if item.is_dir():
            yield from scan_dir_recursive(item.path, patterns=patterns, orig_base_path=orig_base_path)


def make_amalgamated_build(repo_path: str, output_path: str, flags: typing.List[str]):
    output_path = os.path.normpath(os.path.abspath(output_path))

    amalgamate_script = os.path.join(repo_path, 'tools', 'amalgamate.py')
    if not os.path.exists(amalgamate_script):
        raise FileNotFoundError(amalgamate_script)

    args: typing.List[str] = [sys.executable, amalgamate_script, *flags]
    if '--input' not in args:
        args += ['--input', repo_root]
    if '--output' not in args:
        args += ['--output', output_path]

    # make sure the output path exists and is an empty directory
    if not os.path.exists(output_path):
        os.mkdir(output_path)
    elif not is_empty_dir(output_path):
        if not is_amalgamated_build_dir(output_path):
            raise Exception(f'Directory {output_path!r} is not empty and does not appear to be an amalgamated build directory.')
        # remove the old build and make a new directory
        print(f'>>> os.rmtree({output_path!r})')
        shutil.rmtree(output_path)
        os.mkdir(output_path)
    
    assert os.path.exists(output_path) and is_empty_dir(output_path)
    print('>>>', ' '.join(args))
    subprocess.check_call(args)
    assert is_amalgamated_build_dir(output_path)


def make_release_zip_archive(jxc_version: str, output_path: str, packages: typing.Sequence[ArchivePackage], comment: typing.Optional[str] = None):
    """
    Creates a JXC release archive at output_path, containing both amalgamated builds.
    """
    output_path_dir, output_path_name = os.path.split(output_path)
    if 'VERSION' in output_path_name:
        output_path_name = output_path_name.replace('VERSION', jxc_version)
    elif jxc_version not in output_path_name:
        output_path_name = append_to_filename(output_path_name, f'_{jxc_version}')
    output_path = os.path.join(output_path_dir, output_path_name)

    zip_base_dir = f'jxc_{jxc_version}'

    print(f"Creating zip archive {output_path!r}...")

    with zipfile.ZipFile(output_path, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        if comment:
            zf.comment = comment.encode('utf-8')
        for pkg in packages:
            print(f"{pkg.src_dir!r} -> {pkg.zip_base_path!r}")
            for item in scan_dir_recursive(pkg.src_dir, pkg.src_patterns):
                if item.is_dir():
                    continue
                zip_path_parts = [zip_base_dir]
                if pkg.zip_base_path:
                    zip_path_parts.append(pkg.zip_base_path)

                if item.path != os.path.join(pkg.src_dir, item.name):
                    zip_path_parts.append(os.path.relpath(os.path.dirname(item.path), pkg.src_dir))

                zip_path_parts.append(item.name)
                zip_path = os.path.join(*zip_path_parts)
                print(f'\t{item.path!r} --> {zip_path!r}')
                zf.write(item.path, zip_path, compress_type=pkg.compress_type, compresslevel=pkg.compress_level)


def make_zip_comment(jxc_version: str, readme_text: str, packages: typing.List[ArchivePackage]) -> str:
    """
    Generates a zip file comment that includes the library version, overview of the zip file's contents, and the readme file
    """
    lines: typing.List[str] = [ line.rstrip() for line in readme_text.split('\n') ]

    # strip all links and empty lines from the top of the readme file
    while len(lines) > 0:
        if len(lines[0].strip()) == 0 or 'https' in lines[0].lower():
            lines = lines[1:]
        else:
            break

    # add package descriptions to the top of the zip file comment
    pkg_descriptions = []
    for pkg in packages:
        pkg_dir_name: str = pkg.zip_base_path.split('/')[0]
        if pkg.desc and pkg_dir_name:
            pkg_descriptions.append(f'- /{pkg_dir_name} - {pkg.desc}')

    if len(pkg_descriptions) > 0:
        lines = ['# Archive Contents', ''] + pkg_descriptions + [''] + lines
    
    if jxc_version:
        lines = [f'JXC {jxc_version}', ''] + lines
    
    return '\n'.join(lines)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=str, help='Repo root directory')
    parser.add_argument('--amalgamated-core', type=str, help='Path to the amalgamated core build. If it does not exist, it will be built.')
    parser.add_argument('--amalgamated-cpp', type=str, help='Path to the amalgamated cpp build. If it does not exist, it will be built.')
    parser.add_argument('--rebuild', action='store_true', help='Rebuild the amalgamated data, even if it already exists.')
    parser.add_argument('--output', type=str, help='Output zip file path. The JXC version will be added to the filename. '
        'If the string VERSION is in the filename, it will be replaced with the JXC version. Otherwise it will be appended to the end.')
    args = parser.parse_args()

    repo_root: str = os.path.abspath(args.source)
    if not os.path.exists(repo_root):
        raise FileNotFoundError(repo_root)

    # read the release version from the repo
    jxc_version: str = get_output(sys.executable, os.path.join(repo_root, 'tools', 'version.py')).strip()
    if len(list(int(v) for v in jxc_version.split('.'))) != 3:
        raise ValueError(f'Invalid JXC version detected: {jxc_version!r}')

    # ensure amalgamated_core is built
    amal_core: str = os.path.abspath(args.amalgamated_core)
    if args.rebuild or not os.path.exists(amal_core) or is_empty_dir(amal_core):
        make_amalgamated_build(repo_root, amal_core, ['--all-dependencies'])

    # ensure amalgamated_core is built
    amal_cpp: str = os.path.abspath(args.amalgamated_cpp)
    if args.rebuild or not os.path.exists(amal_cpp) or is_empty_dir(amal_cpp):
        make_amalgamated_build(repo_root, amal_cpp, ['--all-dependencies', '--jxc-cpp'])

    # make sure the output path exists, creating it and its parent directory if they don't exist
    # (this is to account for the output path being './build/output/jxc.zip' or similar, and the 'build' directory does not yet exist)
    output_path: str = os.path.abspath(args.output)
    assert not output_path.endswith('/')
    output_path_parent: str = os.path.dirname(output_path)
    if not os.path.exists(output_path_parent):
        output_path_parent_parent = os.path.dirname(output_path_parent)
        if os.path.exists(output_path_parent_parent) and os.path.isdir(output_path_parent_parent):
            os.makedirs(output_path_parent, exist_ok=True)
        else:
            raise ValueError(f"Output path parent directory {output_path_parent_parent} does not exist")

    # list of items included in the zip file
    packages: typing.List[ArchivePackage] = make_archive_package_list(repo_root, amal_core, amal_cpp)

    # generate a zip file comment
    with open(os.path.join(repo_root, 'README.md')) as fp:
        zip_comment = make_zip_comment(jxc_version, fp.read(), packages)

    # compile the release zip file
    make_release_zip_archive(jxc_version, output_path, comment=zip_comment, packages=packages)


