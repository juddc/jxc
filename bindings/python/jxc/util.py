import os
import sys
import time
import typing
import contextlib
import _pyjxc


TokenSpanType = typing.Union[_pyjxc.TokenSpan, _pyjxc.OwnedTokenSpan]


def get_main_module_name() -> str:
    """
    Helper to get the name of the __main__ module, if any
    """
    if (main_module := sys.modules.get("__main__", None)) and hasattr(main_module, '__file__'):
        return os.path.basename(main_module.__file__).removesuffix(".py")
    return '__main__'


_MAIN_MODULE_NAME = get_main_module_name()
_MAIN_MODULES = set(["__main__", get_main_module_name()])


def get_class_path(cls: type) -> str:
    """
    Returns a string representing a class that can be used to retrieve that class
    from a string using find_class.
    """
    match cls.__module__:
        case "__main__":
            #return f"{_MAIN_MODULE_NAME}.{cls.__name__}"
            return cls.__name__
        case "builtins":
            return cls.__name__
        case _:
            return f"{cls.__module__}.{cls.__name__}"


_FIND_CLASS_CACHE: dict[str, tuple[str, str]] = {}


def find_class(path: str) -> typing.Optional[type]:
    """
    Looks up a class using a path in the form `module.class_name`, importing the module if needed.
    If no module name is included, looks up the name in builtins or the __main__ module.
    """
    # We don't want to cache the actual classes, because then we're holding references to those classes and that can
    # be problematic for some modules.
    # Instead, we cache the module/class_name split - just the pair of strings. This seems like a micro-optimization,
    # but in practice this is a 5x speedup, which makes a difference when we're calling this thousands of times while
    # parsing a large JXC document.
    if cache_entry := _FIND_CLASS_CACHE.get(path, None):
        module_path, class_name = cache_entry
        if (mod := sys.modules.get(module_path, None)) and (cls := getattr(mod, class_name, None)):
            return cls
        return getattr(__import__(module_path, fromlist=[class_name]), class_name, None)

    # no cache entry exists - slow path
    match path.split("."):
        case []:
            return None

        case [class_name]:
            # no module listed, assume it's in the __main__ or builtins module
            if (main_module := sys.modules.get('__main__', None)) and (cls := getattr(main_module, class_name, None)):
                _FIND_CLASS_CACHE[path] = ('__main__', class_name)
                return cls
            else:
                _FIND_CLASS_CACHE[path] = ('builtins', class_name)
                return getattr(sys.modules['builtins'], class_name, None)

        case ["builtins", class_name]:
            # get the class from the builtins module
            _FIND_CLASS_CACHE[path] = ('builtins', class_name)
            return getattr(sys.modules['builtins'], class_name, None)

        case [main_module_name, class_name] if main_module_name in _MAIN_MODULES:
            # get the class from the __main__ module
            if main_module := sys.modules.get('__main__', None):
                _FIND_CLASS_CACHE[path] = ('__main__', class_name)
                return getattr(main_module, class_name, None)
            return None

        case [*module_parts, class_name]:
            module_path = ".".join(module_parts)
            _FIND_CLASS_CACHE[path] = (module_path, class_name)
            return getattr(__import__(module_path, fromlist=[class_name]), class_name, None)

    return None



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



if __name__ == "__main__":
    import _pyjxc
    import jxc
    import calendar
    import datetime

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

    test_class_paths = [ get_class_path(cls) for cls in test_class_list ]

    find_class_timer = PerformanceTimer()
    get_class_path_timer = PerformanceTimer()
    num_iters = 100000
    for _ in range(num_iters):
        with get_class_path_timer.iter():
            for cls in test_class_list:
                get_class_path(cls)

        with find_class_timer.iter():
            for class_path in test_class_paths:
                find_class(class_path)

    print('find_class() runtime:', find_class_timer)
    print('get_class_path() runtime:', get_class_path_timer)

    for cls in test_class_list:
        path: str = get_class_path(cls)
        cls_lookup: typing.Optional[type] = find_class(path) if path else None
        print(f'Class = {cls.__name__!r:<26} -> ClassPath = {path!r}')
        if cls is None or cls_lookup is None or cls is not cls_lookup:
            print('\tClass lookup failed:')
            try:
                print(f'\tget_class_path: {get_class_path(cls)}')
            except Exception as e:
                print(f'\tget_class_path: {e}')
            try:
                print(f'\tfind_class: {find_class(get_class_path(cls))}')
            except Exception as e:
                print(f'\tfind_class: {e}')
            print()

