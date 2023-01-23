project('jxc',
    'cpp',
    meson_version: '>= 0.56.0',
    version: run_command(find_program('tools/version.py'), check: true).stdout().strip(),
    default_options: ['warning_level=2', 'cpp_std=c++20'],
)

libjxc_inc = include_directories('jxc/include')
libjxc_src = [
    'jxc/src/jxc_core.cpp',
    'jxc/src/jxc_lexer.cpp',
    'jxc/src/jxc_parser.cpp',
    'jxc/src/jxc_serializer.cpp',
    'jxc/src/jxc_string.cpp',
    'jxc/src/jxc_util.cpp',
]

re2c_exe = find_program('re2c', version: '>=3.0', required: false)
if re2c_exe.found()
    re2c_generator = generator(
        re2c_exe,
        output: '@BASENAME@.cpp',
        arguments: ['-W', '--verbose', '--utf8', '--input-encoding', 'utf8', '--location-format', 'gnu', '@INPUT@', '-o', '@OUTPUT@']
    )
    libjxc_src += [re2c_generator.process('jxc/src/jxc_lexer_gen.re')]
else
    message('re2c not found; using pregenerated lexer')
    libjxc_src += ['jxc/src/jxc_lexer_gen.re.cpp']
endif

libjxc = static_library(
    'jxc',
    sources: libjxc_src,
    dependencies: [
        subproject('fast_float').get_variable('fast_float_dep'),
    ],
    include_directories: libjxc_inc,
    install: true)

libjxc_dep = declare_dependency(
    include_directories: libjxc_inc,
    dependencies: [
        subproject('fast_float').get_variable('fast_float_dep')
    ],
    link_with: libjxc)

libjxc_cpp_inc = include_directories('jxc_cpp/include')

libjxc_cpp_src = [
    'jxc_cpp/src/jxc_document.cpp',
    'jxc_cpp/src/jxc_value.cpp',
]

libjxc_cpp = static_library(
    'jxc_cpp',
    sources: libjxc_cpp_src,
    dependencies: [
        libjxc_dep,
        subproject('unordered_dense').get_variable('unordered_dense_dep'),
    ],
    include_directories: libjxc_cpp_inc,
    install: true)

libjxc_cpp_dep = declare_dependency(
    include_directories: libjxc_cpp_inc,
    dependencies: [
        subproject('unordered_dense').get_variable('unordered_dense_dep'),
    ],
    link_with: [
        libjxc,
        libjxc_cpp,
    ])

if not meson.is_subproject()
    install_headers('jxc/jxc_array.h', subdir: 'jxc')
    install_headers('jxc/jxc_bytes.h', subdir: 'jxc')
    install_headers('jxc/jxc_core.h', subdir: 'jxc')
    install_headers('jxc/jxc_format.h', subdir: 'jxc')
    install_headers('jxc/jxc_lexer.h', subdir: 'jxc')
    install_headers('jxc/jxc_memory.h', subdir: 'jxc')
    install_headers('jxc/jxc_parser.h', subdir: 'jxc')
    install_headers('jxc/jxc_serializer.h', subdir: 'jxc')
    install_headers('jxc/jxc_string.h', subdir: 'jxc')
    install_headers('jxc/jxc_type_traits.h', subdir: 'jxc')
    install_headers('jxc/jxc_util.h', subdir: 'jxc')
    install_headers('jxc/jxc.h', subdir: 'jxc')

    if get_option('jxc_cpp')
        install_headers('jxc_cpp/jxc_converter_std.h', subdir: 'jxc_cpp')
        install_headers('jxc_cpp/jxc_converter_value.h', subdir: 'jxc_cpp')
        install_headers('jxc_cpp/jxc_converter.h', subdir: 'jxc_cpp')
        install_headers('jxc_cpp/jxc_document.h', subdir: 'jxc_cpp')
        install_headers('jxc_cpp/jxc_map.h', subdir: 'jxc_cpp')
        install_headers('jxc_cpp/jxc_value.h', subdir: 'jxc_cpp')
        install_headers('jxc_cpp/jxc_cpp.h', subdir: 'jxc_cpp')
    endif
endif

# summary({
#     'bindir': get_option('bindir'),
#     'libdir': get_option('libdir'),
#     'datadir': get_option('datadir'),
#     },
#     section: 'Directories')
