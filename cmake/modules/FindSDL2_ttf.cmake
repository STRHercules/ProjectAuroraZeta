find_package(SDL2 REQUIRED)

find_path(SDL2_TTF_INCLUDE_DIR SDL_ttf.h
    PATH_SUFFIXES include/SDL2 SDL2 include
    PATHS ${CMAKE_PREFIX_PATH})

find_library(SDL2_TTF_LIBRARY NAMES SDL2_ttf SDL2_ttf.dll
    PATHS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2_ttf DEFAULT_MSG SDL2_TTF_LIBRARY SDL2_TTF_INCLUDE_DIR)

if (SDL2_ttf_FOUND)
    set(SDL2_TTF_LIBRARIES ${SDL2_TTF_LIBRARY})
    set(SDL2_TTF_INCLUDE_DIRS ${SDL2_TTF_INCLUDE_DIR})

    if (NOT TARGET SDL2_ttf::SDL2_ttf)
        add_library(SDL2_ttf::SDL2_ttf UNKNOWN IMPORTED)
        set_target_properties(SDL2_ttf::SDL2_ttf PROPERTIES
            IMPORTED_LOCATION ${SDL2_TTF_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${SDL2_TTF_INCLUDE_DIR}
            INTERFACE_LINK_LIBRARIES SDL2::SDL2
        )
    endif()
endif()

mark_as_advanced(SDL2_TTF_INCLUDE_DIR SDL2_TTF_LIBRARY)
