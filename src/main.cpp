#include <memory>

#include "../engine/core/Application.h"
#include "../engine/platform/SDLWindow.h"
#include "../game/Game.h"

int main() {
    Game::GameRoot game;
    auto window = std::make_unique<Engine::SDLWindow>();
    Engine::WindowConfig config{};
    config.title = "Project Zeta (Placeholder Backend)";

    Engine::Application app(game, std::move(window), config);
    if (!app.initialize()) {
        return 1;
    }

    app.run();
    return 0;
}
