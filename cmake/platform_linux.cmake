function(init_vars_linux)
    set(WHOLE_LIBRARY_FLAG "-Wl,--whole-archive" CACHE INTERNAL "")
    set(NO_WHOLE_LIBRARY_FLAG "-Wl,--no-whole-archive" CACHE INTERNAL "")
    set(CBL_CXX_FLAGS "-Wno-psabi" CACHE INTERNAL "")
    if(CBL_CXX_FLAGS)
        set(CBL_CXX_FLAGS "${CBL_CXX_FLAGS} -static-libstdc++" CACHE INTERNAL "")
    endif()
endfunction()

function(set_exported_symbols_file)
    if(BUILD_ENTERPRISE)
        set_target_properties(
            cblite PROPERTIES LINK_FLAGS
            "-Wl,--version-script=${PROJECT_SOURCE_DIR}/src/exports/generated/CBL_EE.gnu")
    else()
                set_target_properties(
            cblite PROPERTIES LINK_FLAGS
            "-Wl,--version-script=${PROJECT_SOURCE_DIR}/src/exports/generated/CBL.gnu")
    endif()
endfunction()
