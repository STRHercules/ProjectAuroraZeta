set(ENGINE_SOURCES
    core/Application.cpp
    core/Logger.cpp
    platform/NullWindow.cpp
    platform/SDLWindow.cpp
    platform/SDLRenderDevice.cpp
    platform/SDLTexture.cpp
    assets/TextureLoader.cpp
    assets/TextureManager.cpp
    assets/AssetManifest.cpp
    assets/FontLoader.cpp
    gameplay/FogOfWar.h
    gameplay/FogOfWar.cpp
    ecs/ComponentStorage.h
    ecs/Entity.h
    ecs/Registry.h
    ecs/components/Transform.h
    ecs/components/Velocity.h
    ecs/components/Renderable.h
    ecs/components/SpriteAnimation.h
    ecs/components/CombatStats.h
    ecs/components/Health.h
    ecs/components/Status.h
    ecs/components/Projectile.h
    ecs/components/AABB.h
    ecs/components/Tags.h
    status/StatusTypes.h
    status/StatusContainer.h
    status/StatusContainer.cpp
    gameplay/Combat.h
    math/Vec2.h
    render/RenderDevice.h
    render/NullRenderDevice.h
    render/Color.h
    render/Camera2D.h
    render/CameraUtil.h
    render/FogOfWarRenderer.h
    render/FogOfWarRenderer.cpp
    render/TextRenderer.h
    render/BitmapFont.h
    render/BitmapTextRenderer.h
    render/BitmapTextRenderer.cpp
    ui/MiniMapHUD.h
    ui/MiniMapHUD.cpp
    gameplay/InputHelpers.h
    input/ActionState.h
    input/ActionMapper.h
    input/ActionMapper.cpp
    input/InputBinding.h
    input/InputLoader.h
    input/InputLoader.cpp
    net/NetAddress.h
    net/NetPacket.h
    net/NetSerializer.h
    net/UdpSocket.h
    net/UdpSocket.cpp
    net/NetTransport.h
    net/NetTransport.cpp
)

add_library(engine ${ENGINE_SOURCES})
target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(SDL2 REQUIRED)
find_package(SDL2_image QUIET)
find_package(SDL2_ttf REQUIRED)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

if (SDL2_image_FOUND)
    target_link_libraries(engine PUBLIC SDL2::SDL2 SDL2_image::SDL2_image SDL2_ttf::SDL2_ttf nlohmann_json::nlohmann_json)
    target_compile_definitions(engine PUBLIC ZETA_HAS_SDL_IMAGE=1)
else()
    target_link_libraries(engine PUBLIC SDL2::SDL2 SDL2_ttf::SDL2_ttf nlohmann_json::nlohmann_json)
endif()

if (WIN32)
    target_link_libraries(engine PUBLIC ws2_32)
endif()

if (MSVC)
    target_compile_options(engine PRIVATE /W4 /permissive-)
else()
    target_compile_options(engine PRIVATE -Wall -Wextra -Wpedantic)
endif()
