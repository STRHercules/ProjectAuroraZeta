#include "Game.h"

#include "../engine/ecs/components/AABB.h"
#include "../engine/ecs/components/Transform.h"
#include "../engine/ecs/components/Health.h"

namespace Game {

void GameRoot::selectBuildingAt(const Engine::Vec2& worldPos) {
    Engine::ECS::Entity found = Engine::ECS::kInvalidEntity;
    registry_.view<Engine::ECS::Transform, Engine::ECS::AABB, Game::Building>(
        [&](Engine::ECS::Entity e, const Engine::ECS::Transform& tf, const Engine::ECS::AABB& box, const Game::Building&) {
            float dx = std::abs(worldPos.x - tf.position.x);
            float dy = std::abs(worldPos.y - tf.position.y);
            if (dx <= box.halfExtents.x && dy <= box.halfExtents.y) {
                found = e;
            }
        });
    selectedBuilding_ = found;
}

void GameRoot::drawSelectedBuildingPanel(int mouseX, int mouseY, bool leftClickEdge) {
    if (!render_ || paused_) return;
    if (selectedBuilding_ == Engine::ECS::kInvalidEntity) return;
    if (!registry_.has<Game::Building>(selectedBuilding_) || !registry_.has<Engine::ECS::Health>(selectedBuilding_)) {
        selectedBuilding_ = Engine::ECS::kInvalidEntity;
        return;
    }
    const auto* b = registry_.get<Game::Building>(selectedBuilding_);
    const auto* hp = registry_.get<Engine::ECS::Health>(selectedBuilding_);
    Engine::Color panel{24, 32, 44, 230};
    float w = 270.0f;
    float h = (b->type == Game::BuildingType::Barracks) ? 170.0f : 120.0f;
    float x = 20.0f;
    float y = static_cast<float>(viewportHeight_) - h - kHudBottomSafeMargin;
    render_->drawFilledRect(Engine::Vec2{x, y}, Engine::Vec2{w, h}, panel);
    std::string title = "Building: ";
    switch (b->type) {
        case Game::BuildingType::House: title += "House"; break;
        case Game::BuildingType::Turret: title += "Turret"; break;
        case Game::BuildingType::Barracks: title += "Barracks"; break;
        case Game::BuildingType::Bunker: title += "Bunker"; break;
    }
    drawTextUnified(title, Engine::Vec2{x + 10.0f, y + 10.0f}, 1.0f, Engine::Color{210, 240, 255, 240});
    std::ostringstream hpLine;
    hpLine << "HP " << static_cast<int>(hp->currentHealth) << "/" << static_cast<int>(hp->maxHealth)
           << " | Shields " << static_cast<int>(hp->currentShields) << "/" << static_cast<int>(hp->maxShields);
    drawTextUnified(hpLine.str(), Engine::Vec2{x + 10.0f, y + 34.0f}, 0.95f, Engine::Color{200, 230, 240, 230});
    std::ostringstream armorLine;
    armorLine << "Armor H/S: " << hp->healthArmor << "/" << hp->shieldArmor;
    drawTextUnified(armorLine.str(), Engine::Vec2{x + 10.0f, y + 54.0f}, 0.9f, Engine::Color{190, 220, 240, 230});

    if (b->type == Game::BuildingType::Barracks) {
        float btnY = y + 82.0f;
        float btnH = 28.0f;
        float btnW = (w - 30.0f) / 3.0f;
        const char* keys[3] = {"light", "heavy", "medic"};
        for (int i = 0; i < 3; ++i) {
            float btnX = x + 10.0f + i * (btnW + 5.0f);
            bool hover = mouseX >= btnX && mouseX <= btnX + btnW && mouseY >= btnY && mouseY <= btnY + btnH;
            Engine::Color bg = hover ? Engine::Color{40, 60, 80, 230} : Engine::Color{32, 46, 64, 210};
            render_->drawFilledRect(Engine::Vec2{btnX, btnY}, Engine::Vec2{btnW, btnH}, bg);
            auto it = miniUnitDefs_.find(keys[i]);
            int cost = (it != miniUnitDefs_.end()) ? it->second.costCopper : 0;
            std::ostringstream label;
            label << keys[i] << " (" << cost << "c)";
            drawTextUnified(label.str(), Engine::Vec2{btnX + 6.0f, btnY + 6.0f}, 0.85f,
                            Engine::Color{220, 240, 255, 240});
            if (hover && leftClickEdge && it != miniUnitDefs_.end()) {
                Engine::Vec2 spawnPos{0.0f, 0.0f};
                if (const auto* tf = registry_.get<Engine::ECS::Transform>(selectedBuilding_)) {
                    spawnPos = tf->position;
                    spawnPos.x += 20.0f * (i - 1);
                    spawnPos.y += 24.0f;
                }
                auto ent = spawnMiniUnit(it->second, spawnPos);
                (void)ent;
            }
        }
    }

    // Click outside panel to deselect
    if (leftClickEdge && !buildingJustSelected_) {
        if (!(mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + h)) {
            selectedBuilding_ = Engine::ECS::kInvalidEntity;
        }
    }
    buildingJustSelected_ = false;
}

}  // namespace Game
