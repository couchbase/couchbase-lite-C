include("${CMAKE_CURRENT_LIST_DIR}/platform_linux.cmake")

function(set_platform_source_files)
    set(oneValueArgs RESULT)
    cmake_parse_arguments(PLATFORM "" ${oneValueArgs} "" ${ARGN})
    if(NOT DEFINED PLATFORM_RESULT)
        message(FATAL_ERROR set_platform_source_files needs to be called with RESULT)
    endif()

    set(
        ${PLATFORM_RESULT}
        src/CBLDatabase+Android.cc
        src/CBLPlatform_CAPI+Android.cc
        PARENT_SCOPE
    )
endfunction()

function(set_platform_include_directories)
    # No-op
endfunction()

function(init_vars)
    init_vars_linux()
endfunction()

function(set_android_exported_symbols_file)
    if(BUILD_ENTERPRISE)
        set_target_properties(
            cblite PROPERTIES LINK_FLAGS
            "-Wl,--version-script=${PROJECT_SOURCE_DIR}/src/exports/generated/CBL_Android_EE.gnu")
    else()
                set_target_properties(
            cblite PROPERTIES LINK_FLAGS
            "-Wl,--version-script=${PROJECT_SOURCE_DIR}/src/exports/generated/CBL_Android.gnu")
    endif()
endfunction()

function(set_dylib_properties)
    set_android_exported_symbols_file()
    
    target_compile_definitions(cblite-static PRIVATE -D_CRYPTO_MBEDTLS)
    target_link_libraries(cblite PRIVATE atomic log zlibstatic)
endfunction()
