fn main() {
    let target_os = std::env::var("CARGO_CFG_TARGET_OS")
        .unwrap_or_default();
    let use_rpath = matches!(target_os.as_str(), "linux" | "macos");

    // Locate the cmake build dir (ffi/rust/../../build). DOPPLER_BUILD_DIR
    // overrides it — e.g. point at an instrumented `build-cov` so the Rust
    // tests link the coverage libdoppler (see `make coverage`).
    println!("cargo:rerun-if-env-changed=DOPPLER_BUILD_DIR");
    let manifest = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let build_dir = std::env::var("DOPPLER_BUILD_DIR")
        .map(std::path::PathBuf::from)
        .unwrap_or_else(|_| manifest.join("../../build"));
    let build_dir = build_dir.canonicalize().unwrap_or(build_dir);

    println!("cargo:rustc-link-search=native={}", build_dir.display());
    // pocketfft_cxx is built as a subdir target; cmake puts its archive
    // under build/native/src/fft/ rather than the top-level build dir.
    println!(
        "cargo:rustc-link-search=native={}",
        build_dir.join("native/src/fft").display()
    );

    // Homebrew on macOS does not add its lib dir to the default linker
    // search path. Add both Apple-Silicon (/opt/homebrew) and Intel
    // (/usr/local) prefixes so libfftw3 is found regardless of the runner
    // architecture.
    if target_os == "macos" {
        for path in &["/opt/homebrew/lib", "/usr/local/lib"] {
            if std::path::Path::new(path).exists() {
                println!("cargo:rustc-link-search=native={}", path);
            }
        }
    }

    if target_os == "windows" {
        // Static link on Windows: avoids pseudo-relocation failures and
        // the DLL runtime dependency. LTO is disabled on MinGW in CMake
        // so the static archive contains plain object files.
        // pocketfft_cxx stays a separate STATIC archive on Windows because
        // doppler_lib_static is linked with OBJECT libs, not the static
        // pocketfft archive, so it needs to come in separately.
        println!("cargo:rustc-link-lib=static=doppler");
        println!("cargo:rustc-link-lib=static=pocketfft_cxx");
        println!("cargo:rustc-link-lib=dylib=stdc++");
    } else {
        // Dynamic link on Linux/macOS: pocketfft symbols are compiled into
        // libdoppler.so via target_link_libraries in CMakeLists.txt.
        // rpath points at the build tree so the test binary finds the .so.
        println!("cargo:rustc-link-lib=dylib=doppler");
        println!("cargo:rustc-link-lib=dylib=m");
        if use_rpath {
            println!(
                "cargo:rustc-link-arg=-Wl,-rpath,{}",
                build_dir.display()
            );
        }
    }
}
