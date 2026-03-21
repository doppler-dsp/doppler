fn main() {
    // Try pkg-config first (works after `make install` or with PKG_CONFIG_PATH set).
    // If not available, fall back to the cmake build directory.
    let pkg_ok = std::process::Command::new("pkg-config")
        .args(["--libs", "doppler"])
        .output()
        .map(|o| o.status.success())
        .unwrap_or(false);

    if pkg_ok {
        // Let the linker pick up whatever pkg-config says.
        // pkg-config already emits -L and -l flags; pass them through rustc.
        let output = std::process::Command::new("pkg-config")
            .args(["--libs", "--static", "doppler"])
            .output()
            .expect("pkg-config failed");
        let flags = String::from_utf8(output.stdout).unwrap();
        for flag in flags.split_whitespace() {
            if let Some(path) = flag.strip_prefix("-L") {
                println!("cargo:rustc-link-search=native={path}");
            } else if let Some(lib) = flag.strip_prefix("-l") {
                println!("cargo:rustc-link-lib=dylib={lib}");
            }
        }
        return;
    }

    // Fallback: look for libdoppler.so in the cmake build dir.
    let manifest = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let build_dir = manifest.join("../../build/c"); // ffi/rust → ffi → doppler/build/c
    println!("cargo:rustc-link-search=native={}", build_dir.display());
    println!("cargo:rustc-link-lib=dylib=doppler");
    println!("cargo:rustc-link-lib=dylib=zmq");
    println!("cargo:rustc-link-lib=dylib=fftw3");
    println!("cargo:rustc-link-lib=dylib=m");
}
