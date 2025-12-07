// Lightweight entity handle.
#pragma once

#include <cstdint>

namespace Engine::ECS {

using Entity = std::uint32_t;
constexpr Entity kInvalidEntity = 0;

}  // namespace Engine::ECS
