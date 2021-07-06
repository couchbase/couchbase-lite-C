#!/bin/bash -e

SCRIPT_DIR=`dirname $0`
pushd $SCRIPT_DIR/.. > /dev/null

mkdir -p build_cmake
pushd build_cmake > /dev/null
cmake -DCMAKE_BUILD_TYPE=Debug -DCODE_COVERAGE_ENABLED=ON ..    
core_count=`getconf _NPROCESSORS_ONLN`
make -j `expr $core_count + 1`

pushd test > /dev/null
./CBL_C_Tests -r list
popd > /dev/null

lcov -d CMakeFiles/CouchbaseLiteCStatic.dir -c -o CBL_C_Tests.info
find . -type f -name '*.gcda' -delete

lcov --remove CBL_C_Tests.info '/Applications/*' -o CBL_C_Tests_Filtered.info

mkdir -p coverage_reports
genhtml CBL_C_Tests_Filtered.info -o coverage_reports

if [ "$1" == "--show-results" ]; then
  open coverage_reports/index.html
fi

popd > /dev/null
popd > /dev/null
