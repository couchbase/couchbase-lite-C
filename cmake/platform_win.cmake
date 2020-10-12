function(set_platform_source_files)
    # No platform specific
endfunction()

function(set_platform_include_directories)
    set(oneValueArgs RESULT)
    cmake_parse_arguments(PLATFORM "" ${oneValueArgs} "" ${ARGN})
    if(NOT DEFINED PLATFORM_RESULT)
        message(FATAL_ERROR set_platform_include_directories needs to be called with RESULT)
    endif()

    set(
        ${PLATFORM_RESULT}
        vendor/couchbase-lite-core/include/MSVC
        vendor/couchbase-lite-core/vendor/fleece/MSVC
        PARENT_SCOPE
    )
endfunction()

function(init_vars)
    # Compile string literals as UTF-8,
    # Enable exception handling for C++ but disable for extern C
    # Disable the following warnings:
    #   4068 (unrecognized pragma)
    #   4099 (type first seen as struct now seen as class)
    # Disable warning about "insecure" C runtime functions (strcpy vs strcpy_s)
    string(
        CONCAT CBL_CXX_FLAGS 
        "/utf-8 "
        "/EHsc "
        "/wd4068 "
        "/wd4099 "
        "-D_CRT_SECURE_NO_WARNINGS=1"
    )
    set(CBL_CXX_FLAGS ${CBL_CXX_FLAGS} CACHE INTERNAL "")

    string(
        CONCAT CBL_C_FLAGS
        "/utf-8 "
        "/wd4068 "
        "-D_CRT_SECURE_NO_WARNINGS=1" 
    )
    set(CBL_C_FLAGS ${CBL_C_FLAGS} CACHE INTERNAL "")
endfunction()

function(set_dylib_properties)
    if(WINDOWS_STORE)
        target_compile_definitions(CouchbaseLiteCStatic PRIVATE -DMBEDTLS_NO_PLATFORM_ENTROPY)
        set_target_properties(CouchbaseLiteC PROPERTIES COMPILE_FLAGS /ZW)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /nodefaultlib:kernel32.lib /nodefaultlib:ole32.lib")
    endif()
    set_target_properties(CouchbaseLiteC PROPERTIES LINK_FLAGS
        "/def:${PROJECT_SOURCE_DIR}/src/exports/CBL.def")
    target_link_libraries(CouchbaseLiteC PRIVATE zlibstatic Ws2_32)
    target_compile_definitions(CouchbaseLiteCStatic PRIVATE LITECORE_EXPORTS)
endfunction()
