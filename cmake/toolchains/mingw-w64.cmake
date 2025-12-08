set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc-posix)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++-posix)
set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)

# Prefer the MinGW sysroot and any vendor-provided dependencies.
set(CMAKE_FIND_ROOT_PATH
    "/usr/${TOOLCHAIN_PREFIX}"
    "/${TOOLCHAIN_PREFIX}"
)

set(WIN_SDL_ROOT $ENV{WIN_SDL_ROOT})
if (WIN_SDL_ROOT)
    # SDL prebuilt archives follow the layout <name>/x86_64-w64-mingw32/{bin,include,lib,share}
    list(APPEND CMAKE_PREFIX_PATH
        "${WIN_SDL_ROOT}/SDL2-2.30.9/x86_64-w64-mingw32"
        "${WIN_SDL_ROOT}/SDL2_image-2.8.2/x86_64-w64-mingw32"
        "${WIN_SDL_ROOT}/SDL2_ttf-2.22.0/x86_64-w64-mingw32"
    )

    list(APPEND CMAKE_FIND_ROOT_PATH
        "${WIN_SDL_ROOT}/SDL2-2.30.9/x86_64-w64-mingw32"
        "${WIN_SDL_ROOT}/SDL2_image-2.8.2/x86_64-w64-mingw32"
        "${WIN_SDL_ROOT}/SDL2_ttf-2.22.0/x86_64-w64-mingw32"
    )
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
