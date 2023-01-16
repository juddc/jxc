workspace "jxc"
    configurations { "debug", "release" }
    platforms { "linux64", "win64" }
    filter { "platforms:win64" }
        system "Windows"
        architecture "x86_64"
        buildoptions { "/bigobj" }
        defines { "_ITERATOR_DEBUG_LEVEL=0" }
    filter { "platforms:linux64" }
        system "Linux"
        architecture "x86_64"
    filter "configurations:debug"
        defines { "JXC_DEBUG=1" }
        optimize "Off"
        symbols "On"
    filter "configurations:release"
        defines { "JXC_DEBUG=0" }
        optimize "On"
        symbols "Off"


project "jxc"
    kind "StaticLib"
    language "C++"
    targetdir "build/%{cfg.buildcfg}"
    cppdialect "C++20"
    staticruntime "On"
    pic "On"
    flags { "MultiProcessorCompile" }
    files {
        "%{prj.location}/jxc/include/jxc/jxc.h",
        "%{prj.location}/jxc/include/jxc/jxc_array.h",
        "%{prj.location}/jxc/include/jxc/jxc_bytes.h",
        "%{prj.location}/jxc/include/jxc/jxc_core.h",
        "%{prj.location}/jxc/include/jxc/jxc_format.h",
        "%{prj.location}/jxc/include/jxc/jxc_lexer.h",
        "%{prj.location}/jxc/include/jxc/jxc_memory.h",
        "%{prj.location}/jxc/include/jxc/jxc_parser.h",
        "%{prj.location}/jxc/include/jxc/jxc_serializer.h",
        "%{prj.location}/jxc/include/jxc/jxc_string.h",
        "%{prj.location}/jxc/include/jxc/jxc_type_traits.h",
        "%{prj.location}/jxc/include/jxc/jxc_util.h",

        "%{prj.location}/jxc/src/jxc_core.cpp",
        "%{prj.location}/jxc/src/jxc_lexer.cpp",

        -- generated by re2c
        "%{prj.location}/jxc/src/jxc_lexer_gen.re",
        "%{prj.location}/jxc/src/jxc_lexer_gen.re.cpp",


        "%{prj.location}/jxc/src/jxc_parser.cpp",
        "%{prj.location}/jxc/src/jxc_serializer.cpp",
        "%{prj.location}/jxc/src/jxc_string.cpp",
        "%{prj.location}/jxc/src/jxc_util.cpp",

    }
    includedirs {
        "%{prj.location}/jxc/include",
        "%{prj.location}/subprojects/fast_float/include",
    }
    filter { "files:**.re", "platforms:win64" }
        buildmessage "Running re2c"
        buildcommands { "%{prj.location}tools/re2c.exe -W --verbose --utf8 --input-encoding utf8 --location-format msvc %{file.relpath} -o %{file.relpath}.cpp" }
        buildoutputs { "%{file.relpath}.cpp" }
    filter { "files:**.re", "platforms:linux64" }
        buildmessage "Running re2c"
        buildcommands { "re2c -W --verbose --utf8 --input-encoding utf8 --location-format gnu %{file.relpath} -o %{file.relpath}.cpp" }
        buildoutputs { "%{file.relpath}.cpp" }


project "jxc_cpp"
    kind "StaticLib"
    language "C++"
    targetdir "build/%{cfg.buildcfg}"
    cppdialect "C++20"
    staticruntime "On"
    pic "On"
    flags { "MultiProcessorCompile" }
    dependson { "jxc" }
    links { "jxc" }
    files {
        "%{prj.location}/jxc_cpp/include/jxc_cpp/jxc_converter.h",
        "%{prj.location}/jxc_cpp/include/jxc_cpp/jxc_converter_std.h",
        "%{prj.location}/jxc_cpp/include/jxc_cpp/jxc_converter_value.h",
        "%{prj.location}/jxc_cpp/include/jxc_cpp/jxc_document.h",
        "%{prj.location}/jxc_cpp/include/jxc_cpp/jxc_map.h",
        "%{prj.location}/jxc_cpp/include/jxc_cpp/jxc_value.h",

        "%{prj.location}/jxc_cpp/src/jxc_document.cpp",
        "%{prj.location}/jxc_cpp/src/jxc_value.cpp",
    }
    includedirs {
        "%{prj.location}/jxc/include",
        "%{prj.location}/jxc_cpp/include",
        "%{prj.location}/subprojects/unordered_dense/include",
    }


project "benchmark_jxc"
    kind "ConsoleApp"
    language "C++"
    targetdir "build/%{cfg.buildcfg}"
    cppdialect "C++20"
    staticruntime "On"
    pic "On"
    flags { "MultiProcessorCompile" }
    dependson { "jxc", "jxc_cpp" }
    links { "jxc_cpp", "jxc" }
    files {
        "%{prj.location}/jxc_benchmark/src/jxc_benchmark.cpp",
    }
    includedirs {
        "%{prj.location}/jxc_benchmark/src",
        "%{prj.location}/jxc/include",
        "%{prj.location}/jxc_cpp/include",
        "%{prj.location}/subprojects/unordered_dense/include",
    }


project "googletest"
    kind "StaticLib"
    language "C++"
    targetdir "build/%{cfg.buildcfg}"
    cppdialect "C++20"
    staticruntime "On"
    pic "On"
    flags { "MultiProcessorCompile" }
    files {
        "%{prj.location}/subprojects/googletest/googletest/src/gtest-all.cc",
    }
    includedirs {
        "%{prj.location}/subprojects/googletest/googletest/include",
        "%{prj.location}/subprojects/googletest/googletest",
    }
    filter "configurations:debug"
        defines { "DEBUG" }


project "test_jxc"
    kind "ConsoleApp"
    language "C++"
    targetdir "build/%{cfg.buildcfg}"
    cppdialect "C++20"
    staticruntime "On"
    pic "On"
    flags { "MultiProcessorCompile" }
    dependson { "googletest", "jxc", "jxc_cpp" }
    links { "googletest", "jxc", "jxc_cpp" }
    files {
        "%{prj.location}/tests/src/jxc_tests.h",
        "%{prj.location}/tests/src/jxc_core_tests.h",
        "%{prj.location}/tests/src/jxc_util_tests.h",
        "%{prj.location}/tests/src/jxc_cpp_tests.h",

        "%{prj.location}/tests/src/jxc_tests.cpp",
        "%{prj.location}/tests/src/jxc_core_tests.cpp",
        "%{prj.location}/tests/src/jxc_util_tests.cpp",
        "%{prj.location}/tests/src/jxc_cpp_tests.cpp",
    }
    includedirs {
        "%{prj.location}/tests",
        "%{prj.location}/jxc/include",
        "%{prj.location}/jxc_cpp/include",
        "%{prj.location}/subprojects/unordered_dense/include",
        "%{prj.location}/subprojects/googletest/googletest/include",
    }


function find_python_include_dir()
    local versions = { '3.12', '3.11', '3.10', '3.9', '3.8', '3.7' }
    for i, ver in ipairs(versions) do
        local py = os.findheader('python' .. ver .. '/Python.h')
        if py ~= nil then
            return os.findheader('python' .. ver .. '/Python.h') .. '/python' .. ver
        end
    end
    return nil
end


project "_pyjxc"
    kind "SharedLib"
    language "C++"
    targetdir "build/%{cfg.buildcfg}"
    cppdialect "C++17"
    staticruntime "On"
    pic "On"
    flags { "MultiProcessorCompile" }
    dependson { "jxc" }
    links { "jxc" }
    files {
        "%{prj.location}/bindings/python/src/jxc_pyparser.h",
        "%{prj.location}/bindings/python/src/jxc_pyserializer.h",
        "%{prj.location}/bindings/python/src/jxc_python.h",
        "%{prj.location}/bindings/python/src/jxc_python_util.h",

        "%{prj.location}/bindings/python/src/jxc_pyparser.cpp",
        "%{prj.location}/bindings/python/src/jxc_pyserializer.cpp",
        "%{prj.location}/bindings/python/src/jxc_python.cpp",
    }
    includedirs {
        "%{prj.location}/jxc/include",
        "%{prj.location}/bindings/python/src",
        "%{prj.location}/subprojects/pybind11/include",
        "%{prj.location}/subprojects/unordered_dense/include",
    }
    filter "platforms:win64"
        targetextension ".pyd"
        includedirs { "C:/Python310/include" }
        libdirs { "C:/Python310/libs" }
        links { "python310" }
        postbuildcommands {
            "py -3 ./tools/generate_pyjxc_stub.py --module-path ./build/%{cfg.buildcfg} --output ./bindings/python/_pyjxc/__init__.pyi"
        }
    filter "platforms:linux64"
        targetprefix ""
        links { os.findlib('python3') and (os.findlib('python3') .. '/python3') or nil }
        includedirs { find_python_include_dir() }
        postbuildcommands {
            "/usr/bin/env python3 ./tools/generate_pyjxc_stub.py --module-path ./build/%{cfg.buildcfg} --output ./bindings/python/_pyjxc/__init__.pyi"
        }


function setup_example_project(src_file, deps)
    kind "ConsoleApp"
    language "C++"
    targetdir "build/%{cfg.buildcfg}"
    cppdialect "C++20"
    staticruntime "On"
    pic "On"
    flags { "MultiProcessorCompile" }
    files { "%{prj.location}/jxc_examples/src/" .. src_file }
    dependson(deps)
    links(deps)
    includedirs {
        "%{prj.location}/jxc/include",
        "%{prj.location}/jxc_cpp/include",
        "%{prj.location}/subprojects/unordered_dense/include",
    }
end


project "example_basic_parsing"
    setup_example_project("basic_parsing.cpp", { "jxc" })

project "example_basic_serialization"
    setup_example_project("basic_serialization.cpp", { "jxc" })

project "example_basic_jxc_cpp_value"
    setup_example_project("basic_jxc_cpp_value.cpp", { "jxc_cpp", "jxc" })

project "example_custom_parser"
    setup_example_project("custom_parser.cpp", { "jxc" })
