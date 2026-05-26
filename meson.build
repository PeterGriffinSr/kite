project(
    'kite',
    'cpp',
    version: '0.1.0',
    license: 'GPL-3.0-only',
    default_options: ['cpp_std=c++17', 'warning_level=2'],
)

conf = configuration_data()
conf.set_quoted('KITE_VERSION', meson.project_version())

configure_file(
    input: 'lib/version.hpp.in',
    output: 'version.hpp',
    configuration: conf,
)

kite_lib = library(
    'kite',
    sources: [
        'lib/cli/cli.cpp',
        'lib/common/error.cpp',
        'lib/kite.cpp',
    ],
    include_directories: include_directories('lib'),
)

executable(
    'kite',
    sources: ['bin/main.cpp'],
    link_with: kite_lib,
    include_directories: include_directories('lib'),
)