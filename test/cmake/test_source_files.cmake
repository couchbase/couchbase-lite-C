function(set_test_source_files)
    set(oneValueArgs DIR RESULT)
    cmake_parse_arguments(T "" ${oneValueArgs} "" ${ARGN})

    if(NOT DEFINED T_DIR)
        message(FATAL_ERROR Missing DIR which is the directory for the test project)
    endif()

    if(NOT DEFINED T_RESULT)
        message(FATAL_ERROR Missing RESULT which is the variable for setting the source files)
    endif()

    set(
        ${T_RESULT}
        ${T_DIR}/
        ${T_DIR}/BlobTest.cc
        ${T_DIR}/BlobTest_Cpp.cc
        ${T_DIR}/CBLTest.c
        ${T_DIR}/CBLTest.cc
        ${T_DIR}/CBLTestsMain.cpp
        ${T_DIR}/CollectionTest.cc
        ${T_DIR}/CollectionTest_Cpp.cc
        ${T_DIR}/DatabaseTest.cc
        ${T_DIR}/DatabaseTest_Cpp.cc
        ${T_DIR}/DocumentTest.cc
        ${T_DIR}/DocumentTest_Cpp.cc
        ${T_DIR}/LogTest.cc
        ${T_DIR}/PerfTest.cc
        ${T_DIR}/QueryTest.cc
        ${T_DIR}/QueryTest_Cpp.cc
        ${T_DIR}/ReplicatorCollectionTest.cc
        ${T_DIR}/ReplicatorCollectionTest_Cpp.cc
        ${T_DIR}/ReplicatorEETest.cc
        ${T_DIR}/ReplicatorPropEncTest.cc
        ${T_DIR}/ReplicatorTest.cc
        ${T_DIR}/VectorSearchTest.cc
        ${T_DIR}/VectorSearchTest_Cpp.cc
        ${T_DIR}/LazyVectorIndexTest.cc
        ${T_DIR}/../vendor/couchbase-lite-core/vendor/fleece/Fleece/Support/Backtrace.cc
        ${T_DIR}/../vendor/couchbase-lite-core/vendor/fleece/Fleece/Support/LibC++Debug.cc
        ${T_DIR}/../vendor/couchbase-lite-core/vendor/fleece/Fleece/Support/betterassert.cc
        PARENT_SCOPE
    )
endfunction()
