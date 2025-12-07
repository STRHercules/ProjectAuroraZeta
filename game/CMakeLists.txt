set(GAME_SOURCES
    Game.cpp
    render/RenderSystem.cpp
    systems/MovementSystem.cpp
    systems/CameraSystem.cpp
    systems/ProjectileSystem.cpp
    systems/CollisionSystem.cpp
    systems/EnemyAISystem.cpp
    systems/WaveSystem.cpp
)

add_library(zeta_game ${GAME_SOURCES})
target_include_directories(zeta_game PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zeta_game PUBLIC engine)

if (MSVC)
    target_compile_options(zeta_game PRIVATE /W4 /permissive-)
else()
    target_compile_options(zeta_game PRIVATE -Wall -Wextra -Wpedantic)
endif()
