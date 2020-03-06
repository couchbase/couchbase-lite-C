include("${CMAKE_CURRENT_LIST_DIR}/platform_linux.cmake")

function(set_platform_source_files)
    # No-op
endfunction()

function(set_platform_include_directories)
    # No-op
endfunction()

function(init_vars)
    init_vars_linux()
endfunction()

function(set_dylib_properties)
    target_link_libraries(CouchbaseLiteC PUBLIC z ${ICU_LIBS})
endfunction()
