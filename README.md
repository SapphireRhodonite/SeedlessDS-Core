# SeedlessDS-Core

SeedlessDS-Core is a Nintendo DS emulation library reconstructed from DraStic r2.6.0.4a. It is built from source and used by the SeedlessDS Android application.

The implementation is C with AArch64 assembly. The Android integration currently targets `arm64-v8a` and produces `librecon_fn.so`. The original DraStic APK and prebuilt library are not build inputs.

## Integration

The public C interface is [include/seedlessds/nds.h](include/seedlessds/nds.h). It exposes instance lifecycle, ROM loading, configuration, frame execution, input, screen buffers, save states, cheats and memory access. Platform callbacks are declared in [include/seedlessds/platform.h](include/seedlessds/platform.h).

The SeedlessDS application owns the Android JNI layer and build configuration. Its CMake project includes [src/CMakeLists.txt](src/CMakeLists.txt), which lists the core modules, and [third_party/CMakeLists.txt](third_party/CMakeLists.txt). This repository is not a standalone Android app or a released Linux frontend.

## Dependencies

Initialize the Lua submodule with `git submodule update --init --recursive`. Lua 5.3.0, UnRAR 5.0.14, LZMA SDK 9.20 and Android's zlib provide the scripting and archive dependencies. UnRAR and LZMA SDK are fetched from pinned sources with SHA-256 verification. See [third_party/LICENSES.md](third_party/LICENSES.md) for attribution, licenses and build locations.

The Android build also links the system logging, OpenSL ES and OpenGL ES libraries. See the SeedlessDS app's build instructions for the SDK, NDK, CMake and JDK versions.

## Attribution

DraStic is the work of its original authors, including Exophase. This library is a reconstruction of that emulator, not an official DraStic release. Games, original BIOS/firmware images and the original DraStic binary are not included. A project-wide license has not yet been selected; dependencies retain their own licenses.
