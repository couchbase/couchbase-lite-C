function(init_vars_linux)
    set(WHOLE_LIBRARY_FLAG "-Wl,--whole-archive" CACHE INTERNAL "")
    set(NO_WHOLE_LIBRARY_FLAG "-Wl,--no-whole-archive" CACHE INTERNAL "")
endfunction()

function(set_exported_symbols_file)
    if(BUILD_ENTERPRISE)
        set_target_properties(
            CouchbaseLiteC PROPERTIES LINK_FLAGS
            "-Wl,--version-script=${PROJECT_SOURCE_DIR}/src/exports/generated/CBL_EE.gnu")
    else()
                set_target_properties(
            CouchbaseLiteC PROPERTIES LINK_FLAGS
            "-Wl,--version-script=${PROJECT_SOURCE_DIR}/src/exports/generated/CBL.gnu")
    endif()
endfunction()
