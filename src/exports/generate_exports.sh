#! /bin/bash -e
#
# Generates the CBL-C exported symbols list for the Apple, Microsoft and GNU linkers.
# The files are written to the 'generated' subdirectory,
# with extensions '.def' (MS), '.exp' (Apple), '.gnu' (GNU).
# Export files for Enterprise Edition have '_EE' added before the extension.

SCRIPT_DIR=`dirname $0`
cd "$SCRIPT_DIR/generated"

cat ../CBL_Exports.txt ../CBLDefaults_Exports.txt ../Fleece_Exports.txt >exports.txt
cat ../CBL_EE_Exports.txt exports.txt           >exports_ee.txt

cat ../Fleece_Apple_Exports.txt exports.txt     >apple_exports.txt
cat ../Fleece_Apple_Exports.txt exports_ee.txt  >apple_exports_ee.txt

cat ../CBL_Android_Exports.txt exports.txt      >android_exports.txt
cat ../CBL_Android_Exports.txt exports_ee.txt   >android_exports_ee.txt

../format_apple.awk   <apple_exports.txt        >CBL.exp
../format_linux.awk   <exports.txt              >CBL.gnu
../format_linux.awk   <android_exports.txt      >CBL_Android.gnu
../format_windows.awk <exports.txt              >CBL.def

../format_apple.awk   <apple_exports_ee.txt     >CBL_EE.exp
../format_linux.awk   <exports_ee.txt           >CBL_EE.gnu
../format_linux.awk   <android_exports_ee.txt   >CBL_EE_Android.gnu
../format_windows.awk <exports_ee.txt           >CBL_EE.def

rm *exports*.txt
