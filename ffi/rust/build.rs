fn main() {
    let target_os = std::env::var("CARGO_CFG_TARGET_OS")
        .unwrap_or_default();
    let use_rpath = matches!(target_os.as_str(), "linux" | "macos");

    // Locate the cmake build dir (ffi/rust/../../build/c).
    let manifest = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let build_dir = manifest
        .join("../../build/c")
        .canonicalize()
        .unwrap_or_else(|_| manifest.join("../../build/c"));

    println!("cargo:rustc-link-search=native={}", build_dir.display());
    println!("cargo:rustc-link-lib=static=doppler");
    println!("cargo:rustc-link-lib=dylib=zmq");
    println!("cargo:rustc-link-lib=dylib=fftw3");

    if target_os == "windows" {
        println!("cargo:rustc-link-lib=dylib=fftw3_threads");
        println!("cargo:rustc-link-lib=dylib=stdc++");
    } else {
        println!("cargo:rustc-link-lib=dylib=m");
        if use_rpath {
            println!(
                "cargo:rustc-link-arg=-Wl,-rpath,{}",
                build_dir.display()
            );
        }
    }
}
