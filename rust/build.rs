use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let project_root = manifest_dir.parent().unwrap().to_path_buf();
    let build_dir = project_root.join("build");

    // Check if prebuilt library exists
    if !build_dir.join("astrakv.lib").exists() {
        println!("cargo:warning=Building astrakv C++ library (one-time)...");
        let mut cmake_cfg = cmake::Config::new(&project_root);
        cmake_cfg
            .define("ASTRAKV_BUILD_TESTS", "OFF")
            .define("ASTRAKV_BUILD_EXAMPLES", "OFF")
            .profile("Release")
            .generator("Ninja");
        cmake_cfg.build();
    }

    // Link astrakv first
    println!("cargo:rustc-link-search=native={}", build_dir.display());
    println!("cargo:rustc-link-lib=static=astrakv");

    // Collect ALL .lib files from _deps and explicitly link each one
    let deps = build_dir.join("_deps");
    let mut libs: Vec<String> = Vec::new();
    collect_libs(&deps, &mut libs);
    
    // Deduplicate by basename and link each one
    libs.sort();
    libs.dedup();
    for lib in &libs {
        println!("cargo:rustc-link-lib=static={}", lib);
    }

    let target = env::var("TARGET").unwrap();
    if target.contains("windows") || target.contains("msvc") {
        println!("cargo:rustc-link-lib=shlwapi");
        println!("cargo:rustc-link-lib=rpcrt4");
        println!("cargo:rustc-link-lib=ws2_32");
    }
    if !target.contains("msvc") {
        if target.contains("apple") {
            println!("cargo:rustc-link-lib=dylib=c++");
        } else {
            println!("cargo:rustc-link-lib=dylib=stdc++");
        }
    }

    println!("cargo:rerun-if-changed=../include/astrakv/kv.h");

    let bindings = bindgen::Builder::default()
        .header("../include/astrakv/kv.h")
        .allowlist_function("astrakv_.*")
        .allowlist_type("astrakv_.*")
        .clang_arg("-I../include")
        .clang_arg("-I../src")
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}

fn collect_libs(dir: &PathBuf, libs: &mut Vec<String>) {
    if let Ok(entries) = std::fs::read_dir(dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                collect_libs(&path, libs);
            } else if let Some(name) = path.file_stem() {
                let name_str = name.to_string_lossy();
                if path.extension().map_or(false, |e| e == "lib") {
                    // Skip lib-prefixed duplicates (abseil has libabsl_base.lib)
                    let clean = name_str.strip_prefix("lib").unwrap_or(&name_str);
                    if !libs.contains(&clean.to_string()) {
                        libs.push(clean.to_string());
                    }
                    println!("cargo:rustc-link-search=native={}", path.parent().unwrap().display());
                }
            }
        }
    }
}
