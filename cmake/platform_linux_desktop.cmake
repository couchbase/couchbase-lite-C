include("${CMAKE_CURRENT_LIST_DIR}/platform_linux.cmake")

function(set_platform_source_files)
    # No-op
endfunction()

function(set_platform_include_directories)
    # No-op
endfunction()

function(init_vars)
    init_vars_linux()

    set (CMAKE_INSTALL_RPATH "\$ORIGIN" PARENT_SCOPE)

    find_path(LIBICU_INCLUDE unicode/ucol.h
        HINTS "${CMAKE_BINARY_DIR}/tlm/deps/icu4c.exploded"
        PATH_SUFFIXES include)
    if (NOT LIBICU_INCLUDE)
        message(FATAL_ERROR "libicu header files not found")
    endif()
    message("Using libicu header files in ${LIBICU_INCLUDE}")
    include_directories("${LIBICU_INCLUDE}")
    mark_as_advanced(LIBICU_INCLUDE)

    
    set(CBL_CXX_FLAGS "${CBL_CXX_FLAGS} -Wno-psabi" CACHE INTERNAL "")
    set(LITECORE_DYNAMIC_ICU ON CACHE BOOL "If enabled, search for ICU at runtime so as not to depend on a specific version")
endfunction()

function(set_dylib_properties)
    set_exported_symbols_file()
endfunction()
