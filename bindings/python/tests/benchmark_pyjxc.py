import os
import sys
import time
import glob
import argparse
import typing
import json
import contextlib
import calendar
import datetime


jxc_repo_root = os.path.realpath(os.path.join(os.path.abspath(os.path.dirname(__file__)), '..', '..', '..'))
jxc_build_release_path = os.path.join(jxc_repo_root, 'build', 'release')
jxc_repo_bindings_path = os.path.join(jxc_repo_root, 'bindings', 'python')

running_from_repo = all(os.path.exists(os.path.join(jxc_repo_root, dirname))
    for dirname in ('jxc', 'jxc_benchmark', 'jxc_cpp', 'jxc_examples'))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--path', '-p', type=str, nargs='*',
        help="Path inserted in sys.path before importing the _pyjxc and jxc modules")
    parser.add_argument('--class-lookup', '-c', action='store_true',
        help="Run benchmarks for jxc.util.get_class_path and jxc.util.find_class")
    parser.add_argument('--input', '-i', type=str, nargs='*',
        help='JXC or JSON files to benchmark. Accepts globs.')
    parser.add_argument('--num-iters', '-n', type=int, default=64,
        help='Number of times to run the benchmark')
    benchmark_args = parser.parse_args()

    if benchmark_args.path:
        for i, path in enumerate(benchmark_args.path):
            print(f'sys.path.insert({i}, {path!r})')
            sys.path.insert(i, path)


try:
    import _pyjxc
except ImportError:
    _pyjxc = None
    if running_from_repo and os.path.exists(jxc_build_release_path):
        sys.path.insert(0, jxc_build_release_path)
        try:
            import _pyjxc
        except ImportError:
            pass

try:
    import jxc
except ImportError:
    jxc = None
    if running_from_repo and os.path.exists(jxc_repo_bindings_path):
        sys.path.insert(1, jxc_repo_bindings_path)
        try:
            import jxc
        except ImportError:
            pass


if _pyjxc is None:
    print("Failed importing _pyjxc. You can pass the flag --path to add import search paths. See --help for usage.")
    sys.exit(1)

if jxc is None:
    print("Failed importing jxc. You can pass the flag --path to add import search paths. See --help for usage.")
    sys.exit(1)


import jxc.decode
import jxc.encode
import jxc.util
assert isinstance(_pyjxc.__version__, str)  # sanity check


class PerformanceTimer:
    def __init__(self):
        self.runs = []

    def reset(self):
        self.runs.clear()

    @contextlib.contextmanager
    def iter(self):
        start_time_ns = time.perf_counter_ns()
        yield
        end_time_ns = time.perf_counter_ns()
        self.runs.append(end_time_ns - start_time_ns)

    def elapsed_ns(self) -> int:
        return sum(self.runs)
    
    def elapsed_ms(self) -> float:
        return sum([ val / 1e6 for val in self.runs ])

    def average_ns(self) -> int:
        return int(self.elapsed_ns() / len(self.runs))
    
    def average_ms(self) -> float:
        return self.elapsed_ms() / len(self.runs)

    def __str__(self):
        return f"average {self.average_ns()}ns ({self.average_ms():.3f}ms) over {len(self.runs)} runs"



def benchmark_class_lookup():
    test_class_list = [
        int,
        float,
        str,
        bytes,
        bytearray,
        calendar.TextCalendar,
        calendar.HTMLCalendar,
        datetime.date,
        datetime.datetime,
        datetime.time,
        _pyjxc.Element,
        _pyjxc.ElementType,
        _pyjxc.Token,
        _pyjxc.TokenType,
    ]

    test_class_paths = [ jxc.util.get_class_path(cls) for cls in test_class_list ]

    find_class_timer = PerformanceTimer()
    get_class_path_timer = PerformanceTimer()
    num_iters = 100000
    for _ in range(num_iters):
        with get_class_path_timer.iter():
            for cls in test_class_list:
                jxc.util.get_class_path(cls)

        with find_class_timer.iter():
            for class_path in test_class_paths:
                jxc.util.find_class(class_path)

    print('find_class() runtime:', find_class_timer)
    print('get_class_path() runtime:', get_class_path_timer)

    for cls in test_class_list:
        path: str = jxc.util.get_class_path(cls)
        cls_lookup: typing.Optional[type] = jxc.util.find_class(path) if path else None
        print(f'Class = {cls.__name__!r:<26} -> ClassPath = {path!r}')
        if cls is None or cls_lookup is None or cls is not cls_lookup:
            print('\tClass lookup failed:')
            try:
                print(f'\tget_class_path: {jxc.util.get_class_path(cls)}')
            except Exception as e:
                print(f'\tget_class_path: {e}')
            try:
                print(f'\tfind_class: {jxc.util.find_class(jxc.util.get_class_path(cls))}')
            except Exception as e:
                print(f'\tfind_class: {e}')
            print()


def run_benchmark(label: str, files: typing.List[str], num_iters: int = 64, func: typing.Callable[[str], typing.Any] = None):
    benchmark_data: typing.List[str] = []
    for path in files:
        with open(path, 'r', encoding='utf-8') as fp:
            benchmark_data.append(fp.read())

    total_data_bytes = sum([ len(d) for d in benchmark_data ])
    if total_data_bytes < 2**10:
        total_data_size_str = f'{total_data_bytes} bytes'
    elif total_data_bytes < 2**20:
        total_data_size_str = f'{total_data_bytes / 1024:.2f} KB'
    else:
        total_data_size_str = f'{total_data_bytes / 1024 / 1024:.2f} MB'

    print(f"Benchmarking {label} (combined file size {total_data_size_str})...")

    result_data = []
    result_times = []
    for _ in range(num_iters):
        start_time = time.time()
        for data in benchmark_data:
            result_data.append(func(data))
        result_times.append(time.time() - start_time)

    avg_runtime_sec = sum(result_times) / len(result_times)
    avg_runtime_ms = avg_runtime_sec * 1000
    print(f"{label} average runtime ({num_iters} runs): {avg_runtime_ms:.4f}ms")



if __name__ == "__main__":
    if not benchmark_args.class_lookup and not benchmark_args.input:
        print("Nothing to benchmark. See --help for usage.")
        sys.exit(1)

    print(f'jxc module imported from {sys.modules["jxc"].__file__}')
    print(f'_pyjxc module imported from {sys.modules["_pyjxc"].__file__}')
    print()

    if benchmark_args.class_lookup:
        benchmark_class_lookup()

    if benchmark_args.input:
        files = []
        for path in benchmark_args.input:
            if '?' in path or '*' in path:
                for glob_path in glob.glob(path, recursive=True):
                    files.append(glob_path)
            else:
                files.append(path)

        if len(files) == 0:
            print(f"No matching files for glob list {benchmark_args.input!r}")
        else:
            print(f"Benchmarking files: {files!r}")

            run_benchmark('jxc.loads', files, benchmark_args.num_iters, lambda data: jxc.loads(data))

            # compare against the builtin json module if all inputs have json extensions
            if all(name.endswith('.json') for name in files):
                run_benchmark('json.loads', files, benchmark_args.num_iters, lambda data: json.loads(data))
