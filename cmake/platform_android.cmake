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
    set_exported_symbols_file()
    
    target_compile_definitions(CouchbaseLiteCStatic PRIVATE -D_CRYPTO_MBEDTLS)
    target_link_libraries(CouchbaseLiteC PRIVATE atomic log zlibstatic)
endfunction()
