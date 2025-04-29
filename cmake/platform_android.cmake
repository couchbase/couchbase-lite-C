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

function(set_android_exported_symbols_file_and_linker_flags)
    if(BUILD_ENTERPRISE)
        set(EXPORT_LINK_FLAGS "-Wl,--version-script=${PROJECT_SOURCE_DIR}/src/exports/generated/CBL_EE_Android.gnu")
    else()
        set(EXPORT_LINK_FLAGS "-Wl,--version-script=${PROJECT_SOURCE_DIR}/src/exports/generated/CBL_Android.gnu")
    endif()
    
    # Set the final link flags
    set(ANDROID_LINK_FLAGS "${EXPORT_LINK_FLAGS} -Wl,-z,max-page-size=16384")
    
    # Set the target properties with the combined link flags
    set_target_properties(cblite PROPERTIES LINK_FLAGS "${ANDROID_LINK_FLAGS}")
endfunction()

function(set_dylib_properties)
    set_android_exported_symbols_file_and_linker_flags()
    
    target_compile_definitions(cblite-static PRIVATE -D_CRYPTO_MBEDTLS)
    target_link_libraries(cblite PRIVATE atomic log zlibstatic)
endfunction()
