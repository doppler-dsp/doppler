- **Install trees no longer ship an empty `include/doppler/`.** The
    directory holds only a build-time template; `install(DIRECTORY)` skipped
    the file and created the directory anyway. Found by vcpkg's post-build
    lint.
