macro(generate_edition)
    if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
        # Script mode, use environment
        set(CBLITE_CE_DIR "${CMAKE_CURRENT_LIST_FILE}/../..")
        if(NOT DEFINED VERSION)
            message(
                FATAL_ERROR 
                "No version information passed (use -DVERSION=X.Y.Z)"
            )
        endif()

        set(CouchbaseLite_C_VERSION ${VERSION})
        if(NOT DEFINED OUTPUT_DIR)
            message(
                FATAL_ERROR 
                "No output directory information passed (use -DOUTPUT_DIR=...)"
            )
        endif()

        if(NOT DEFINED BLD_NUM)
            message(
                FATAL_ERROR 
                "No build number information passed (use -DBLD_NUM=###)"
            )
        endif()
        set(CouchbaseLite_C_BUILD ${BLD_NUM})

        string(REPLACE "." ";" TMP ${CouchbaseLite_C_VERSION})
        list(LENGTH TMP TMP_COUNT)
        if(NOT TMP_COUNT EQUAL 3)
            message(FATAL_ERROR "Invalid version passed (${CouchbaseLite_C_VERSION}), must be format X.Y.Z")
        endif()

        list(GET TMP 0 CouchbaseLite_C_VERSION_MAJOR)
        list(GET TMP 1 CouchbaseLite_C_VERSION_MINOR)
        list(GET TMP 2 CouchbaseLite_C_VERSION_PATCH)
    else()
        set(CBLITE_CE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
        set(OUTPUT_DIR ${PROJECT_BINARY_DIR})
        if(NOT DEFINED CouchbaseLite_C_VERSION OR
            NOT DEFINED CouchbaseLite_C_VERSION_MAJOR OR
            NOT DEFINED CouchbaseLite_C_VERSION_MINOR OR
            NOT DEFINED CouchbaseLite_C_VERSION_PATCH)
            message(FATAL_ERROR "Required variable from parent not defined, aborting...")
        endif()

        if(DEFINED ENV{BLD_NUM})
            set(CouchbaseLite_C_BUILD $ENV{BLD_NUM})
        else()
            message(WARNING "No BLD_NUM set, defaulting to 0...")
            set(CouchbaseLite_C_BUILD 0)
        endif()
    endif()

    math(EXPR CouchbaseLite_C_VERNUM "${CouchbaseLite_C_VERSION_MAJOR} * 1000000 + ${CouchbaseLite_C_VERSION_MINOR} * 1000 + ${CouchbaseLite_C_VERSION_PATCH}")
    string(TIMESTAMP CouchbaseLite_C_SOURCE_ID)

    find_package(Git)
    if(Git_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
            WORKING_DIRECTORY ${CBLITE_CE_DIR} 
            OUTPUT_VARIABLE HASH
            RESULT_VARIABLE SUCCESS
        )

        if(NOT SUCCESS EQUAL 0)
            message(WARNING "Failed to get CE hash of build!")
        else()
            string(SUBSTRING ${HASH} 0 7 HASH)
        endif()
        
        if(BUILD_ENTERPRISE)
            set(EE_PATH ${CBLITE_CE_DIR}/../couchbase-lite-c-ee)
            execute_process(
                COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
                WORKING_DIRECTORY ${EE_PATH} 
                OUTPUT_VARIABLE EE_HASH
                RESULT_VARIABLE SUCCESS
            )

            if(NOT SUCCESS EQUAL 0)
                message(WARNING "Failed to get EE hash of build!")
            else()
                string(SUBSTRING ${EE_HASH} 0 7 EE_HASH)
            endif()

            string(PREPEND HASH "${EE_HASH}+")
        endif()
        string(APPEND CouchbaseLite_C_SOURCE_ID " ${HASH}")
    else()
        string(APPEND CouchbaseLite_C_SOURCE_ID " <unknown commit>")
    endif()
    
    configure_file(
        "${CBLITE_CE_DIR}/include/cbl/CBL_Edition.h.in"
        "${OUTPUT_DIR}/generated_headers/public/cbl/CBL_Edition.h"
    )
    message(STATUS "Wrote ${OUTPUT_DIR}/generated_headers/public/cbl/CBL_Edition.h...")
endmacro()

if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
    generate_edition()
endif()
