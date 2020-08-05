function(set_platform_source_files)
    set(oneValueArgs RESULT)
    cmake_parse_arguments(PLATFORM "" ${oneValueArgs} "" ${ARGN})
    if(NOT DEFINED PLATFORM_RESULT)
        message(FATAL_ERROR set_platform_include_directories needs to be called with RESULT)
    endif()

    set(
        ${PLATFORM_RESULT}
        src/CBLDatabase+ObjC.mm
        PARENT_SCOPE
    )
endfunction()

function(set_platform_include_directories)
    # No-op
endfunction()

function(init_vars)
    # No-op
endfunction()

function(set_dylib_properties)
    set_target_properties(CouchbaseLiteC PROPERTIES LINK_FLAGS
            "-exported_symbols_list ${PROJECT_SOURCE_DIR}/src/exports/generated/CBL.exp")
    target_link_libraries(CouchbaseLiteC PUBLIC
            "-framework CoreFoundation"
            "-framework Foundation"
            "-framework CFNetwork"
            "-framework Security"
            "-framework SystemConfiguration"
            z)
endfunction()
