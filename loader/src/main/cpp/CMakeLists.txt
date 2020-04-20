cmake_minimum_required(VERSION 3.4.1)

add_executable(
        app_process_loader.so
        app_process_loader.c
        hookext.c
)

target_link_libraries(app_process_loader.so log)

set_property(
        TARGET app_process_loader.so
        PROPERTY
        LINK_FLAGS
        # "-pie -fPIE": make sure this is a position-independent executable to be able to be loaded as a shared library
        # "-Wl,--export-dynamic": add all symbols to dynamic table, otherwise only referenced symbols will be kept
        # "-Wl,--version-script=": fine control which symbols are exported, in order to avoid symbol conflict
        "-pie -fPIE -Wl,--export-dynamic -Wl,--version-script=${CMAKE_SOURCE_DIR}/exported-symbols.txt"
)