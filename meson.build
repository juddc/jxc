project('jxc',
    'cpp',
    meson_version: '>= 0.56.0',
    version: run_command(find_program('python', 'tools/version.py'), check: true).stdout().strip(),
    default_options: ['warning_level=2', 'cpp_std=c++20'],
)

re2c_generator = generator(
    find_program('re2c'),
    output: '@BASENAME@.cpp',
    arguments: ['-W', '--verbose', '--utf8', '--input-encoding', 'utf8', '--location-format', 'gnu', '@INPUT@', '-o', '@OUTPUT@']
)

libjxc_inc = include_directories('include')
libjxc_src = [
    'jxc/src/jxc_core.cpp',
    'jxc/src/jxc_lexer.cpp',
    'jxc/src/jxc_parser.cpp',
    'jxc/src/jxc_serializer.cpp',
    'jxc/src/jxc_string.cpp',
    'jxc/src/jxc_util.cpp',
    re2c_generator.process('jxc/src/jxc_lexer_gen.re'),
]

libjxc = static_library(
    'jxc',
    sources: libjxc_src,
    dependencies: subproject('fast_float').get_variable('fast_float_dep'),
    include_directories: libjxc_inc,
    install: true)

libjxc_dep = declare_dependency(
    include_directories: libjxc_inc,
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
    link_with: libjxc)

# summary({
#     'bindir': get_option('bindir'),
#     'libdir': get_option('libdir'),
#     'datadir': get_option('datadir'),
#     },
#     section: 'Directories')
