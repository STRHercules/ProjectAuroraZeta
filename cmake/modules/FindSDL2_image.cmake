find_package(SDL2 REQUIRED)

find_path(SDL2_IMAGE_INCLUDE_DIR SDL_image.h
    PATH_SUFFIXES include/SDL2 SDL2 include
    PATHS ${CMAKE_PREFIX_PATH})

find_library(SDL2_IMAGE_LIBRARY NAMES SDL2_image SDL2_image.dll
    PATHS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2_image DEFAULT_MSG SDL2_IMAGE_LIBRARY SDL2_IMAGE_INCLUDE_DIR)

if (SDL2_image_FOUND)
    set(SDL2_IMAGE_LIBRARIES ${SDL2_IMAGE_LIBRARY})
    set(SDL2_IMAGE_INCLUDE_DIRS ${SDL2_IMAGE_INCLUDE_DIR})

    if (NOT TARGET SDL2_image::SDL2_image)
        add_library(SDL2_image::SDL2_image UNKNOWN IMPORTED)
        set_target_properties(SDL2_image::SDL2_image PROPERTIES
            IMPORTED_LOCATION ${SDL2_IMAGE_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${SDL2_IMAGE_INCLUDE_DIR}
            INTERFACE_LINK_LIBRARIES SDL2::SDL2
        )
    endif()
endif()

mark_as_advanced(SDL2_IMAGE_INCLUDE_DIR SDL2_IMAGE_LIBRARY)
