fn main() {
    let target_os = std::env::var("CARGO_CFG_TARGET_OS")
        .unwrap_or_default();
    let use_rpath = matches!(target_os.as_str(), "linux" | "macos");

    // Locate the cmake build dir (ffi/rust/../../build).
    let manifest = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let build_dir = manifest
        .join("../../build")
        .canonicalize()
        .unwrap_or_else(|_| manifest.join("../../build"));

    println!("cargo:rustc-link-search=native={}", build_dir.display());
    // pocketfft_cxx is built as a subdir target; cmake puts its archive
    // under build/native/src/fft/ rather than the top-level build dir.
    println!(
        "cargo:rustc-link-search=native={}",
        build_dir.join("native/src/fft").display()
    );

    // Homebrew on macOS does not add its lib dir to the default linker
    // search path. Add both Apple-Silicon (/opt/homebrew) and Intel
    // (/usr/local) prefixes so libzmq and libfftw3 are found regardless
    // of the runner architecture.
    if target_os == "macos" {
        for path in &["/opt/homebrew/lib", "/usr/local/lib"] {
            if std::path::Path::new(path).exists() {
                println!("cargo:rustc-link-search=native={}", path);
            }
        }
    }

    println!("cargo:rustc-link-lib=dylib=zmq");

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
