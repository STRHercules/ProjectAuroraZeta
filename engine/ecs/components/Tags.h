// Marker tags for entity classification.
#pragma once

namespace Engine::ECS {

struct HeroTag {};
struct EnemyTag {};
struct ProjectileTag {};
struct PickupTag {};
struct BossTag {};
struct EliteTag {};
struct SolidTag {};  // World collision (static obstacles, scenery, etc.)

}  // namespace Engine::ECS
