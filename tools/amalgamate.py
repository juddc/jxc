#!/usr/bin/env python3
import os
import re
import glob
import argparse
import typing


PREPROCESSOR_REGEX = re.compile(r"^#(?:\s*)([a-z]+)(.*)$")


SORTED_HEADER_NAMES = [
    # Standalone headers - these have no dependencies other than the standard library
    'jxc_format.h',
    'jxc_type_traits.h',

    # jxc library
    'jxc_core.h',
    'jxc_memory.h',
    'jxc_bytes.h',
    'jxc_string.h',
    'jxc_array.h',
    'jxc_util.h',
    'jxc_lexer.h',
    'jxc_parser.h',
    'jxc_serializer.h',
    'jxc.h', # meta-header for jxc (includes all previous headers)

    # jxc-cpp library
    'jxc_map.h',
    'jxc_value.h',
    'jxc_document.h',
    'jxc_converter.h',
    'jxc_converter_std.h',
    'jxc_converter_value.h',
    'jxc_cpp.h', # meta-header for jxc-cpp
]


SORTED_CPP_NAMES = [
    # JXC library
    'jxc_core.cpp',
    'jxc_util.cpp',
    'jxc_string.cpp',
    'jxc_lexer.cpp',
    'jxc_lexer_gen.re.cpp',
    'jxc_parser.cpp',
    'jxc_serializer.cpp',

    # JXC-CPP library
    'jxc_document.cpp',
    'jxc_value.cpp',
]


def validate_file_list(var_name: str, var_type: str, input_file_names: list[str], valid_names: list[str]):
    """
    Checks if SORTED_HEADER_NAMES or SORTED_CPP_NAMES matches the input file list
    """
    name_set = set(input_file_names)
    valid_name_set = set(valid_names)
    unexpected_files = name_set.difference(valid_name_set)
    if len(unexpected_files) > 0:
        raise ValueError(f"Found one or more unknown {var_type}s: {sorted(list(unexpected_files))!r}. "
            f"If you added or removed a {var_type}, you need to update {var_name} in amalgamate.py.")


def sort_files(headers: dict[str, list[str]], sorted_names: list[str]) -> list[tuple[str, list[str]]]:
    result: list[tuple[str, list[str]]] = []
    used_headers = set()
    for name in sorted_names:
        assert name in headers
        assert name not in used_headers
        used_headers.add(name)
        result.append((name, headers[name]))
    return result


def parse_preprocessor_line(line: str) -> tuple[typing.Optional[str], typing.Optional[str]]:
    if preproc := PREPROCESSOR_REGEX.match(line):
        directive = preproc.group(1)
        directive_value = preproc.group(2)
        # strip single-line comment from line if there is one
        if "//" in directive_value:
            directive_value = directive_value.split("//")[0]
        return (directive, directive_value.strip())
    else:
        return (None, None)


def parse_include_path(inc: str) -> tuple[str, str]:
    path = inc.strip()
    assert len(path) > 2
    if path[0] == '<':
        if path[-1] != '>':
            raise ValueError(f"Invalid include {inc!r}")
        return (path, path[1:-1])
    elif path[0] == '"':
        if path[-1] != '"':
            raise ValueError(f"Invalid include {inc!r}")
        return (path, path[1:-1])
    else:
        raise ValueError(f"Invalid include {inc!r}")


class FileData:
    def __init__(self, path: str):
        self.name = os.path.basename(path)
        self.path = path
        with open(path) as fp:
            self.lines = [ line.rstrip() for line in fp ]

    def write(self, fp: typing.TextIO, write_name: bool = True):
        if write_name:
            fp.write(f"// ============ START {self.name} ============\n")
        for line in self.lines:
            fp.write(f"{line}\n")
        if write_name:
            fp.write(f"// ============ END {self.name} ============\n")

    def strip_pragma_once(self):
        found_pragma_once = False
        new_lines: list[str] = []
        for line in self.lines:
            preproc, preproc_value = parse_preprocessor_line(line)
            if preproc == "pragma" and preproc_value == "once":
                found_pragma_once = True
            else:
                new_lines.append(line)
        self.lines = new_lines
        return found_pragma_once

    def strip_includes(self) -> list[tuple[str, str]]:
        includes: list[tuple[str, str]] = []
        new_lines: list[str] = []
        for line in self.lines:
            preproc, preproc_value = parse_preprocessor_line(line)
            if preproc == "include":
                includes.append(parse_include_path(preproc_value))
            else:
                new_lines.append(line)
        self.lines = new_lines
        return includes

    def fixup_line_indicators(self, new_filename: str) -> list[tuple[str, str]]:
        new_lines: list[str] = []
        cur_line = 1
        for line in self.lines:
            preproc, preproc_value = parse_preprocessor_line(line)
            if preproc == "line":
                line_num, path = preproc_value.split(" ")
                line_num = int(line_num)
                assert path.startswith('"') and path.endswith('"')
                path = path[1:-1]
                if path.endswith('jxc_lexer_gen.re.cpp'):
                    new_lines.append(f'#line {cur_line + 1} "{new_filename}"')
                else:
                    new_lines.append(f'#line {line_num} "{path}"')
            else:
                new_lines.append(line)
            cur_line += 1

        self.lines = new_lines


def amalgamate(jxc_dir: str, deps_dir: str, output_dir: str, jxc_cpp_dir: typing.Optional[str], deps: set[str] | list[str]):
    # sort the dependency list for deterministic output
    if not isinstance(deps, list):
        deps = list(deps)
    deps.sort()

    # validate deps and load their files
    dep_data: list[FileData] = []
    for dep in deps:
        if not dep.endswith(".h"):
            raise ValueError(f"Invalid dependency {dep!r}: expected .h file")
        matching_files = list(glob.glob(os.path.join(deps_dir, '**', dep), recursive=True))
        if len(matching_files) == 1:
            dep_data.append(FileData(matching_files[0]))
        elif len(matching_files) > 1:
            raise ValueError(f"Found multiple possible paths for dependency {dep!r}: {matching_files!r}")
        else:
            raise FileNotFoundError(f"Dependency {dep!r} not found")

    std_includes: list[tuple[str, str]] = []
    dep_includes: list[tuple[str, str]] = []

    def classify_include(inc_tuple: tuple[str, str]):
        inc_value, inc_path = inc_tuple
        if inc_value.startswith('<') and inc_tuple not in std_includes:
            std_includes.append(inc_tuple)
        elif inc_value.startswith('"') and "jxc" not in inc_path and inc_tuple not in dep_includes:
            dep_includes.append(inc_tuple)

    include_globs = [os.path.join(jxc_dir, "include", "**", "*.h")]
    if jxc_cpp_dir is not None:
        include_globs.append(os.path.join(jxc_cpp_dir, 'include', '**', '*.h'))

    header_data: list[FileData] = []
    for inc_glob in include_globs:
        for path in glob.glob(inc_glob):
            data = FileData(path)
            data.strip_pragma_once()
            for inc in data.strip_includes():
                classify_include(inc)
            header_data.append(data)

    # make sure all headers in header_data have an entry in SORTED_HEADER_NAMES
    validate_file_list("SORTED_HEADER_NAMES", "header", [ f.name for f in header_data ], SORTED_HEADER_NAMES)

    # sort headers based on their ordering in SORTED_HEADER_NAMES
    header_data.sort(key=lambda data: SORTED_HEADER_NAMES.index(data.name))

    with open(os.path.join(output_dir, 'jxc.h'), 'w') as fp:
        fp.write("#ifndef JXC_H_GUARD\n")
        fp.write("#define JXC_H_GUARD\n")
        fp.write("\n")

        for inc_value, inc_path in std_includes:
            fp.write(f"#include {inc_value}\n")
        fp.write("\n")

        for inc_value, inc_path in dep_includes:
            dep_name = os.path.basename(inc_path)
            dep_idx = None
            for i in range(len(dep_data)):
                if dep_data[i].name == dep_name:
                    dep_idx = i
                    break
            if dep_idx is not None:
                data: FileData = dep_data.pop(dep_idx)
                data.write(fp)
            else:
                fp.write(f"#include {inc_value}\n")
        fp.write("\n")

        for data in header_data:
            data.write(fp)

        fp.write("#endif // #ifndef JXC_H_GUARD\n")
        fp.write("\n")

    std_includes.clear()
    dep_includes.clear()

    src_globs = [os.path.join(jxc_dir, 'src', "*.cpp")]
    if jxc_cpp_dir is not None:
        src_globs.append(os.path.join(jxc_cpp_dir, 'src', '*.cpp'))

    src_data: list[FileData] = []
    for src_glob in src_globs:
        for path in glob.glob(src_glob):
            data = FileData(path)
            for inc in data.strip_includes():
                classify_include(inc)
            src_data.append(data)

    # make sure all headers in header_data have an entry in SORTED_CPP_NAMES
    validate_file_list("SORTED_CPP_NAMES", "cpp file", [ f.name for f in src_data ], SORTED_CPP_NAMES)

    # sort source files based on their ordering in SORTED_HEADER_NAMES
    src_data.sort(key=lambda data: SORTED_CPP_NAMES.index(data.name))

    src_output_file = os.path.join(output_dir, 'jxc.cpp')
    with open(src_output_file, 'w') as fp:
        fp.write("#include \"jxc.h\"\n")
        fp.write('\n')

        for inc_value, inc_path in std_includes:
            fp.write(f"#include {inc_value}\n")
        fp.write('\n')

        for inc_value, inc_path in dep_includes:
            dep_name = os.path.basename(inc_path)
            dep_idx = None
            for i in range(len(dep_data)):
                if dep_data[i].name == dep_name:
                    dep_idx = i
                    break
            if dep_idx is not None:
                data: FileData = dep_data.pop(dep_idx)
                data.write(fp)
            else:
                fp.write(f"#include {inc_value}\n")

        fp.write('\n')

        for data in src_data:
            data.write(fp)

        fp.write('\n')

    # now that we know what the output file looks like, rewrite it with the `#line` values fixed up
    src_output_data: FileData = FileData(src_output_file)
    src_output_data.fixup_line_indicators(os.path.basename(src_output_file))
    with open(src_output_file, 'w') as fp:
        src_output_data.write(fp, write_name=False)


class Template:
    def __init__(self, tmpl: str, ctx: typing.Optional[dict[str, typing.Any]] = None):
        self.tmpl = tmpl.strip()
        self.ctx = ctx or {}
    
    def copy(self):
        return Template(self.tmpl + '', { k: v for k, v in self.ctx.items() })

    def set(self, var_name: str, var_value: typing.Any):
        self.ctx[var_name] = var_value

    def render(self) -> str:
        result = self.tmpl
        for var_name, var_value in self.ctx.items():
            result = result.replace(f"${var_name}$", str(var_value))
        return result + '\n'


PREMAKE5_LUA = Template(r"""
workspace "test_jxc_amalgamate"
    configurations { "debug", "release" }
    platforms { "linux64", "win64" }
    filter { "platforms:win64" }
        system "Windows"
        architecture "x86_64"
    filter { "platforms:linux64" }
        system "Linux"
        architecture "x86_64"

project "test_jxc_amalgamate"
    kind "ConsoleApp"
    language "C++"
    targetdir "build/%{cfg.buildcfg}"
    cppdialect "C++20"
    staticruntime "On"
    pic "On"
    flags { "MultiProcessorCompile" }
    files {
        "%{prj.location}/jxc.h",
        "%{prj.location}/jxc.cpp",
        "%{prj.location}/test_jxc_amalgamate_main.cpp",
    }
    includedirs {
        "%{prj.location}",
    }
    filter "configurations:debug"
        defines { "JXC_DEBUG=1" }
        optimize "Off"
        symbols "On"
    filter "configurations:release"
        defines {"JXC_DEBUG=0"}
        optimize "On"
        symbols "Off"
""")

TEST_JXC_AMALGAMATE_MAIN = Template(r"""
#include <iostream>
#include <string>
#include "jxc.h"

#define ENABLE_JXC_CPP $ENABLE_JXC_CPP$

bool parse_elements(const std::string& buf)
{
    std::cout << "Parsing " << jxc::detail::debug_string_repr(buf) << ":" << std::endl;
    size_t idx = 0;
    jxc::JumpParser parser(buf);
    while (parser.next())
    {
        std::cout << "\t[" << idx++ << "]: parser.next() -> " << parser.value().to_repr() << std::endl;
    }
    if (parser.has_error())
    {
        std::cerr << "\t" << "Parse error: " << parser.get_error().to_string(buf) << std::endl;
        return false;
    }
    return true;
}

template<typename T>
using SerializeFunc = void (*)(jxc::Serializer&, const T&);

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

void serialize_pixel(jxc::Serializer& doc, const int32_t& value)
{
    doc.value_int(value, "px");
}

void serialize_vec3(jxc::Serializer& doc, const Vec3& value)
{
    doc.annotation("vec3")
        .object_begin()
        .identifier("x").object_sep().value_float(value.x, "", 4, true)
        .identifier("y").object_sep().value_float(value.y, "", 4, true)
        .identifier("z").object_sep().value_float(value.z, "", 4, true)
        .object_end();
}

template<typename T>
std::string test_serialize(const T& value, SerializeFunc<T> callback)
{
    jxc::StringOutputBuffer result;
    auto settings = jxc::SerializerSettings::make_compact();
    jxc::Serializer doc(&result, settings);
    callback(doc, value);
    doc.flush();

    const std::string str = result.to_string();
    std::cout << "Serialized value to " << jxc::detail::debug_string_repr(str) << std::endl;
    return str;
}

bool run_jxc_tests()
{
    if (!parse_elements("null")) { return false; }
    if (!parse_elements("true")) { return false; }
    if (!parse_elements("25.2%")) { return false; }
    if (!parse_elements("vec3[0, 1, -2]")) { return false; }
    if (!parse_elements("rgb{r: 0.4, g: 0.7, b: 0.9}")) { return false; }

    if (test_serialize(100, &serialize_pixel) != "100px") { return false; }
    if (test_serialize(-42, &serialize_pixel) != "-42px") { return false; }
    if (test_serialize(Vec3{ -4.5f, 60.775f, 0.0f }, &serialize_vec3) != "vec3{x:-4.5000,y:60.7750,z:0.0000}") { return false; }
    return true;
}

#if ENABLE_JXC_CPP
bool test_value_type(const jxc::Value& val, jxc::ValueType expected_type)
{
    if (val.get_type() != expected_type)
    {
        std::cerr << "Expected value type " << jxc::value_type_to_string(expected_type)
            << ", got " << jxc::value_type_to_string(val.get_type()) << "\n";
        return false;
    }
    return true;
}

bool test_value_annotation(const jxc::Value& val, const std::string& expected_annotation)
{
    if (val.get_annotation_source().as_view() != expected_annotation)
    {
        std::cerr << "Expected annotation " << jxc::detail::debug_string_repr(expected_annotation)
            << ", got " << jxc::detail::debug_string_repr(val.get_annotation_source().as_view()) << "\n";
        return false;
    }
    return true;
}

bool test_serialize_value(const jxc::Value& val, const std::string& val_jxc_expected)
{
    const std::string val_repr = val.to_repr();
    const std::string val_jxc = jxc::serialize(val, jxc::SerializerSettings::make_compact());
    if (val_jxc == val_jxc_expected)
    {
        std::cout << "Serialized " << val_repr << " to " << jxc::detail::debug_string_repr(val_jxc) << std::endl;
        return true;
    }
    else
    {
        std::cerr << "Serialize error for value " << val_repr << ":" << std::endl;
        std::cerr << "\t[Expected] " << jxc::detail::debug_string_repr(val_jxc_expected) << std::endl;
        std::cerr << "\t[ Actual ] " << jxc::detail::debug_string_repr(val_jxc) << std::endl;
        return false;
    }
}

bool run_jxc_cpp_tests()
{
    // parsing
    {
        jxc::Value val = jxc::parse("null");
        if (!test_value_type(val, jxc::ValueType::Null)) { return false; }
    }

    {
        jxc::Value val = jxc::parse("vec3{ x: 10, y: -20, z: 100 }");
        if (!val.is_owned(true)) { std::cerr << "Expected owned data, got view\n"; return false; }
        if (!test_value_type(val, jxc::ValueType::Object)) { return false; }
        if (!test_value_annotation(val, "vec3")) { return false; }
        if (val.size() != 3) { std::cerr << "Expected object size 3, got " << val.size() << "\n"; return false; }
        if (!val.contains("x")) { std::cerr << "Expected `x` key\n"; return false; }
        if (!val.contains("y")) { std::cerr << "Expected `y` key\n"; return false; }
        if (!val.contains("z")) { std::cerr << "Expected `z` key\n"; return false; }
        if (val["x"].cast<int32_t>() != 10) { std::cerr << "Expected obj.x == 10, got " << val["x"].to_repr() << "\n"; return false; }
        if (val["y"].cast<int32_t>() != -20) { std::cerr << "Expected obj.x == -20, got " << val["y"].to_repr() << "\n"; return false; }
        if (val["z"].cast<int32_t>() != 100) { std::cerr << "Expected obj.y == 100, got " << val["z"].to_repr() << "\n"; return false; }
        jxc::Value val_constructed = jxc::annotated_object("vec3", {
            std::make_pair("x", 10),
            std::make_pair("y", -20),
            std::make_pair("z", 100),
        });
        if (val != val_constructed)
        {
            std::cerr << "Expected " << val.to_repr() << " == " << val_constructed.to_repr() << "\n";
            return false;
        }
    }

    if (!test_serialize_value(jxc::Value(nullptr), "null")) { return false; }
    if (!test_serialize_value(jxc::Value(12.5, "px"), "12.5px")) { return false; }
    if (!test_serialize_value(jxc::Value{ true, 1234, "oh hai" }, "[true,1234,\"oh hai\"]")) { return false; }
    jxc::Value quat(jxc::default_object);
    quat.set_annotation("quat");
    quat["x"] = 0.0;
    quat["y"] = 0.0;
    quat["z"] = 0.0;
    quat["w"] = 1.0;
    if (!test_serialize_value(quat, "quat{x:0.0,y:0.0,z:0.0,w:1.0}")) { return false; }
    return true;
}
#endif // ENABLE_JXC_CPP

int main(int argc, const char** argv)
{
    std::cout << "=== Running libjxc tests ===\n";
    if (!run_jxc_tests())
    {
        std::cerr << "One or more jxc tests failed!" << std::endl;
        return 1;
    }
    std::cout << "=== libjxc tests ran successfully ===\n\n";

#if ENABLE_JXC_CPP
    std::cout << "=== Running libjxc_cpp tests ===\n";
    if (!run_jxc_cpp_tests())
    {
        std::cerr << "One or more jxc_cpp tests failed!" << std::endl;
        return 1;
    }
    std::cout << "=== libjxc_cpp tests ran successfully ===\n\n";
#endif // ENABLE_JXC_CPP

    return 0;
}
""", {
    "ENABLE_JXC_CPP": 0,
})


def test_amalgamate(output_dir: str, enable_jxc_cpp: bool):
    premake5_lua = PREMAKE5_LUA.copy()
    with open(os.path.join(output_dir, 'premake5.lua'), 'w') as fp:
        fp.write(premake5_lua.render())

    test_jxc_amalgamate_main = TEST_JXC_AMALGAMATE_MAIN.copy()
    test_jxc_amalgamate_main.set("ENABLE_JXC_CPP", 1 if enable_jxc_cpp else 0)
    with open(os.path.join(output_dir, 'test_jxc_amalgamate_main.cpp'), 'w') as fp:
        fp.write(test_jxc_amalgamate_main.render())



if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', '-i', type=str, default='.', help='Input directory (repo root)')
    parser.add_argument('--output', '-o', type=str, default='./build/amalgamated', help='Output directory')
    parser.add_argument('--jxc-cpp', '-c', action='store_true', help='Include jxc_cpp library')
    parser.add_argument('--all-dependencies', '-a', action='store_true', help='Include all dependencies')
    parser.add_argument('--dependency', '-d', action='append', help='Include dependency by header name')
    parser.add_argument('--test', '-t', action='store_true', help='Output a premake5.lua build script main.cpp file and run some basic tests')
    args = parser.parse_args()

    if not os.path.exists(args.input) or not os.path.isdir(args.input):
        raise ValueError(f"Input directory {args.input!r} does not exist or is not a directory")

    if not os.path.exists(args.output):
        # create the output dir and any needed parent dirs
        os.makedirs(args.output, exist_ok=True)
    if not os.path.exists(args.output):
        raise ValueError(f"Output directory {args.output!r} does not exist or is not a directory")
    
    def is_library_path(path: str) -> bool:
        return (os.path.exists(path)
            and os.path.isdir(path)
            and os.path.exists(os.path.join(path, 'include'))
            and os.path.exists(os.path.join(path, 'src')))

    jxc_path = os.path.join(args.input, 'jxc')
    if not is_library_path(jxc_path):
        raise ValueError(f"Failed to find jxc library directory (tried {jxc_path!r})")

    jxc_cpp_path = None
    if args.jxc_cpp:
        jxc_cpp_path = os.path.join(args.input, 'jxc_cpp')
        if not is_library_path(jxc_cpp_path):
            raise ValueError(f"Failed to find jxc-cpp library directory (tried {jxc_cpp_path!r})")

    deps_path = os.path.join(args.input, 'subprojects')
    if not os.path.exists(deps_path) or not os.path.isdir(deps_path):
        raise ValueError(f"Failed to find valid dependencies base directory (tried {deps_path!r})")

    deps = set()
    if args.dependency:
        for name in args.dependency:
            deps.add(name)

    if args.all_dependencies:
        deps.add("fast_float.h")
        deps.add("unordered_dense.h")

    amalgamate(jxc_path, deps_path, args.output, jxc_cpp_dir=jxc_cpp_path, deps=deps)

    if args.test:
        test_amalgamate(args.output, enable_jxc_cpp=args.jxc_cpp)

