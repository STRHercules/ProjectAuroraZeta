# Android APK Build Approach

This document outlines a repeatable path to package Project Aurora Zeta as an Android APK using SDL2, the Android NDK, and Gradle.
It assumes familiarity with Android Studio and the command-line SDK tools.

## Prerequisites
- Android Studio (Iguana/2024 or newer) with the **Android SDK 34+** platform and **Android NDK r26+** installed.
- CMake 3.22+ and Ninja (installed via the Android SDK components).
- Java 11+ (the Gradle wrapper in the generated project will use it automatically).
- An Android device or emulator with ARM64; x86_64 emulators also work if you build that ABI.

## Project Layout for Android
Create a new `android/` directory at the repo root that contains a standard SDL2 Android template wired to CMake:
```
android/
├─ SDL2/                     # SDL2 (and SDL2_image/SDL2_ttf) source trees for Android
├─ app/
│  ├─ build.gradle           # Uses externalNativeBuild with CMake
│  ├─ src/main/AndroidManifest.xml
│  ├─ src/main/assets/       # Game data and assets copied from the repo
│  ├─ src/main/java/org/libsdl/app/SDLActivity.java
│  └─ src/main/cpp/CMakeLists.txt
├─ build.gradle
└─ settings.gradle
```
Use the official SDL2 release tarballs and copy the `android-project` template into `android/`, then add `SDL2_image` and `SDL2_ttf` in the same way (`SDL2_mixer` is optional).

## Wiring CMake
The Android module should build the game as a shared library consumed by `SDLActivity`.
A minimal `app/src/main/cpp/CMakeLists.txt` can reuse the repo’s libraries:
```cmake
cmake_minimum_required(VERSION 3.22)
project(zeta_android LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point CMake at the repo sources
set(ZETA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
add_subdirectory(${ZETA_ROOT}/engine ${CMAKE_CURRENT_BINARY_DIR}/engine)
add_subdirectory(${ZETA_ROOT}/game ${CMAKE_CURRENT_BINARY_DIR}/game)

add_library(zeta SHARED ${ZETA_ROOT}/src/main.cpp)
target_include_directories(zeta PRIVATE ${ZETA_ROOT})

# SDL2 targets come from the SDL Android template (SDL2, SDL2_ttf, SDL2_image)
target_link_libraries(zeta PRIVATE zeta_game engine SDL2 SDL2_ttf SDL2_image)
```
In `app/build.gradle`, enable the native build and point it to this CMake file:
```groovy
android {
    defaultConfig {
        applicationId "com.projectaurora.zeta"
        minSdk 24
        targetSdk 34
        externalNativeBuild {
            cmake { abiFilters "arm64-v8a", "x86_64" }
        }
        ndk { abiFilters "arm64-v8a", "x86_64" }
    }
    externalNativeBuild {
        cmake { path "src/main/cpp/CMakeLists.txt" }
    }
    // Keep the screen on and lock to landscape
    buildFeatures { prefab true }
}
```
Ensure `settings.gradle` includes `include(':app')` and that `build.gradle` applies the Android plugin and Kotlin/JVM plugins as needed by the SDL template.

## Entry Point Adjustments
Android expects `SDL_main` to be exported from the shared library instead of a desktop `main` symbol.
Adjust `src/main.cpp` to use the SDL-provided entry signature under `__ANDROID__` while preserving desktop builds (example):
```cpp
#ifdef __ANDROID__
extern "C" int SDL_main(int, char**) {
#else
int main() {
#endif
    Game::GameRoot game;
    auto window = std::make_unique<Engine::SDLWindow>();
    Engine::WindowConfig config{};
    config.title = "Project Aurora Zeta";

    Engine::Application app(game, std::move(window), config);
    if (!app.initialize()) { return 1; }
    app.run();
    return 0;
}
```
The Android SDL template links `SDL_main` automatically; on desktop platforms the existing behavior remains unchanged.

## Asset Packaging
`SDLActivity` loads assets from `app/src/main/assets`.
Copy the repository’s `assets/` and `data/` folders (and any required fonts) into that directory during Gradle builds.
You can automate this with a Gradle task:
```groovy
tasks.register("copyZetaAssets", Copy) {
    from("${project.rootDir}/../assets", "${project.rootDir}/../data")
    into("${projectDir}/src/main/assets")
}
preBuild.dependsOn(copyZetaAssets)
```
Ensure that any runtime save path points to `SDL_AndroidGetInternalStoragePath()` or `SDL_AndroidGetExternalStoragePath()` when running on Android.

## Building and Installing
From the repo root:
```bash
cd android
./gradlew assembleDebug        # builds libzeta.so and packages app-debug.apk
adb install -r app/build/outputs/apk/debug/app-debug.apk
```
For release builds, set up a signing config and run `./gradlew assembleRelease`.
Use `adb logcat | grep SDL` to troubleshoot startup issues on device.

## Notes and Limitations
- Network play requires the `INTERNET` permission in `AndroidManifest.xml`; add `android:usesCleartextTraffic="true"` if testing on a LAN without TLS.
- Keep the activity in landscape by setting `android:screenOrientation="landscape"` in the manifest.
- Performance tuning: disable high-DPI scaling, prefer `arm64-v8a` ABI, and reduce texture resolution if memory is tight.
- If SDL2_image is omitted, guard code with `ZETA_HAS_SDL_IMAGE` (already defined in the CMake logic) to avoid runtime missing-symbol errors.
