sources = [
    'djinterop/enginelibrary/el_crate_impl.cpp',
    'djinterop/enginelibrary/el_database_impl.cpp',
    'djinterop/enginelibrary/el_storage.cpp',
    'djinterop/enginelibrary/el_track_impl.cpp',
    'djinterop/enginelibrary/el_transaction_guard_impl.cpp',
    'djinterop/enginelibrary/encode_decode_utils.cpp',
    'djinterop/enginelibrary/performance_data_format.cpp',
    'djinterop/enginelibrary/schema/schema_1_6_0.cpp',
    'djinterop/enginelibrary/schema/schema_1_7_1.cpp',
    'djinterop/enginelibrary/schema/schema_1_9_1.cpp',
    'djinterop/enginelibrary/schema/schema_1_11_1.cpp',
    'djinterop/enginelibrary/schema/schema_1_13_0.cpp',
    'djinterop/enginelibrary/schema/schema_1_13_1.cpp',
    'djinterop/enginelibrary/schema/schema_1_13_2.cpp',
    'djinterop/enginelibrary/schema/schema_1_15_0.cpp',
    'djinterop/enginelibrary/schema/schema_1_17_0.cpp',
    'djinterop/enginelibrary/schema/schema_1_18_0_ep.cpp',
    'djinterop/enginelibrary/schema/schema_1_18_0_fw.cpp',
    'djinterop/enginelibrary/schema/schema.cpp',
    'djinterop/crate.cpp',
    'djinterop/database.cpp',
    'djinterop/enginelibrary.cpp',
    'djinterop/track.cpp',
    'djinterop/transaction_guard.cpp',
    'djinterop/util.cpp',
    'djinterop/impl/crate_impl.cpp',
    'djinterop/impl/database_impl.cpp',
    'djinterop/impl/track_impl.cpp',
    'djinterop/impl/transaction_guard_impl.cpp',
]

# Dependencies required by the main library.
core_deps = [thread_dep, dl_dep, zlib_dep, sqlite3_dep, sqlite_modern_cpp_dep]

# Set a compile-time definition that we are building the libdjinterop source.
# This information is needed in `config.hpp` to determine whether public symbols
# ought to be exported or imported.
building_library_args = ['-DDJINTEROP_SOURCE']

djinterop_lib = library(
    'djinterop',
    sources: sources,
    include_directories: inc,
    dependencies: core_deps,
    install: true,
    version: meson.project_version(),
    soversion: meson.project_version().split('.')[0],
    cpp_args: building_library_args)

