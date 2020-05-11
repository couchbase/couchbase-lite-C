// Couchbase Lite C API bindings generator
//
// Copyright (c) 2020 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// This script runs during a Cargo build and generates the raw/unsafe Rust bindings, "bindings.rs",
// in an internal build directory, where they are included by `src/c_api.rs`.
//
// References:
// - https://rust-lang.github.io/rust-bindgen/tutorial-3.html
// - https://doc.rust-lang.org/cargo/reference/build-scripts.html

extern crate bindgen;

use std::env;
use std::fs;
use std::path::PathBuf;

//TODO: This is likely not valid for your system, but I don't know how to find it automatically
static DEFAULT_LIBCLANG_PATH : &str = "/usr/local/Cellar/llvm/10.0.0_3/lib";

static INCLUDE_CBL_IN_LIB : bool = false;

fn main() {
    let root_dir = PathBuf::from("../../..");  // The root of the couchbase-lite-c repo
    let cbl_headers = root_dir.join("include/cbl");
    let fleece_headers = root_dir.join("vendor/couchbase-lite-core/vendor/fleece/API");

    // Set LIBCLANG_PATH environment variable if it's not already set:
    if env::var("LIBCLANG_PATH").is_err() {
        env::set_var("LIBCLANG_PATH", DEFAULT_LIBCLANG_PATH);
        println!("cargo:rustc-env=LIBCLANG_PATH={}", DEFAULT_LIBCLANG_PATH);
    }

    // The bindgen::Builder is the main entry point
    // to bindgen, and lets you build up options for
    // the resulting bindings.
    let bindings = bindgen::Builder::default()
        // The input header we would like to generate bindings for.
        .header("wrapper.h")
        // C '#include' search paths:
        .clang_arg("-I".to_owned() + cbl_headers.to_str().unwrap())
        .clang_arg("-I".to_owned() + fleece_headers.to_str().unwrap())
        // Which symbols to generate bindings for:
        .whitelist_type("CBL.*")
        .whitelist_type("FL.*")
        .whitelist_var("k?CBL.*")
        .whitelist_var("k?FL.*")
        .whitelist_function("CBL.*")
        .whitelist_function("_?FL.*")
        // Tell cargo to invalidate the built crate whenever any of the
        // included header files changed.
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        // Finish the builder and generate the bindings.
        .generate()
        // Unwrap the Result and panic on failure.
        .expect("Unable to generate bindings");

    // Write the bindings to the $OUT_DIR/bindings.rs file.
    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_dir.join("bindings.rs"))
        .expect("Couldn't write bindings!");

    // Tell cargo to tell rustc to link the CouchbaseLiteC shared library.
    //TODO: Abort the build now if the library does not exist, and tell the user to run CMake.
    let root = root_dir.to_str().unwrap();

    if INCLUDE_CBL_IN_LIB {
        println!("cargo:rustc-link-search={}/build_cmake", root);
        println!("cargo:rustc-link-search={}/build_cmake/vendor/couchbase-lite-core", root);
        println!("cargo:rustc-link-search={}/build_cmake/vendor/couchbase-lite-core/vendor/BLIP-Cpp", root);
        println!("cargo:rustc-link-search={}/build_cmake/vendor/couchbase-lite-core/vendor/fleece", root);
        println!("cargo:rustc-link-search={}/build_cmake/vendor/couchbase-lite-core/vendor/mbedtls/library", root);
        println!("cargo:rustc-link-search={}/build_cmake/vendor/couchbase-lite-core/vendor/sqlite3-unicodesn", root);

        println!("cargo:rustc-link-lib=static=CouchbaseLiteCStatic");
        println!("cargo:rustc-link-lib=static=FleeceStatic");

        println!("cargo:rustc-link-lib=static=liteCoreStatic");
        println!("cargo:rustc-link-lib=static=liteCoreWebSocket");
        println!("cargo:rustc-link-lib=static=SQLite3_UnicodeSN");
        println!("cargo:rustc-link-lib=static=BLIPStatic");
        println!("cargo:rustc-link-lib=static=mbedtls");
        println!("cargo:rustc-link-lib=static=mbedcrypto");

        println!("cargo:rustc-link-lib=c++");
        println!("cargo:rustc-link-lib=z");

        println!("cargo:rustc-link-lib=framework=CoreFoundation");
        println!("cargo:rustc-link-lib=framework=Foundation");
        println!("cargo:rustc-link-lib=framework=CFNetwork");
        println!("cargo:rustc-link-lib=framework=Security");
        println!("cargo:rustc-link-lib=framework=SystemConfiguration");
    } else {
        // Copy the CBL dylib:
        let src = root_dir.join("build_cmake/libCouchbaseLiteC.dylib");
        let dst = out_dir.join("libCouchbaseLiteC.dylib");
        println!("cargo:rerun-if-changed={}", src.to_str().unwrap());
        fs::copy(src, dst).expect("copy dylib");
        // Tell rustc to link it:
        println!("cargo:rustc-link-search={}", out_dir.to_str().unwrap());
        println!("cargo:rustc-link-lib=dylib=CouchbaseLiteC");
    }

    // Tell cargo to invalidate the built crate whenever the wrapper changes
    println!("cargo:rerun-if-changed=wrapper.h");
}
