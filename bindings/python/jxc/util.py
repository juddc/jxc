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

