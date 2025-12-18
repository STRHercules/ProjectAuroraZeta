find_package(SDL2 REQUIRED)

find_path(SDL2_MIXER_INCLUDE_DIR SDL_mixer.h
    PATH_SUFFIXES include/SDL2 SDL2 include
    PATHS ${CMAKE_PREFIX_PATH})

find_library(SDL2_MIXER_LIBRARY NAMES SDL2_mixer SDL2_mixer.dll
    PATHS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2_mixer DEFAULT_MSG SDL2_MIXER_LIBRARY SDL2_MIXER_INCLUDE_DIR)

if (SDL2_mixer_FOUND)
    set(SDL2_MIXER_LIBRARIES ${SDL2_MIXER_LIBRARY})
    set(SDL2_MIXER_INCLUDE_DIRS ${SDL2_MIXER_INCLUDE_DIR})

    if (NOT TARGET SDL2_mixer::SDL2_mixer)
        add_library(SDL2_mixer::SDL2_mixer UNKNOWN IMPORTED)
        set_target_properties(SDL2_mixer::SDL2_mixer PROPERTIES
            IMPORTED_LOCATION ${SDL2_MIXER_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${SDL2_MIXER_INCLUDE_DIR}
            INTERFACE_LINK_LIBRARIES SDL2::SDL2
        )
    endif()
endif()

mark_as_advanced(SDL2_MIXER_INCLUDE_DIR SDL2_MIXER_LIBRARY)

