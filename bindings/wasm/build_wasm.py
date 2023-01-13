#!/usr/bin/env python3
import os
import sys
import shutil
import subprocess


DEBUG = True
EMCC = '/usr/lib/emscripten/emcc'


def compile(output: str, input_files: list[str], include_dirs: list[str], exports: list[str], debug=False):
    args = []
    if debug:
        args.append('-g3')  # include all the debug info
        args.append('-gsource-map')
    else:
        args.append('-g0')  # include no debug info
        args.append('-flto')  # link-time optimization

        #args.append('-O3')  # all the optimizations
        args.append('-Os')  # optimize for code size
        #args.append('-Oz')  # optimize even more based on code size at the expense of runtime perf
    
    args.append('-lembind')

    for path in include_dirs:
        args.append(f'-I{path}')

    if len(exports) > 0:
        args.append('-sEXPORTED_FUNCTIONS=' + ','.join(exports))

    args.append('-sEXPORTED_RUNTIME_METHODS=ccall,cwrap')

    args.append('-std=c++20')

    subprocess.check_call([EMCC] + input_files + ['-o', output] + args)


def print_usage_and_exit(ret_code=0):
    print(f"Usage: {sys.argv[0]} [debug|release]")
    print('\t(defaults to debug)')
    sys.exit(ret_code)


if __name__ == "__main__":
    args = sys.argv[1:]
    if len(args) > 0:
        if len(args) != 1:
            print(f"Error: Expected 0 or 1 arg, got {len(args)}")
            print_usage_and_exit(1)
        match args[0]:
            case '--help' | '-help' | '-h' | '-?' | '/?':
                print_usage_and_exit()
            case 'debug' | '--debug' | '-d':
                DEBUG = True
            case 'release' | '--release' | '-r':
                DEBUG = False
            case _:
                print("Error: Invalid argument")
                print_usage_and_exit(1)

    repo_root_dir = os.path.realpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
    wasm_source_dir = os.path.join(repo_root_dir, 'bindings', 'wasm')
    build_root_dir = os.path.join(repo_root_dir, 'build', 'wasm')

    if DEBUG:
        build_dir = os.path.join(build_root_dir, 'debug')
    else:
        build_dir = os.path.join(build_root_dir, 'release')
    os.makedirs(build_dir, exist_ok=True)

    source_files = [
        os.path.join(wasm_source_dir, 'jxc_wasm.cpp'),
    ]

    libjxc_src_dir = os.path.join(repo_root_dir, 'jxc', 'src')    
    for path in os.listdir(libjxc_src_dir):
        if path.endswith('.cpp'):
            source_files.append(os.path.join(libjxc_src_dir, path))

    compile(
        output=os.path.join(build_dir, 'jxc.js'),
        input_files=source_files,
        include_dirs=[
            os.path.join(repo_root_dir, 'jxc', 'include'),
            os.path.join(repo_root_dir, 'subprojects', 'fast_float', 'include'),
        ],
        exports=[],
        debug=DEBUG)

    # copy lexer tester
    lexer_test_path = os.path.join(wasm_source_dir, 'tests', 'test_wasm_lexer.html')
    shutil.copy2(lexer_test_path, os.path.join(build_dir, 'test_wasm_lexer.html'))

    # copy server script
    server_script_path = os.path.join(build_dir, 'run_test_server.sh')
    with open(server_script_path, 'w') as fp:
        fp.write("#!/bin/sh\n")
        fp.write(f"/usr/bin/env python3 -m http.server --bind 127.0.0.1 --directory {os.path.abspath(build_dir)} 8080\n")
    os.chmod(server_script_path, 0o755)
