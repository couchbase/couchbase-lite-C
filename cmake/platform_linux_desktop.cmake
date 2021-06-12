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

    set (_icu_libs)
        foreach (_lib icuuc icui18n icudata)
            unset (_iculib CACHE)
            find_library(_iculib ${_lib})
            if (NOT _iculib)
                message(FATAL_ERROR "${_lib} not found")
            endif()
            list(APPEND _icu_libs ${_iculib})
        endforeach()
        set (ICU_LIBS ${_icu_libs} CACHE STRING "ICU libraries" FORCE)
        message("Found ICU libs at ${ICU_LIBS}")

        find_path(LIBICU_INCLUDE unicode/ucol.h
            HINTS "${CMAKE_BINARY_DIR}/tlm/deps/icu4c.exploded"
            PATH_SUFFIXES include)
        if (NOT LIBICU_INCLUDE)
            message(FATAL_ERROR "libicu header files not found")
        endif()
        message("Using libicu header files in ${LIBICU_INCLUDE}")
        include_directories("${LIBICU_INCLUDE}")
        mark_as_advanced(ICU_LIBS LIBICU_INCLUDE)
endfunction()

function(set_dylib_properties)
    set_exported_symbols_file()
    
    target_link_libraries(CouchbaseLiteC PUBLIC z ${ICU_LIBS})
endfunction()
